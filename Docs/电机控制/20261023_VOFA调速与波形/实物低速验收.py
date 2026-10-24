"""Board-specific bounded 50RPM test. Requires explicit --run; stops in finally."""
from pathlib import Path
import argparse,time,json,math,struct,csv,re
import serial
E=Path(__file__).resolve().parent
TAIL=b'\0\0\x80\x7f'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',action='store_true');a=p.parse_args()
 if not a.run:p.error('Use --run only after TB6612 wiring is confirmed')
 report=dict(phase='initial',motion_commands_sent=False,commands=[],samples=[],errors=[])
 with serial.Serial('COM11',115200,timeout=.05,write_timeout=.5) as usb,serial.Serial('COM12',115200,timeout=.05,write_timeout=.5) as host:
  def command(text,via='usb',wait=.2):
   (usb if via=='usb' else host).write((text+'\r\n').encode('ascii'));time.sleep(wait)
   reply=usb.read(usb.in_waiting or 1).decode('utf-8',errors='replace')
   report['commands'].append(dict(command=text,via=via,reply=reply));print(text,repr(reply),flush=True);return reply
  pending=bytearray();started=time.monotonic()
  def capture(seconds,check=False):
   until=time.monotonic()+seconds
   while time.monotonic()<until:
    pending.extend(host.read(min(host.in_waiting or 1,4096)))
    while True:
     end=pending.find(TAIL)
     if end<0:break
     packet=bytes(pending[max(0,end-32):end]);del pending[:end+4]
     if len(packet)!=32:continue
     v=struct.unpack('<8f',packet)
     if not all(math.isfinite(x) for x in v):raise RuntimeError('Nonfinite curve payload')
     report['samples'].append(dict(t_s=round(time.monotonic()-started,6),phase=report['phase'],channels=list(v)))
     if check:
      if v[7]!=0:raise RuntimeError('Motor fault '+str(v[7]))
      if abs(v[1])>150:raise RuntimeError('Unexpected speed for bounded 50RPM test')
      if v[2]>5 and v[1]<-15:raise RuntimeError('Encoder sign disagrees with forward reference')
    if len(pending)>4096:del pending[:-36]
  try:
   command('@quiet');status=command('@motor status')
   if not all(s in status for s in ['ready=1','driver=1','on=0','fault=0']):raise RuntimeError('Motor not ready/disabled/fault-free')
   report['initial_factory_state']=status
   command('@knob -1',via='host');report['knob_decrease']=command('@motor status')
   command('@knob 1',via='host');report['knob_restore']=command('@motor status')
   command('@motor rpm 50');host.reset_input_buffer()
   reply=command('@motor plot on')
   if 'accepted=1' not in reply:
    time.sleep(.4);reply=command('@motor plot on')
   if 'accepted=1' not in reply:raise RuntimeError('Plot did not start')
   report['phase']='disabled_baseline';capture(.8)
   baseline=[s['channels'] for s in report['samples'] if s['phase']=='disabled_baseline']
   if not baseline:raise RuntimeError('No JustFloat frames')
   if any(abs(v[1])>10 for v in baseline[-10:]):raise RuntimeError('Motor turning while controller disabled; check external power/routing')
   report['motion_commands_sent']=True;report['phase']='short_press_start'
   command('@key 7 tap',via='host',wait=.25)
   report['enabled_state']=command('@motor status')
   if 'on=1' not in report['enabled_state']:raise RuntimeError('Short-press start not enabled')
   report['phase']='run_50rpm';capture(4,check=True)
   report['phase']='short_press_stop';command('@key 7 tap',via='host',wait=.25)
   report['short_stop_state']=command('@motor status')
   if 'on=0' not in report['short_stop_state']:raise RuntimeError('Short-press stop failed')
   capture(.8)
  except Exception as exc:
   report['errors'].append(str(exc));print('TEST STOP:',exc,flush=True)
  finally:
   for cmd in ['@motor stop','@quiet','@motor rpm 400']:
    try:command(cmd)
    except Exception as exc:report['errors'].append('Cleanup '+cmd+': '+str(exc))
   time.sleep(.5)
   try:report['final_state']=command('@motor status')
   except Exception as exc:report['errors'].append('Final status: '+str(exc))
   (E/'实物低速运行.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
   with (E/'实物低速曲线.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.writer(f);w.writerow(['t_s','phase','requested_rpm','measured_rpm','reference_rpm','raw_rpm','pwm_permille','error_rpm','enabled','fault'])
    for s in report['samples']:w.writerow([s['t_s'],s['phase'],*s['channels']])
 return int(bool(report['errors']))
if __name__=='__main__':raise SystemExit(main())
