"""Validate polygons; split by capture SESSION to prevent adjacent-frame leakage."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import shutil

CLASSES = ["carton", "black_parcel"]

def validate_label(text):
    classes = set()
    for line in text.splitlines():
        f = line.split()
        if not f:
            continue
        if f[0] not in ("0", "1") or len(f)<7 or len(f)%2!=1:
            raise ValueError("Expected class 0/1 and >=3 polygon (x,y) vertices")
        coords = list(map(float, f[1:]))
        if not all(math.isfinite(x) and 0<=x<=1 for x in coords):
            raise ValueError("Polygon coordinates must be finite normalized values in [0,1]")
        points=list(zip(coords[::2],coords[1::2]))
        area=abs(sum(x*points[(i+1)%len(points)][1]-y*points[(i+1)%len(points)][0] for i,(x,y) in enumerate(points)))/2
        if area < 4/(240*240):
            raise ValueError("Degenerate polygon")
        classes.add(int(f[0]))
    return classes

def prepare(raw, labels, output, seed=42):
    from PIL import Image
    if output.exists():
        raise ValueError("Output exists; choose a new dataset version (no overwrite)")
    images=sorted(raw.rglob("*.png"));sessions={};records=[]
    if not images:
        raise ValueError("No images")
    for image in images:
        rel=image.relative_to(raw)
        if len(rel.parts)!=2:
            raise ValueError("Use raw/session_name/image.png layout")
        label=labels/rel.with_suffix(".txt")
        if not label.is_file():
            raise ValueError(f"Unlabeled image: {image}; empty txt means explicit negative")
        classes=validate_label(label.read_text(encoding="utf-8"))
        with Image.open(image) as im:
            if im.size!=(240,240):raise ValueError("Use actual 240x240 raw captures")
        sessions.setdefault(rel.parts[0],set()).update(classes)
        records.append((image,label,rel,classes))
    names=sorted(sessions)
    if len(names)<5:
        raise ValueError("At least 5 separate capture sessions are needed for train/val/test")
    random.Random(seed).shuffle(names)
    n=max(1,len(names)//5)
    groups={"test":names[:n],"val":names[n:2*n],"train":names[2*n:]}
    for split,group in groups.items():
        if set().union(*(sessions[name] for name in group)) != {0,1}:
            raise ValueError(f"{split} lacks carton or black_parcel; capture both classes per session")
    memberships={name:split for split,group in groups.items() for name in group}
    hashes={}
    for image,_,rel,_ in records:
        sha=hashlib.sha256(image.read_bytes()).hexdigest();split=memberships[rel.parts[0]]
        if sha in hashes and hashes[sha]!=split:
            raise ValueError("Identical images cross splits; remove duplicate captures")
        hashes[sha]=split
    output.mkdir(parents=True)
    manifest=[]
    for image,label,rel,classes in records:
        split=memberships[rel.parts[0]];name="__".join(rel.parts)
        dest=output/"images"/split/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(image,dest)
        dest=output/"labels"/split/Path(name).with_suffix(".txt");dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(label,dest)
        manifest.append(dict(source=str(rel),split=split,classes=sorted(classes),sha256=hashlib.sha256(image.read_bytes()).hexdigest()))
    config="path: "+json.dumps(output.resolve().as_posix())+"\ntrain: images/train\nval: images/val\ntest: images/test\nnames:\n  0: carton\n  1: black_parcel\n"
    (output/"dataset.yaml").write_text(config,encoding="utf-8")
    (output/"manifest.json").write_text(json.dumps(dict(seed=seed,session_splits=groups,images=manifest),indent=2),encoding="utf-8")
    return groups

def main():
    p=argparse.ArgumentParser(description=__doc__)
    root=Path(__file__).parent/"data"
    p.add_argument("--raw",type=Path,default=root/"raw");p.add_argument("--labels",type=Path,default=root/"labels")
    p.add_argument("--output",type=Path,default=root/"parcel_v1");p.add_argument("--seed",type=int,default=42)
    a=p.parse_args();print(prepare(a.raw,a.labels,a.output,a.seed))
if __name__=="__main__":main()
