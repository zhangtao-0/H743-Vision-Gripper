from pathlib import Path
import json,serial,time,struct,csv,math,re
E=Path(__file__).resolve().parent
r={'mode':'MCU fixed PWM700/1000;1000ms automatic stop;no PID;normal source restored before flash','samples':[],'commands':[],'errors':[]}
p=serial.Serial('COM9',115200,timeout=.03,write_timeout=.5)
raw=bytearray();buf=bytearray();t0=time.monotonic()
def cmd(c,wait=.35):
 p.write((c+'\r\n').encode());time.sleep(wait);d=p.read(p.in_waiting);r['commands'].append({'command':c,'reply_tail':d[-500:].decode('utf8',errors='replace')});return d.decode('utf8',errors='replace')
def capture(duration):
 end=time.monotonic()+duration
 while time.monotonic()<end:
  d=p.read(min(p.in_waiting or 1,4096));buf.extend(d);raw.extend(d)
  while True:
   i=buf.find(b'\0\0\x80\x7f')
   if i<0:break
   q=bytes(buf[max(0,i-32):i]);del buf[:i+4]
   if len(q)!=32:continue
   v=struct.unpack('<8f',q)
   if not all(math.isfinite(x) for x in v) or v[6] not in (0.,1.) or v[7] not in (0.,1.,2.,3.,4.,5.):raise RuntimeError('invalid telemetry')
   r['samples'].append([time.monotonic()-t0,*v])
   if v[7]:raise RuntimeError('firmware fault '+str(v[7]))
   if abs(v[3])>630:raise RuntimeError('host overspeed cutoff')
try:
 cmd('@motor disable');cmd('@quiet');cmd('@motor plot off');cmd('@motor clear');r['initial_state']=cmd('@motor status')
 if not all(s in r['initial_state'] for s in ['ready=1','driver=1','on=0','fault=0']):raise RuntimeError('not ready')
 cmd('@motor rpm 400');p.reset_input_buffer();p.write(b'@motor plot on\r\n');capture(.4)
 if len(r['samples'])<10:raise RuntimeError('missing telemetry')
 r['enable_sent_s']=time.monotonic()-t0;p.write(b'@motor enable\r\n');capture(1.5)
 active=[s for s in r['samples'] if s[7]==1]
 r['active_sample_count']=len(active)
 r['active_span_s']=active[-1][0]-active[0][0] if active else 0
 r['peak_raw_rpm']=max((abs(s[4]) for s in active),default=0)
 r['pwm_range']=[min((s[5] for s in active),default=0),max((s[5] for s in active),default=0)]
 r['mcu_automatic_stop_observed']=bool(active and r['samples'][-1][7]==0)
finally:
 try:
  cmd('@motor disable');cmd('@quiet');cmd('@motor plot off');r['final_state']=cmd('@motor status')
  r['off_confirmed']='on=0' in r['final_state'] and 'pwm=0' in r['final_state']
 except Exception as e:r['errors'].append('cleanup: '+str(e))
 p.close();(E/'固定PWM实测.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');(E/'固定PWM实测.bin').write_bytes(raw)
 with (E/'固定PWM实测.csv').open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.writer(f);w.writerow(['seconds','target','measured','reference','raw','pwm','error','enabled','fault']);w.writerows(r['samples'])
print(json.dumps({k:v for k,v in r.items() if k not in ['samples','commands']},ensure_ascii=False,indent=2))
