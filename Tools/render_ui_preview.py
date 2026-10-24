"""Render C-model snapshots with the exact firmware ASCII/Chinese bitmap data."""
from pathlib import Path
import json,re
from PIL import Image,ImageDraw,ImageFont
H=Path(__file__).resolve().parents[1];e=H/'Docs/屏幕交互'
s=(H/'project/App/vendor/lcd/lcd_fonts.c').read_text(encoding='utf-8',errors='replace')
s=s.split('ASCII_1608_Table []',1)[1].split('};',1)[0];s=re.sub(r'/\*.*?\*/','',s,flags=re.S)
ascii_data=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s))
f=(H/'project/App/modules/ui/ui_font.h').read_text(encoding='utf-8')
codes=[int(x,16) for x in re.findall(r'0x[0-9a-f]+',f.split('ui_glyph_codes[UI_GLYPHS]=',1)[1].split(';',1)[0])]
bits=bytes(int(x,16) for x in re.findall(r'0x[0-9A-F]{2}',f.split('ui_glyph_bits[UI_GLYPHS][32]=',1)[1].split('};',1)[0]))
labels=[]
for row in f.split('ui_labels[10][8]=',1)[1].split('};',1)[0].splitlines():
 if '{0x' in row:labels.append([int(x,16) for x in re.findall(r'0x[0-9a-f]+',row)])
def glyph(im,x,y,data,w,fg,bg):
 for row in range(16):
  for col in range(w):im.putpixel((x+col,y+row),fg if data[row*(w//8)+col//8]&(1<<(col%8)) else bg)
scenes=json.loads((e/'页面模型快照.json').read_text(encoding='utf-8'))
canvas=Image.new('RGB',(len(scenes)*260,300),'#eeeeee');draw=ImageDraw.Draw(canvas);font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',15)
for num,scene in enumerate(scenes):
 im=Image.new('RGB',(240,240),'black')
 for r in scene['rows']:
  y=r['index']*16;bg=(0,0,128) if r['selected'] else (0,0,0);fg=(0,255,255) if r['index']==0 else (255,255,255)
  ImageDraw.Draw(im).rectangle((0,y,239,y+15),fill=bg)
  for x,c in enumerate(r['text'][:30]):
   if 32<=ord(c)<127:glyph(im,x*8,y,ascii_data[(ord(c)-32)*16:(ord(c)-31)*16],8,fg,bg)
  if r['chinese']:
   for x,code in enumerate(labels[r['chinese']-1]):
    offset=codes.index(code)*32;glyph(im,8+x*16,y,bits[offset:offset+32],16,fg,bg)
 im.save(e/(scene['name']+'.png'));canvas.paste(im,(num*260+10,30));draw.text((num*260+10,8),scene['name'],font=font,fill='black')
canvas.save(e/'本版页面模型预览.png');print(e/'本版页面模型预览.png')
