#include "ui_service.h"
#include "ui_core.h"
#include "ui_input.h"
#include "ui_port.h"
#include "ui_font.h"
#include "integration_config.h"
#include "integration_service.h"
#include "board_camera.h"
#include "lcd_spi_154.h"
#include <string.h>
static UiModel model;
static UiInput input;
static UiView view,previous;
static uint32_t next_frame,last_revision;
static uint8_t initialized,valid[UI_ROWS],cursor,test_mode,rotation;
uint8_t UiService_Rotation(void){return rotation;}
void UiService_SetRotation(uint8_t r){
 static const uint8_t dir[]={Direction_V,Direction_H,Direction_V_Flip,Direction_H_Flip};
 rotation=r&3U;if(!Integration_GetStatus()->lcd_bus_ready)return;BoardCamera_SetPreview(0,32,144);LCD_SetDirection(dir[rotation]);
 memset(valid,0,sizeof(valid));next_frame=0;last_revision=~model.revision;
}
static void Label(uint8_t id,uint16_t y){
 uint8_t n,g;if(!id||id>10)return;
 for(n=0;n<8 && ui_labels[id-1U][n];n++){
  for(g=0;g<UI_GLYPHS;g++)if(ui_labels[id-1U][n]==ui_glyph_codes[g]){
   LCD_DrawImage((uint16_t)(8U+n*16U),y,16,16,ui_glyph_bits[g]);break;
  }
 }
}
void UiService_Init(void){
 UiCore_Init(&model,UiPort_Operations());UiInput_Init(&input);initialized=1;
 memset(valid,0,sizeof(valid));last_revision=~model.revision;next_frame=0;
 BoardCamera_SetPreview(0,32,144);
}
void UiService_Input(uint32_t now,uint8_t up,uint8_t down,uint8_t ok,uint8_t back){
 UiEvent event;
 if(!initialized)return;
 event=UiInput_Sample(&input,now,up,down,ok,back);
 if(event==UI_NONE)return;
 if(test_mode){UiService_LcdTest(0);return;} /* first key only restores the menu */
 UiCore_Event(&model,event);
}
void UiService_LcdTest(uint8_t active){test_mode=(uint8_t)(active!=0);memset(valid,0,sizeof(valid));next_frame=0;}
void UiService_Task(uint32_t now){
 uint8_t n,painted=0,preview;
 if(!initialized||!Integration_GetStatus()->lcd_bus_ready)return;
 if(test_mode){BoardCamera_SetPreview(0,32,144);return;}
 if(last_revision!=model.revision || (int32_t)(now-next_frame)>=0){
  UiCore_View(&model,&view);last_revision=model.revision;next_frame=now+100U;
  preview=(uint8_t)(view.camera && BoardCamera_GetStatus()->ready && BoardCamera_GetStatus()->enabled);
  if(view.camera&&!preview)strcpy(view.row[6],BoardCamera_GetStatus()->ready?"CAMERA STOPPED":"CAMERA UNAVAILABLE");
  BoardCamera_SetPreview(preview,32,144);
 }
 preview=(uint8_t)(view.camera && BoardCamera_GetStatus()->ready && BoardCamera_GetStatus()->enabled);
 LCD_SetAsciiFont(&ASCII_Font16);
 /* At most two changed text rows per cooperative iteration. Never clear a live preview. */
 for(n=0;n<UI_ROWS && painted<2U;n++){
  uint8_t row=cursor;cursor=(uint8_t)((cursor+1U)%UI_ROWS);
  if(preview && row>=2U && row<=10U){valid[row]=0;continue;}
  if(valid[row] && !strcmp(view.row[row],previous.row[row]) && view.chinese[row]==previous.chinese[row] && view.highlight[row]==previous.highlight[row])continue;
  LCD_SetBackColor(view.highlight[row]?DARK_BLUE:LCD_BLACK);LCD_SetColor(view.highlight[row]?DARK_BLUE:LCD_BLACK);
  LCD_FillRect(0,(uint16_t)(row*16U),240,16);LCD_SetColor(row==0?LCD_CYAN:LCD_WHITE);
  LCD_DisplayString(0,(uint16_t)(row*16U),view.row[row]);Label(view.chinese[row],(uint16_t)(row*16U));
  strcpy(previous.row[row],view.row[row]);previous.chinese[row]=view.chinese[row];previous.highlight[row]=view.highlight[row];valid[row]=1;painted++;
 }
}
