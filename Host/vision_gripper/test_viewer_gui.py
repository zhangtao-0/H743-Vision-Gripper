"""Hidden Tk + fake serial integration test; no real COM port, camera or ML weights."""
import sys,time
import tkinter as tk
import serial
from wire import Frame,encode
import viewer
writes=[]
class FakePort:
    def __init__(self,*args,**kwargs):self.data=b"";self.active=False;self.tick=0
    def __enter__(self):return self
    def __exit__(self,*args):pass
    @property
    def in_waiting(self):return min(len(self.data),8192)
    def write(self,data):
        writes.append(data)
        if data==b"@video on\r\n":self.active=True
        if data==b"@video off\r\n":self.active=False
        return len(data)
    def read(self,n):
        time.sleep(.003)
        if not self.data and self.active:
            self.tick+=1;self.data=encode(Frame(self.tick,self.tick*500,240,240,0,b"\x00\xf8"*57600))
        d,self.data=self.data[:n],self.data[n:];return d
serial.Serial=FakePort
original=tk.Tk
state={}
def factory():
    root=original();root.withdraw();state["root"]=root
    proto=root.protocol
    def protocol(name,callback):
        def done():
            labels=[x for x in root.winfo_children() if isinstance(x,tk.Label) and x.cget("image")]
            assert labels,"Frame never reached the GUI"
            assert b"@video on\r\n" in writes
            assert all(b"claw" not in x and b"motor" not in x and b"pwm" not in x for x in writes)
            callback()
            print("PASS hidden GUI: fake USB decoded, displayed, shutdown; no motion commands")
        root.after(1600,done)
        return proto(name,callback)
    root.protocol=protocol
    return root
tk.Tk=factory
sys.argv=["viewer.py","--port","SIMULATED-NOT-COM"]
viewer.main()
