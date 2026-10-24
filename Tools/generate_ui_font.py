
"""Generate the small fixed Chinese UI label subset from a local font; no downloads."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
labels=['智能夹爪','参数调节','执行控制','默认参数','运行观测','编辑参数','确认执行','固件默认值','指令帮助','图像查看']
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',16)
chars=sorted(set(''.join(labels)))
lines=['/* Generated 16x16 label subset; regenerate with Tools/generate_ui_font.py. */','#ifndef UI_FONT_H','#define UI_FONT_H','#include <stdint.h>',f'#define UI_GLYPHS {len(chars)}U','static const uint16_t ui_glyph_codes[UI_GLYPHS]={'+','.join(hex(ord(c)) for c in chars)+'};','static const uint8_t ui_glyph_bits[UI_GLYPHS][32]={']
for c in chars:
 im=Image.new('1',(16,16));draw=ImageDraw.Draw(im);bbox=draw.textbbox((0,0),c,font=font);draw.text((-bbox[0],-bbox[1]),c,font=font,fill=1)
 data=[sum((1<<bit) if im.getpixel((x+bit,y)) else 0 for bit in range(8)) for y in range(16) for x in [0,8]]
 lines.append('{'+','.join('0x%02X'%x for x in data)+'}, /* U+%04X */'%ord(c))
lines+=['};','static const uint16_t ui_labels[10][8]={']
for label in labels:lines.append('{'+','.join(hex(ord(c)) for c in label)+',0},')
lines+=['};','#endif']
(ROOT/'project/App/modules/ui/ui_font.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('glyphs',len(chars))
