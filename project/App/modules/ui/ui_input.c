#include "ui_input.h"
#include <string.h>
void UiInput_Init(UiInput *s){memset(s,0,sizeof(*s));}
UiEvent UiInput_Sample(UiInput *s,uint32_t now,uint8_t up,uint8_t down,uint8_t ok,uint8_t back){
 uint8_t state[4],i;UiEvent event=UI_NONE;static const UiEvent actions[]={UI_PREV,UI_NEXT,UI_OK,UI_BACK};
 (void)now;state[0]=(up!=0);state[1]=(down!=0);state[2]=(ok!=0);state[3]=(back!=0);
 if(s->chord){if(!state[2]&&!state[3])s->chord=0;memcpy(s->previous,state,4);return UI_NONE;}
 if(state[2]&&state[3]){s->chord=1;memcpy(s->previous,state,4);return UI_SHORTCUT;}
 for(i=0;i<4;i++)if(!state[i]&&s->previous[i])event=actions[i];
 memcpy(s->previous,state,4);return event;
}
