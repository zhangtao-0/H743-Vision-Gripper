import unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from synthetic_demo import create_demo_image
from shape_baseline import detect_shapes

class TestSyntheticVision(unittest.TestCase):
    def test_three_shapes(self):
        result = detect_shapes(create_demo_image())
        self.assertEqual(len(result), 3, result)
        self.assertEqual(set(item['type'] for item in result), {
            'cylinder_top_view', 'block_top_view', 'l_bracket_top_view'})
    def test_centers_are_image_pixels(self):
        result = detect_shapes(create_demo_image())
        for obj in result:
            self.assertGreaterEqual(obj['center_px'][0], 0)
            self.assertLess(obj['center_px'][0], 640)
            self.assertGreaterEqual(obj['center_px'][1], 0)
            self.assertLess(obj['center_px'][1], 480)

if __name__ == '__main__':
    unittest.main()
