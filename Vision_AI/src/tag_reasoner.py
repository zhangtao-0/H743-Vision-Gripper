"""Evidence-aware tag composition. Candidate strategy only; no hardware transport."""
from pathlib import Path
import argparse
import json
import math

ROOT = Path(__file__).resolve().parents[1]


def reason(tags, context=None, ontology=None, rules=None):
    ontology = ontology or json.loads((ROOT / "Library/02_分层标签/ontology.json").read_text(encoding="utf8"))
    rules = rules or json.loads((ROOT / "Library/07_抓取规则/rules.json").read_text(encoding="utf8"))
    context = context or {}
    rank = ontology["source_rank"]
    accepted, unknown, excluded, conflicts = {}, [], [], []
    for item in tags:
        item = dict(item)
        tag = item.get("id")
        score = item.get("confidence", 0)
        source = item.get("source", "inferred")
        if not isinstance(tag, str) or ":" not in tag or source not in rank:
            raise ValueError("Every tag needs namespaced id and recognized evidence source")
        if not isinstance(score, (int, float)) or not math.isfinite(score) or not 0 <= score <= 1:
            raise ValueError("Confidence must be finite in [0,1]")
        if item.get("status") == "unknown" or score < rules["minimum_confidence"]:
            excluded.append(item)
            continue
        if tag.split(":", 1)[0] not in ontology["namespaces"]:
            unknown.append(item)
            continue
        # The same property at different parts is not a contradiction.
        key = (tag, item.get("scope", "object"))
        if key not in accepted or (rank[source], score) > (rank[accepted[key]["source"]], accepted[key]["confidence"]):
            accepted[key] = dict(item, source=source)
    for group in ontology["exclusive_groups"]:
        for scope in {key[1] for key in accepted}:
            keys = [key for key in accepted if key[1] == scope and key[0] in group]
            if len(keys) > 1:
                best_rank = max(rank[accepted[key]["source"]] for key in keys)
                winners = [key for key in keys if rank[accepted[key]["source"]] == best_rank]
                if len(winners) > 1:
                    conflicts.append(dict(scope=scope, tags=[key[0] for key in winners]))
                    for key in keys:
                        excluded.append(accepted.pop(key))
                else:
                    for key in keys:
                        if key != winners[0]:
                            excluded.append(accepted.pop(key))
    ids = {key[0] for key in accepted}
    confidence = {tag: max(x["confidence"] for (name, _), x in accepted.items() if name == tag) for tag in ids}
    scores = {strategy: 0.0 for strategy in rules["strategies"]}
    traces, constraints, required, forbidden = [], set(), set(), set()
    for prior in rules["priors"]:
        if set(prior["when"]) <= ids:
            weight = min(confidence[t] for t in prior["when"]) * prior["weight"]
            scores[prior["strategy"]] += weight
            traces.append(dict(kind="additive", tags=prior["when"], strategy=prior["strategy"], score=weight))
    for combo in sorted(rules["combinations"], key=lambda x: x["priority"], reverse=True):
        if set(combo["all"]) <= ids:
            traces.append(dict(kind="combination", rule=combo["id"], priority=combo["priority"]))
            forbidden.update(combo.get("forbid", []))
            constraints.update(combo.get("constraints", []))
            required.update(combo.get("requirements", []))
            if combo.get("strategy"):
                scores[combo["strategy"]] += combo["bonus"] * min(confidence[t] for t in combo["all"])
    hazard = sorted(ids.intersection(rules["hard_reject_tags"]))
    ranked = sorted((dict(strategy=s, score=round(n, 4)) for s, n in scores.items() if n > 0 and s not in forbidden), key=lambda x: (-x["score"], x["strategy"]))
    required.update(rules["execution_requirements"])
    # Plain booleans, inferred labels and model scores cannot claim a measured calibration.
    missing = sorted(key for key in required if not isinstance(context.get(key), dict)
                     or context[key].get("verified") is not True
                     or context[key].get("source") not in ("measured", "human_verified"))
    reasons = (["hard_hazard"] if hazard else []) + (["conflicting_evidence"] if conflicts else [])
    if not ranked:
        reasons.append("no_supported_strategy")
    if len(ranked) > 1 and abs(ranked[0]["score"] - ranked[1]["score"]) < 1e-6:
        reasons.append("ambiguous_strategy_tie")
    if missing:
        reasons.append("missing_verified_execution_context")
    return dict(schema="h743.grasp_candidate.v1", accepted_tags=list(accepted.values()), unknown_tags=unknown,
                excluded_tags=excluded, conflicts=conflicts, hazards=hazard, constraints=sorted(constraints),
                ranked_candidates=ranked, forbidden_strategies=sorted(forbidden),
                selected_strategy=None if hazard or conflicts or not ranked or "ambiguous_strategy_tie" in reasons else ranked[0]["strategy"],
                missing_requirements=missing, decision_trace=traces, reject_reasons=reasons,
                planning_conditions_met=not reasons, grasp_valid=False, mode="DRY_RUN", hardware_commands_sent=False)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    if a.output.exists():
        p.error("Output exists; preserve evidence and choose new output")
    request = json.loads(a.input.read_text(encoding="utf8"))
    result = reason(request["tags"], request.get("context"))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print(result["selected_strategy"], result["reject_reasons"])


if __name__ == "__main__":
    main()
