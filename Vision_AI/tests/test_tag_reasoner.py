from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from tag_reasoner import reason


def tag(name, source="model", confidence=.9, **kwargs):
    return dict(id=name, source=source, confidence=confidence, **kwargs)


class TestReasoner(unittest.TestCase):
    def test_one_semantic_layer_has_fallback_strategy(self):
        result = reason([tag("semantic:cup")])
        self.assertEqual(result["selected_strategy"], "side_wrap")
        self.assertFalse(result["grasp_valid"])
        self.assertTrue(result["missing_requirements"])

    def test_additive_layers(self):
        one = reason([tag("semantic:cup")])
        two = reason([tag("semantic:cup"), tag("shape:cylinder")])
        self.assertGreater(two["ranked_candidates"][0]["score"], one["ranked_candidates"][0]["score"])

    def test_combination_and_hard_veto_have_priority(self):
        tags = [tag("semantic:cup"), tag("part:handle_present", "human_verified")]
        self.assertEqual(reason(tags)["selected_strategy"], "handle_support")
        fragile = reason(tags + [tag("state:fragile")])
        self.assertIn("handle_support", fragile["forbidden_strategies"])
        self.assertEqual(fragile["selected_strategy"], "side_wrap")
        self.assertIn("force_limit_measured", fragile["missing_requirements"])

    def test_hazard_cannot_be_outvoted(self):
        result = reason([tag("semantic:cup"), tag("shape:cylinder"), tag("safety:hot")])
        self.assertIsNone(result["selected_strategy"])
        self.assertIn("hard_hazard", result["reject_reasons"])

    def test_conflict_refusal_and_measured_override(self):
        tags = [tag("semantic:cup"), tag("state:empty"), tag("state:contains_liquid")]
        self.assertTrue(reason(tags)["conflicts"])
        result = reason(tags + [tag("state:contains_liquid", "measured")])
        self.assertFalse(result["conflicts"])
        self.assertIn("keep_upright", result["constraints"])

    def test_unknown_tags_unknown_state_and_low_scores_preserved(self):
        result = reason([tag("custom:xyz"), tag("semantic:cup", confidence=.2), tag("state:fragile", status="unknown")])
        self.assertEqual(len(result["unknown_tags"]), 1)
        self.assertEqual(len(result["excluded_tags"]), 2)
        self.assertIsNone(result["selected_strategy"])

    def test_duplicate_tags_cannot_double_vote(self):
        self.assertEqual(reason([tag("semantic:cup")])["ranked_candidates"], reason([tag("semantic:cup")] * 100)["ranked_candidates"])

    def test_inferred_context_cannot_claim_calibration(self):
        context = {name: dict(verified=True, source="inferred") for name in ("camera_calibrated", "gripper_geometry_verified", "workspace_verified", "feedback_verified", "operator_hardware_authorization")}
        self.assertFalse(reason([tag("semantic:cup")], context)["planning_conditions_met"])
        for value in context.values():
            value["source"] = "human_verified"
        result = reason([tag("semantic:cup")], context)
        self.assertTrue(result["planning_conditions_met"])
        self.assertFalse(result["grasp_valid"])
        self.assertFalse(result["hardware_commands_sent"])

    def test_arbitrary_number_of_layers(self):
        result = reason([tag("semantic:box")] + [tag(f"shape:custom_{i}") for i in range(80)])
        self.assertEqual(len(result["accepted_tags"]), 81)
        self.assertEqual(result["selected_strategy"], "opposed_faces")

    def test_equal_strategy_scores_are_ambiguous(self):
        result = reason([tag("semantic:cup"), tag("semantic:box")])
        self.assertIsNone(result["selected_strategy"])
        self.assertIn("ambiguous_strategy_tie", result["reject_reasons"])

    def test_confidence_must_be_valid(self):
        for value in (-1, 2, float("nan")):
            with self.assertRaises(ValueError):
                reason([tag("semantic:cup", confidence=value)])


class TestPredictionBridge(unittest.TestCase):
    def test_real_model_semantics_do_not_invent_material_mass_or_3d(self):
        from prediction_tags import interpret
        result = interpret(dict(name="cup", confidence=.8), (240, 240))
        self.assertEqual(result["tags"][0]["id"], "semantic:cup")
        self.assertEqual(result["geometry"]["mass"], "unknown")
        self.assertEqual(result["geometry"]["three_dimensional_shape"], "unknown")
        self.assertFalse(result["decision"]["grasp_valid"])

    def test_polygon_geometry_is_explicitly_2d(self):
        from prediction_tags import interpret
        result = interpret(dict(name="bottle", confidence=.9,
            segments=dict(x=[.2, .3, .3, .2], y=[.1, .1, .8, .8])), (240, 240))
        self.assertIn("shape:elongated_2d", [t["id"] for t in result["tags"]])
        self.assertNotIn("shape:cylinder", [t["id"] for t in result["tags"]])

    def test_invalid_normalized_polygon_rejected(self):
        from prediction_tags import interpret
        with self.assertRaises(ValueError):
            interpret(dict(name="cup", confidence=.9, segments=dict(x=[0, 1, 2], y=[0, 1, 0])), (240, 240))


if __name__ == "__main__":
    unittest.main()
