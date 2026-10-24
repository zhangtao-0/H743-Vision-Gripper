"""Single HOST UART actual test, no USB CDC required. Explicit --run only."""
from pathlib import Path
import serial,time,json,struct,math,csv,argparse
E=Path(__file__).resolve().parent;TAIL=b"\0\0\x80\x7f"
def main():
 p=argparse.ArgumentParser();p.add_argument('--run',action='store_true');a=p.parse_args()
 if not a.run:p.error('explicit --run required after wiring confirmation')
 report=dict(driver='L298N',firmware='L298N profile1, PA8/PE11 Hall powered5V, GPIO_NOPULL, feedback-progress guard; flashed and programmer verified',wiring='User confirmed ENA jumper removed/PB14, 5V-EN retained; VM reported10.3V',commands=[],samples=[],errors=[],motion_commands_sent=False,phase='initial')
 raw=bytearray();pending=bytearray();started=time.monotonic()
 with serial.Serial('COM9',115200,timeout=.02,write_timeout=.4) as port:
  def command(text,reply=True):
   port.write((text+'\r\n').encode('ascii'));time.sleep(.1 if reply else .02)
   data=port.read(port.in_waiting) if reply and port.in_waiting else b''
   report['commands'].append(dict(command=text,reply=data.decode('utf8',errors='replace') if reply else None));print(text,(repr(data[-240:]) if reply else '(plot exclusive)'),flush=True);return data.decode('utf8',errors='replace')
  def capture(seconds,stationary=False):
   end=time.monotonic()+seconds
   while time.monotonic()<end:
    chunk=port.read(min(port.in_waiting or 1,4096));raw.extend(chunk);pending.extend(chunk)
    while True:
     i=pending.find(TAIL)
     if i<0:break
     packet=bytes(pending[max(0,i-32):i]);del pending[:i+4]
     if len(packet)!=32:continue
     v=struct.unpack('<8f',packet)
     if not all(math.isfinite(x) for x in v) or v[6] not in (0.,1.) or v[7] not in (0.,1.,2.,3.,4.,5.):raise RuntimeError('bad telemetry')
     report['samples'].append(dict(t_s=time.monotonic()-started,phase=report['phase'],channels=list(v)))
     if v[7]!=0:raise RuntimeError('motor fault '+str(v[7]))
     if max(abs(v[1]),abs(v[3]))>600:raise RuntimeError('overspeed guard')
     if stationary and (abs(v[1])>10 or v[6]!=0):raise RuntimeError('not stationary/disabled')
     if not stationary and v[2]>15 and v[1]<-15:raise RuntimeError('feedback sign disagrees')
    if len(pending)>4096:raise RuntimeError('stream desynchronized')
  try:
   command('@motor disable');command('@quiet');report['before_clear']=command('@motor status');command('@motor clear');status=command('@motor status');report['initial_state']=status
   if not all(x in status for x in ['ready=1','driver=1','on=0','fault=0']):raise RuntimeError('not ready/disabled/fault-free')
   command('@param get 22');command('@param get 23');command('@param get 24');command('@motor rpm 400');port.reset_input_buffer();command('@motor plot on',reply=False)
   report['phase']='disabled_baseline';capture(.5,stationary=True)
   if len(report['samples'])<10:raise RuntimeError('insufficient baseline frames')
   report['motion_commands_sent']=True;report['phase']='run_400';command('@motor enable',reply=False);capture(3.5)
   measured=[s['channels'][1] for s in report['samples'] if s['phase']=='run_400'][-20:]
   report['late_mean_rpm']=sum(measured)/len(measured) if measured else 0
   if not 320<=report['late_mean_rpm']<=480:raise RuntimeError('400RPM tracking not achieved')
   report['phase']='disable';command('@motor disable',reply=False);capture(.6)
  except Exception as error:report['errors'].append(str(error));print('STOP',str(error),flush=True)
  finally:
   try:
    command('@motor disable',reply=False);time.sleep(.4);command('@quiet',reply=False);time.sleep(.5);command('@motor plot off');time.sleep(.2)
    report['cleanup_buffer_hex']=port.read(port.in_waiting).hex();command('@motor rpm 400');time.sleep(.3);report['final_state']=command('@motor status')
    if 'on=0' not in report['final_state'] or 'pwm=0' not in report['final_state']:report['errors'].append('Final disable not confirmed')
   except Exception as error:report['errors'].append('cleanup '+str(error))
   (E/'编码器5V实际运行.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');(E/'编码器5V实际原始波形.bin').write_bytes(raw)
   with (E/'编码器5V实际波形.csv').open('w',encoding='utf-8-sig',newline='') as f:
    w=csv.writer(f);w.writerow(['t_s','phase','target_rpm','measured_rpm','reference_rpm','raw_rpm','pwm_permille','error_rpm','enabled','fault'])
    for s in report['samples']:w.writerow([s['t_s'],s['phase'],*s['channels']])
 return int(bool(report['errors']))
if __name__=='__main__':raise SystemExit(main())
