"""Compare saved proposals with human 2D references; no physical success inference."""
import argparse,csv,json,math
from pathlib import Path

def evaluate(path):
    details=[]
    with path.open(encoding="utf-8-sig",newline="") as f:
        for row in csv.DictReader(f):
            meta=(path.parent/row["metadata"]).resolve()
            data=json.loads(meta.read_text(encoding="utf-8"));p=data.get("proposal")
            expected=row["human_graspable"].strip()=="1"
            accepted=bool(p and p.get("stable"))
            center=angle=None;correct=False
            if expected and accepted:
                center=math.hypot(float(p["u"])-float(row["gt_u"]),float(p["v"])-float(row["gt_v"]))
                angle=abs((float(p["angle"])-float(row["gt_angle"])+90)%180-90)
                correct=center<=float(row["center_tol_px"]) and angle<=float(row["angle_tol_deg"])
            if not expected:correct=not accepted
            details.append(dict(metadata=str(meta),expected=expected,accepted=accepted,center_error_px=center,
                                angle_error_deg=angle,correct_2d=correct))
    if not details:raise ValueError("No reference trials")
    return dict(scope="human-reviewed 2D candidates only; no depth, force or physical grasp proof",trials=len(details),
                correct_2d=sum(x["correct_2d"] for x in details),
                false_accepts=sum(x["accepted"] and not x["expected"] for x in details),
                missed=sum(x["expected"] and not x["accepted"] for x in details),details=details)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("references",type=Path);p.add_argument("--output",type=Path,required=True)
    a=p.parse_args();report=evaluate(a.references);a.output.write_text(json.dumps(report,indent=2),encoding="utf-8");print(json.dumps({k:v for k,v in report.items() if k!="details"},indent=2))
if __name__=="__main__":main()
