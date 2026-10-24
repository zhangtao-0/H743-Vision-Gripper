"""Small offline polygon labeler: click vertices, Finish object, Save & next."""
import argparse
from pathlib import Path

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--images", type=Path, default=Path(__file__).parent/"data/raw")
    p.add_argument("--labels", type=Path, default=Path(__file__).parent/"data/labels")
    args = p.parse_args()
    images = sorted(args.images.rglob("*.png"))
    if not images:
        p.error("No raw PNG images; capture different sessions first")
    import tkinter as tk
    from PIL import Image, ImageTk
    win = tk.Tk();win.title("Parcel polygons: carton / black_parcel")
    canvas = tk.Canvas(win,width=720,height=720);canvas.pack()
    info = tk.StringVar();tk.Label(win,textvariable=info).pack()
    cls = tk.StringVar(value="carton")
    tk.OptionMenu(win,cls,"carton","black_parcel").pack(side="left")
    state = dict(index=0,points=[],objects=[],size=(240,240))
    def label_path():
        return args.labels/images[state["index"]].relative_to(args.images).with_suffix(".txt")
    def redraw():
        canvas.delete("mark")
        for cid,pts in state["objects"]:
            flat=[v*3 for point in pts for v in point]
            canvas.create_polygon(*flat,outline="lime" if cid==0 else "cyan",fill="",width=2,tags="mark")
        pts=state["points"]
        if len(pts)>1:canvas.create_line(*[v*3 for point in pts for v in point],fill="orange",width=2,tags="mark")
        for x,y in pts:canvas.create_oval(x*3-3,y*3-3,x*3+3,y*3+3,fill="orange",tags="mark")
        info.set(f"{state['index']+1}/{len(images)} {images[state['index']].name}: objects={len(state['objects'])}; click contour; Finish; Save. Negative only for NO parcel.")
    def load():
        image=Image.open(images[state["index"]]).convert("RGB")
        if image.size!=(240,240):raise ValueError("Expected raw 240x240 capture")
        state["size"]=image.size;state["points"]=[];state["objects"]=[]
        path=label_path()
        if path.exists():
            for row in path.read_text().splitlines():
                fields=row.split();cid=int(fields[0]);v=list(map(float,fields[1:]));state["objects"].append((cid,[(v[i]*240,v[i+1]*240) for i in range(0,len(v),2)]))
        photo=ImageTk.PhotoImage(image.resize((720,720)));canvas.delete("all");canvas.create_image(0,0,anchor="nw",image=photo);canvas.image=photo;redraw()
    def point(event):
        state["points"].append((min(239,max(0,event.x/3)),min(239,max(0,event.y/3))));redraw()
    def finish():
        if len(state["points"])<3:return
        state["objects"].append((0 if cls.get()=="carton" else 1,list(state["points"])));state["points"]=[];redraw()
    def save(negative=False):
        if state["points"]:
            info.set("Finish object first; active polygon has not been saved");return
        if negative and state["objects"]:
            info.set("Clear polygons before declaring a negative");return
        if not state["objects"] and not negative:
            info.set("Use Negative explicitly for empty background, or label all parcels");return
        path=label_path();path.parent.mkdir(parents=True,exist_ok=True)
        lines=[]
        for cid,pts in state["objects"]:
            lines.append(str(cid)+" "+" ".join(f"{v/240:.6f}" for pt in pts for v in pt))
        path.write_text("\n".join(lines)+("\n" if lines else ""),encoding="utf-8")
        if state["index"]+1==len(images):win.destroy();return
        state["index"]+=1;load()
    def clear():state["points"]=[];state["objects"]=[];redraw()
    def undo(_):
        if state["points"]:state["points"].pop()
        redraw()
    canvas.bind("<Button-1>",point);win.bind("<BackSpace>",undo)
    tk.Button(win,text="Finish object",command=finish).pack(side="left")
    tk.Button(win,text="Clear",command=clear).pack(side="left")
    tk.Button(win,text="Save & next",command=save).pack(side="left")
    tk.Button(win,text="Negative & next",command=lambda:save(True)).pack(side="left")
    load();win.mainloop()
if __name__=="__main__":main()
