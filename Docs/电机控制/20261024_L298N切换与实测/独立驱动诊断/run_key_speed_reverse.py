from pathlib import Path
import serial,time,struct,json,math,csv,statistics
E=Path(__file__).resolve().parent
r={'samples':[],'commands':[],'errors':[]};raw=bytearray();buf=bytearray();phase='setup';t0=time.monotonic()
p=serial.Serial('COM9',115200,timeout=.03,write_timeout=.5)
def command(c,reply=True):
 p.write((c+'\r\n').encode())
 if reply:
  time.sleep(.35);d=p.read(p.in_waiting);r['commands'].append([c,d[-400:].decode('utf8',errors='replace')]);return d.decode('utf8',errors='replace')
 time.sleep(.03);r['commands'].append([c,'plot exclusive'])
def capture(seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  d=p.read(min(p.in_waiting or 1,4096));buf.extend(d);raw.extend(d)
  while True:
   i=buf.find(b'\0\0\x80\x7f')
   if i<0:break
   q=bytes(buf[max(0,i-32):i]);del buf[:i+4]
   if len(q)!=32:continue
   v=struct.unpack('<8f',q)
   if not all(math.isfinite(x) for x in v) or v[6] not in (0.,1.):raise RuntimeError('bad stream')
   r['samples'].append({'phase':phase,'time':time.monotonic()-t0,'values':list(v)})
   if v[7]!=0:raise RuntimeError('fault '+str(v[7]))
   if abs(v[3])>600:raise RuntimeError('overspeed')
def check(expected):
 s=[v['values'] for v in r['samples'] if v['phase']==phase][-20:]
 mean=statistics.mean(v[1] for v in s) if s else 0
 r.setdefault('phase_results',{})[phase]={'late_mean_rpm':mean,'expected':expected,'late_enabled':s[-1][6] if s else None}
 if expected and (abs(mean-expected)>max(15,abs(expected)*.1) or s[-1][6]!=1):raise RuntimeError('tracking failed '+phase)
 if not expected and (abs(mean)>5 or s[-1][6]!=0):raise RuntimeError('stop failed')
try:
 command('@motor disable');command('@quiet');command('@motor plot off');r['initial']=command('@motor status')
 if not all(x in r['initial'] for x in ['on=0','fault=0','ready=1']):raise RuntimeError('not ready')
 command('@debug sts off');command('@input mode mixed');command('@debug motor on');command('@motor rpm 200')
 p.reset_input_buffer();command('@motor plot on',False);capture(.4)
 phase='KEY5_start_200';command('@key 5 tap',False);capture(3);check(200)
 phase='knob_plus_205';command('@knob 1',False);capture(1.5);check(205)
 phase='reverse_minus205';command('@motor reverse',False);capture(3);check(-205)
 phase='KEY5_stop';command('@key 5 tap',False);capture(1.2);check(0)
except Exception as e:r['errors'].append(str(e))
finally:
 command('@motor disable',False);time.sleep(.4);command('@quiet',False);time.sleep(.4);command('@motor plot off');command('@debug motor off');command('@motor rpm 400');r['final']=command('@motor status');p.close()
 (E/'启停调速换向实测.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');(E/'启停调速换向实测.bin').write_bytes(raw)
 with (E/'启停调速换向实测.csv').open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.writer(f);w.writerow(['phase','time','target','actual','ramp','raw','pwm','error','enabled','fault'])
  for s in r['samples']:w.writerow([s['phase'],s['time'],*s['values']])
 print(json.dumps({k:v for k,v in r.items() if k not in ['samples','commands']},ensure_ascii=False,indent=2))
