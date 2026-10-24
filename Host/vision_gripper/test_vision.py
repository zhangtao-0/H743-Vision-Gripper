import math
import random
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
from grasp import Segmenter
from evaluate_grasps import evaluate
import json
from pathlib import Path
import numpy as np
from PIL import Image
from wire import Decoder, Frame, encode, rgb_image, displayed
from grasp import from_mask, Stability
from prepare_dataset import validate_label, prepare

class WireTests(unittest.TestCase):
    def setUp(self):
        # RGB565 primary colors, little endian memory words.
        self.frame=Frame(42,1000,240,240,0,(b"\x00\xf8\xe0\x07\x1f\x00"*19200))
    def test_fragmentation_text_and_color(self):
        data=b"SYS ready\r\n"+encode(self.frame)+b"reply\r\n"+encode(self.frame)
        rng=random.Random(5);decoder=Decoder();frames=[]
        while data:
            n=rng.randrange(1,500);frames+=decoder.feed(data[:n]);data=data[n:]
        self.assertEqual(len(frames),2)
        image=rgb_image(frames[0]);self.assertEqual(image.getpixel((0,0)),(255,0,0))
        self.assertEqual(image.getpixel((1,0)),(0,255,0));self.assertEqual(image.getpixel((2,0)),(0,0,255))
    def test_crc_corrupt_and_truncated_resync(self):
        bad=bytearray(encode(self.frame));bad[400]^=1
        good=encode(self.frame)
        decoder=Decoder();found=decoder.feed(bad+good[:1500]+good)
        self.assertEqual(len(found),1);self.assertGreaterEqual(decoder.bad_crc,2)
    def test_header_crc_bound_and_memory(self):
        bad=bytearray(encode(self.frame));bad[24:28]=b"\xff"*4
        decoder=Decoder();found=decoder.feed(bad[:40]+encode(self.frame));self.assertEqual(len(found),1)
        decoder.feed(b"x"*1000000);self.assertLessEqual(len(decoder.buffer),7)
    def test_transforms(self):
        im=Image.new("RGB",(2,2));im.putdata([(255,0,0),(0,255,0),(0,0,255),(255,255,255)])
        self.assertEqual(displayed(im,1).getpixel((1,0)),(255,0,0))
        self.assertEqual(displayed(im,4).getpixel((1,0)),(255,0,0))

class GraspTests(unittest.TestCase):
    def mask(self):
        a=np.zeros((240,240),np.uint8);a[85:155,45:195]=1;return a
    def test_rectangle_and_obstacle(self):
        p=from_mask(self.mask(),"carton",.9)
        self.assertAlmostEqual(p.u,119.5,delta=1);self.assertAlmostEqual(p.v,119.5,delta=1)
        self.assertAlmostEqual(abs(p.angle),90,delta=1);self.assertGreater(p.width,70)
        obs=np.zeros((240,240),np.uint8);obs[80:85,:]=1
        self.assertIsNone(from_mask(self.mask(),"carton",.9,obs))
    def test_no_mask_cropped_or_ambiguous(self):
        self.assertIsNone(from_mask(np.zeros((240,240)),"carton",.9))
        self.assertIsNone(from_mask(self.mask(),"carton",float("nan")))
        a=self.mask();a[:100,0:60]=1;self.assertIsNone(from_mask(a,"carton",.9))
        a=np.zeros((240,240));a[80:160,80:160]=1;self.assertIsNone(from_mask(a,"carton",.9))
    def test_stability_stale_and_motion(self):
        s=Stability(4)
        for i in range(3):self.assertFalse(s.update(from_mask(self.mask(),"carton",.9),i*.5).stable)
        self.assertTrue(s.update(from_mask(self.mask(),"carton",.9),1.5).stable)
        self.assertFalse(s.update(from_mask(self.mask(),"carton",.9),4).stable)
        s.reset()
        for i in range(4):
            p=from_mask(self.mask(),"carton",.9);p.u+=i*4;p=s.update(p,5+i*.5)
        self.assertFalse(p.stable)  # adjacent motion small but full-window drift too large
        self.assertIsNone(s.update(None,8))

class DatasetTests(unittest.TestCase):
    def test_bad_labels(self):
        for text in ["0 .1 .2 .3 .4", "2 .1 .1 .2 .1 .1 .2", "0 nan .1 .2 .1 .1 .2", "0 .1 .1 .1 .1 .1 .1"]:
            with self.assertRaises(ValueError):validate_label(text)
        self.assertEqual(validate_label(""),set())
    def test_session_split_and_duplicate_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);raw=root/"raw";labels=root/"labels"
            row="0 .1 .1 .5 .1 .5 .5\n1 .6 .6 .9 .6 .9 .9\n"
            for i in range(5):
                rel=Path(f"s{i}")/"sample.png";(raw/rel).parent.mkdir(parents=True)
                Image.new("RGB",(240,240),(i*20,1,2)).save(raw/rel)
                path=labels/rel.with_suffix(".txt");path.parent.mkdir(parents=True);path.write_text(row)
            groups=prepare(raw,labels,root/"out")
            self.assertEqual(len(set(sum(groups.values(),[]))),5)
            self.assertTrue((root/"out/dataset.yaml").is_file())
            (raw/"s1/sample.png").write_bytes((raw/"s0/sample.png").read_bytes())
            # All identical forces a cross-split duplicate regardless of random group assignment.
            for i in range(2,5):(raw/f"s{i}/sample.png").write_bytes((raw/"s0/sample.png").read_bytes())
            with self.assertRaises(ValueError):prepare(raw,labels,root/"out2")


class ModelContractTests(unittest.TestCase):
    def test_segmentation_wrapper_with_mocked_model(self):
        class Tensor:
            def __init__(self,x):self.x=x
            def cpu(self):return self
            def numpy(self):return self.x
        mask=np.zeros((1,240,240));mask[0,85:155,45:195]=1
        result=SimpleNamespace(masks=SimpleNamespace(data=Tensor(mask)),
            names={0:"carton",1:"black_parcel"},boxes=SimpleNamespace(cls=np.array([0]),conf=np.array([.9])))
        calls=[]
        class FakeYolo:
            names={0:"carton",1:"black_parcel"}
            def __init__(self,*a,**k):pass
            def predict(self,**kwargs):calls.append(kwargs);return [result]
        with tempfile.TemporaryDirectory() as folder:
            weights=Path(folder)/"mock.pt";weights.write_bytes(b"MOCK, NOT LEARNED WEIGHTS")
            with patch.dict("sys.modules",{"ultralytics":SimpleNamespace(YOLO=FakeYolo)}):
                model=Segmenter(str(weights));proposal=model.predict(Image.new("RGB",(240,240)))
                self.assertEqual(proposal.label,"carton");self.assertEqual(calls[0]["imgsz"],320)
                with self.assertRaises(ValueError):Segmenter(str(weights),classes=("unknown",))
    def test_manual_reference_evaluation(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/"yes.json").write_text(json.dumps({"proposal":{"stable":True,"u":100,"v":100,"angle":-90}}))
            (root/"no.json").write_text(json.dumps({"proposal":None}))
            csv=root/"refs.csv";csv.write_text("metadata,human_graspable,gt_u,gt_v,gt_angle,center_tol_px,angle_tol_deg\n"
                "yes.json,1,102,100,90,5,5\nno.json,0,,,,,\n")
            report=evaluate(csv);self.assertEqual(report["correct_2d"],2);self.assertEqual(report["false_accepts"],0)

if __name__=="__main__":unittest.main(verbosity=2)
