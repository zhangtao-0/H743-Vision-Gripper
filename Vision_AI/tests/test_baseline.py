import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(ROOT / "integration"))
from baseline import Config, detect, read_image, run, write_image
from import_saved_stream import import_stream

spec = importlib.util.spec_from_file_location("provided_demo", ROOT / "Resources/教学包原件/03_最小可运行演示/src/synthetic_demo.py")
demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(demo)


class TestBaseline(unittest.TestCase):
    def test_three_shapes_and_unexecutable_pixel_output(self):
        mask, objects, rejected = detect(demo.create_demo_image())
        self.assertEqual({o["shape_hint"] for o in objects}, {"cylinder_top_view", "block_top_view", "l_bracket_top_view"})
        self.assertEqual(len(objects), 3)
        self.assertFalse(rejected)
        circle = next(o for o in objects if o["shape_hint"] == "cylinder_top_view")
        np.testing.assert_allclose(circle["center_px"], (148, 138), atol=1)
        self.assertTrue(circle["orientation_ambiguous"])
        for obj in objects:
            self.assertFalse(obj["grasp_valid"])
            x, y = obj["interior_point_px"]
            self.assertEqual(mask[y, x], 255)

    def test_nonconvex_centroid_is_not_contact_point(self):
        _, objects, _ = detect(demo.create_demo_image())
        bracket = next(o for o in objects if o["shape_hint"] == "l_bracket_top_view")
        self.assertFalse(bracket["center_inside"])
        self.assertNotEqual(bracket["center_px"], bracket["interior_point_px"])

    def test_background_failure_is_preserved_and_retuning_is_local(self):
        dim = (demo.create_demo_image().astype(float) * .55).astype(np.uint8)
        _, bad, rejected = detect(dim)
        self.assertFalse(bad)
        self.assertTrue(rejected)
        _, recovered, _ = detect(dim, Config(threshold=100))
        self.assertEqual(len(recovered), 3)

    def test_exposure_and_polarity(self):
        _, objects, _ = detect(np.full((240, 240, 3), 250, np.uint8))
        self.assertFalse(objects)
        image = np.full((240, 240, 3), 35, np.uint8)
        image[60:170, 60:180] = 210
        self.assertFalse(detect(image)[1])
        self.assertEqual(len(detect(image, Config(mode="gray_light", threshold=100))[1]), 1)

    def test_hsv_and_invalid_config(self):
        self.assertEqual(len(detect(demo.create_demo_image(), Config(mode="hsv"))[1]), 3)
        for config in (Config(mode="unknown"), Config(threshold=-1), Config(min_area=float("nan")), Config(hsv_high=(200, 255, 255))):
            with self.assertRaises(ValueError):
                detect(demo.create_demo_image(), config)

    def test_unicode_paths_no_overwrite_and_provenance(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / "中文目录"
            image = root / "原图.png"
            write_image(image, demo.create_demo_image())
            before = image.read_bytes()
            self.assertEqual(read_image(image).shape, (480, 640, 3))
            output = root / "实验一"
            result = run(image, output, source_kind="synthetic")
            self.assertEqual(result["coordinate_system"], "Pixel")
            self.assertEqual(result["source_kind"], "synthetic")
            self.assertFalse(result["motion_commands_sent"])
            self.assertEqual(read_image(output / "comparison.png").shape, (480, 1920, 3))
            with self.assertRaises(ValueError):
                run(image, output)
            self.assertEqual(image.read_bytes(), before)

    def test_240_square_camera_frame_scale(self):
        import cv2
        image = cv2.resize(demo.create_demo_image(), (240, 240))
        self.assertEqual(len(detect(image, Config(min_area=100))[1]), 3)


class TestExistingProtocolAdapter(unittest.TestCase):
    def test_crc_rgb565_raw_orientation_no_serial(self):
        spec = importlib.util.spec_from_file_location("existing_wire_for_test", ROOT.parent / "Host/vision_gripper/wire.py")
        wire = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = wire
        spec.loader.exec_module(wire)
        frame = wire.Frame(42, 1234, 240, 240, 7, b"\x00\xf8" * (240 * 240))
        valid = wire.encode(frame)
        corrupt = bytearray(valid)
        corrupt[-1] ^= 1
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            source = folder / "record.bin"
            source.write_bytes(b"text outside frame\r\n" + corrupt + valid)
            report = import_stream(source, folder / "imported", "synthetic")
            self.assertEqual(report["frames"], 1)
            self.assertGreater(report["bad_crc"], 0)
            self.assertFalse(report["serial_opened"])
            self.assertFalse(report["commands_sent"])
            image = read_image(next((folder / "imported").glob("*.png")))
            self.assertEqual(image[0, 0].tolist(), [0, 0, 255])
            meta = json.loads(next(p for p in (folder / "imported").glob("*.json") if p.name != "import_report.json").read_text())
            self.assertEqual(meta["orientation"], "raw_sensor")
            self.assertEqual(meta["sequence"], 42)
            with self.assertRaises(ValueError):
                import_stream(source, folder / "imported")

    def test_no_valid_frame_is_failure_with_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            source = folder / "bad.bin"
            source.write_bytes(b"not a frame")
            with self.assertRaises(ValueError):
                import_stream(source, folder / "output")
            self.assertTrue((folder / "output/import_report.json").is_file())


if __name__ == "__main__":
    unittest.main()
