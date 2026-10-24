"""Actual board test: explicit --run, bounded stages, stop on fault, always disable."""
from pathlib import Path
import argparse,time,json,struct,math,csv
import serial
E=Path(__file__).resolve().parent
TAIL=b"\x00\x00\x80\x7f"
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--run',action='store_true');a=parser.parse_args()
 if not a.run:parser.error('explicit --run required')
 report=dict(vm_user_confirmed=10.3,firmware='motor-vofa-v1 (@debug unsupported)',commands=[],samples=[],errors=[],motion_commands_sent=False,phase='initial')
 raw=bytearray();pending=bytearray();started=time.monotonic()
 with serial.Serial('COM11',115200,timeout=.02,write_timeout=.3) as usb,serial.Serial('COM9',115200,timeout=.02,write_timeout=.3) as host:
  def command(text,wait=.05):
   usb.write((text+'\r\n').encode('ascii'));time.sleep(wait)
   reply=usb.read(usb.in_waiting or 1).decode('utf8',errors='replace')
   report['commands'].append(dict(command=text,reply=reply,t_s=time.monotonic()-started));print(text,repr(reply),flush=True);return reply
  def capture(seconds,limit=600,check=True):
   end=time.monotonic()+seconds
   while time.monotonic()<end:
    data=host.read(min(host.in_waiting or 1,4096));raw.extend(data);pending.extend(data)
    while True:
     i=pending.find(TAIL)
     if i<0:break
     packet=bytes(pending[max(0,i-32):i]);del pending[:i+4]
     if len(packet)!=32:continue
     values=struct.unpack('<8f',packet)
     if not all(math.isfinite(v) for v in values):raise RuntimeError('nonfinite telemetry')
     if values[6] not in (0.,1.) or values[7] not in (0.,1.,2.,3.,4.,5.):raise RuntimeError('invalid telemetry framing')
     report['samples'].append(dict(t_s=time.monotonic()-started,phase=report['phase'],channels=list(values)))
     if check and values[7]!=0:raise RuntimeError('motor fault '+str(values[7]))
     if max(abs(values[1]),abs(values[3]))>limit:raise RuntimeError('speed guard')
     if check and values[2]>15 and values[1]<-15:raise RuntimeError('feedback sign disagrees')
    if len(pending)>4096:raise RuntimeError('telemetry stream desynchronized')
  try:
   command('@motor disable');command('@quiet');status=command('@motor status',.12);report['before_explicit_clear']=status;command('@motor clear');status=command('@motor status',.12);report['initial_state']=status;report['repair_note']='User reports reconnected A/B; requested direct400RPM'
   if not all(s in status for s in ['ready=1','driver=1','on=0','fault=0']):raise RuntimeError('initial state not disabled/fault-free')
   command('@motor rpm 400');host.reset_input_buffer();reply=command('@motor plot on',.1)
   if 'accepted=1' not in reply:raise RuntimeError('plot start refused')
   report['phase']='disabled_baseline';capture(.5,limit=30)
   if len(report['samples'])<10:raise RuntimeError('insufficient baseline telemetry')
   if any(abs(s['channels'][1])>5 or s['channels'][6]!=0 for s in report['samples'][-10:]):raise RuntimeError('not stationary before enable')
   report['phase']='run_400_after_AB_repair';report['motion_commands_sent']=True;reply=command('@motor enable',.03)
   if 'enable=1' not in reply:raise RuntimeError('enable refused')
   capture(3.5,limit=600)
   report['state_after_400']=command('@motor status',.08)
   report['phase']='disable';command('@motor disable',.03);capture(.7,limit=600)
  except Exception as error:
   report['errors'].append(str(error));print('STOP:',error,flush=True)
  finally:
   for cmd in ['@motor disable','@quiet','@motor rpm 400']:
    try:command(cmd,.08)
    except Exception as error:report['errors'].append('cleanup: '+str(error))
   time.sleep(.3)
   try:report['final_state']=command('@motor status',.12)
   except Exception as error:report['errors'].append('final: '+str(error))
   (E/'400重接AB后运行.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
   (E/'400重接AB后波形原始.bin').write_bytes(raw)
   with (E/'400重接AB后波形.csv').open('w',encoding='utf-8-sig',newline='') as file:
    writer=csv.writer(file);writer.writerow(['t_s','phase','target_rpm','measured_rpm','reference_rpm','raw_rpm','pwm_permille','error_rpm','enabled','fault'])
    for sample in report['samples']:writer.writerow([sample['t_s'],sample['phase'],*sample['channels']])
 return int(bool(report['errors']))
if __name__=='__main__':raise SystemExit(main())
