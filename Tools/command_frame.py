"""Offline frame formatter. No serial/network access or hardware actions."""
import argparse,math,struct

def packet(device,instruction,params):
    if not 0<=device<=254 or not 0<=instruction<=255 or len(params)>249:
        raise ValueError('ID/Inst/length out of range')
    body=bytes([device,len(params)+2,instruction])+bytes(params)
    return b'\xff\xff'+body+bytes([(~sum(body))&255])

def f5(command,value):
    if not 0<=command<=5 or not math.isfinite(value):raise ValueError('Cmd 0..5; finite float required')
    payload=struct.pack('<f',value)
    if not math.isfinite(struct.unpack('<f',payload)[0]):raise ValueError('float32 overflow')
    return bytes.fromhex('FF FF FE 07 F5')+bytes([command])+payload+b'\0'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='kind',required=True)
    num=lambda s:int(s,0)
    p=sub.add_parser('ping');p.add_argument('id',type=num)
    p=sub.add_parser('read');p.add_argument('id',type=num);p.add_argument('address',type=num);p.add_argument('count',type=num)
    p=sub.add_parser('local');p.add_argument('instruction',help='F0..F4 (hex)');p.add_argument('params',nargs='*',help='hex byte list, e.g. 02 04 84 03')
    p=sub.add_parser('f5');p.add_argument('command',type=num);p.add_argument('value',type=float)
    args=parser.parse_args()
    try:
        if args.kind=='ping':frame=packet(args.id,1,[])
        elif args.kind=='read':frame=packet(args.id,2,bytes([args.address,args.count]))
        elif args.kind=='local':
            instruction=int(args.instruction,16)
            if instruction not in range(0xF0,0xF5):raise ValueError('local accepts F0..F4; use f5 for float frames')
            frame=packet(254,instruction,[int(x,16) for x in args.params])
        else:frame=f5(args.command,args.value)
        print(frame.hex(' ').upper())
    except (ValueError,OverflowError,struct.error) as exc:parser.error(str(exc))
if __name__=='__main__':main()
