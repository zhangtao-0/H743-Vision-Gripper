/* Pure navigation/edit model: no HAL, display, transport or motor dependencies. */
#include "ui_core.h"
#include <stdio.h>
#include <string.h>
static const char *ResultName(UiResult r) {
 static const char *const text[]={"OK","OFFLINE","BUSY","BLOCKED","CHANGED: reopen edit","FAILED","NOT IMPLEMENTED"};
 return (unsigned)r<7U?text[r]:"FAILED";
}
const UiLocation *UiCore_Current(const UiModel *m){return &m->path[m->depth-1U];}
static UiLocation *Current(UiModel *m){return &m->path[m->depth-1U];}
static void Push(UiModel *m,UiPage page){
 if(m->depth>=UI_DEPTH)return;
 memset(&m->path[m->depth],0,sizeof(UiLocation));m->path[m->depth++].page=page;
}
static void Back(UiModel *m){
 if(Current(m)->page==UI_DEBUG)m->has_saved=0;
 if(m->depth>1U)m->depth--;
 m->result=UI_ACCEPTED;
}
void UiCore_Init(UiModel *m,const UiOperations *ops){memset(m,0,sizeof(*m));m->ops=ops;m->depth=1;m->path[0].page=UI_HOME;}
static uint8_t CommandItem(const UiModel *m,const UiLocation *p,uint8_t selected){
 uint8_t i,n=0;
 for(i=0;i<m->ops->command_count;i++)if(!m->ops->command_group||m->ops->command_group(i)==p->tab){if(n++==selected)return i;}
 return 255;
}
static const char *const groups[]={"CLAW / STS","MG310 MOTOR","CAMERA / IMAGE","LOCAL PWM","SYSTEM"};
static const char *const image_items[]={"View live image","Take still (RAM)","Image rotate +90","Image mirror L/R","Image flip U/D","Edge view toggle","LCD rotate +90"};
static uint8_t Count(const UiModel *m,UiPage page){
 switch(page){case UI_HOME:return 7;case UI_DEBUG:return 4;case UI_EXEC_GROUP:return 5;case UI_HELP:return m->ops->help_count;case UI_IMAGE:return 7;case UI_PARAMS:case UI_FACTORY:return m->ops->parameter_count;
 case UI_COMMANDS:{uint8_t i,n=0;for(i=0;i<m->ops->command_count;i++)if(!m->ops->command_group||m->ops->command_group(i)==UiCore_Current(m)->tab)n++;return n;}case UI_DEFAULTS:return (uint8_t)(m->ops->default_count+1U);
 case UI_CONFIRM:return 2;default:return 0;}
}
void UiCore_Event(UiModel *m,UiEvent event){
 UiLocation *p;uint8_t count;UiResult r;UiParameter parameter;
 if(event==UI_NONE)return;
 m->revision++;p=Current(m);m->result=UI_ACCEPTED;
 if(event==UI_SHORTCUT){
  if(p->page==UI_DEBUG){
   if(m->has_saved){memcpy(m->path,m->saved,sizeof(m->path));m->depth=m->saved_depth;m->has_saved=0;}
   else Back(m);
  }else{
   memcpy(m->saved,m->path,sizeof(m->path));m->saved_depth=m->depth;m->has_saved=1;
   memset(m->path,0,sizeof(m->path));m->depth=1;m->path[0].page=UI_HOME;m->path[0].selected=6;Push(m,UI_DEBUG);
  }
  return;
 }
 if(event==UI_BACK){Back(m);return;}
 if(p->page==UI_VIEW){
  if(event==UI_NEXT)m->result=m->ops->command_run(12);
  else if(event==UI_PREV)m->result=m->ops->command_run(13);
  else if(event==UI_OK){m->result=m->ops->command_run(p->tab?19:11);if(m->result==UI_ACCEPTED)p->tab^=1U;}
  return;
 }
 if(p->page==UI_MONITOR){if(event==UI_NEXT||event==UI_OK)p->tab=(uint8_t)((p->tab+1U)%4U);else if(event==UI_PREV)p->tab=(uint8_t)((p->tab+3U)%4U);return;}
 if(p->page==UI_EDIT){
  if(event==UI_NEXT){if(p->draft>p->max-p->step)p->draft=p->max;else p->draft+=p->step;}
  else if(event==UI_PREV){if(p->draft<p->min+p->step)p->draft=p->min;else p->draft-=p->step;}
  else if(event==UI_OK){
   r=m->ops->parameter_read(p->item,&parameter);
   if(r==UI_ACCEPTED && parameter.value!=p->original)r=UI_STALE;
   if(r==UI_ACCEPTED)r=m->ops->parameter_write(p->item,p->draft,p->original);
   if(r==UI_ACCEPTED)Back(m);m->result=r;
  }
  return;
 }
 count=Count(m,p->page);
 if((event==UI_NEXT||event==UI_PREV)&&count){
  p->selected=(uint8_t)((p->selected+(event==UI_NEXT?1U:count-1U))%count);
  if(p->selected<p->top)p->top=p->selected;
  if(p->selected>=p->top+UI_LIST_ROWS)p->top=(uint8_t)(p->selected-UI_LIST_ROWS+1U);
  return;
 }
 if(event!=UI_OK)return;
 if(p->page==UI_HOME){static const UiPage pages[]={UI_PARAMS,UI_EXEC_GROUP,UI_DEFAULTS,UI_MONITOR,UI_HELP,UI_IMAGE,UI_DEBUG};Push(m,pages[p->selected]);}
 else if(p->page==UI_DEBUG){m->result=m->ops->command_run((uint8_t)(20U+p->selected));}
 else if(p->page==UI_EXEC_GROUP){uint8_t group=p->selected;Push(m,UI_COMMANDS);Current(m)->tab=group;}
 else if(p->page==UI_HELP){m->result=m->ops->help_print(p->selected);}
 else if(p->page==UI_IMAGE){
  static const uint8_t action[]={19,11,12,13,14,16,15};
  uint8_t item=p->selected;r=m->ops->command_run(action[item]);m->result=r;
  if(item<2 && r==UI_ACCEPTED){Push(m,UI_VIEW);Current(m)->tab=item;}
 }
 else if(p->page==UI_PARAMS){
  r=m->ops->parameter_read(p->selected,&parameter);
  if(r!=UI_ACCEPTED){m->result=r;return;}
  if(parameter.step<=0){m->result=UI_BLOCKED;return;}
  Push(m,UI_EDIT);Current(m)->item=p->selected;Current(m)->original=parameter.value;
  Current(m)->draft=parameter.value<parameter.min?parameter.min:parameter.value>parameter.max?parameter.max:parameter.value;
  Current(m)->min=parameter.min;Current(m)->max=parameter.max;Current(m)->step=parameter.step;
 }else if(p->page==UI_COMMANDS||p->page==UI_DEFAULTS){
  if(p->page==UI_DEFAULTS && p->selected==0){Push(m,UI_FACTORY);return;}
  Push(m,UI_CONFIRM);Current(m)->kind=(uint8_t)(p->page==UI_DEFAULTS);Current(m)->item=p->page==UI_COMMANDS?CommandItem(m,p,p->selected):(uint8_t)(p->selected-1U);
 }else if(p->page==UI_CONFIRM){
  if(!p->selected){Back(m);return;}
  r=p->kind?m->ops->default_run(p->item):m->ops->command_run(p->item);
  if(r==UI_ACCEPTED)Back(m);else p->selected=0; /* failures never leave Execute armed */
  m->result=r;
 }
}
void UiCore_View(const UiModel *m,UiView *v){
 const UiLocation *p=UiCore_Current(m);uint8_t i,count;UiParameter param;UiResult r;
 memset(v,0,sizeof(*v));
 snprintf(v->row[0],sizeof(v->row[0]),"%29s","H743");
 v->chinese[0]=(uint8_t)(p->page==UI_HELP?9:p->page==UI_IMAGE||p->page==UI_VIEW?10:p->page==UI_EXEC_GROUP?3:p->page==UI_HOME?1:p->page==UI_PARAMS?2:p->page==UI_COMMANDS?3:p->page==UI_DEFAULTS?4:p->page==UI_MONITOR?5:p->page==UI_EDIT?6:p->page==UI_CONFIRM?7:8);
 snprintf(v->row[12],sizeof(v->row[12]),"%s%s",m->has_saved?"HOT: ":"",ResultName(m->result));
 strcpy(v->row[13],"K1:Up K2:Down K3:OK K4:Back");
 snprintf(v->row[14],sizeof(v->row[14]),"K3+4 shortcut:%s",p->page==UI_DEBUG?(m->has_saved?"Return":"Back"):"Debug");
 if(p->page==UI_HOME){
  static const uint8_t labels[]={2,3,4,5,9,10};
  for(i=0;i<6;i++){v->chinese[2U+i]=labels[i];v->highlight[2U+i]=(uint8_t)(i==p->selected);}
  strcpy(v->row[8],"DEBUG / KEY GRANTS");v->highlight[8]=(uint8_t)(p->selected==6);
  strcpy(v->row[10],"Select a function");return;
 }
 if(p->page==UI_DEBUG){
  strcpy(v->row[0],"DEBUG / KEY GRANTS");v->chinese[0]=0;
  for(i=0;i<4;i++){snprintf(v->row[2U+i],sizeof(v->row[2U+i]),"%s",m->ops->command_name((uint8_t)(20U+i)));v->highlight[2U+i]=(uint8_t)(i==p->selected);}
  strcpy(v->row[8],"K5 start/stop K6 -/+ RPM");strcpy(v->row[9],"Grants persist after BACK");return;
 }
 if(p->page==UI_VIEW){v->camera=1;strcpy(v->row[1],"K1:mirror K2:+90 K3:shot");m->ops->status_line(3,0,v->row[11],sizeof(v->row[11]));return;}
 if(p->page==UI_MONITOR){
  snprintf(v->row[1],sizeof(v->row[1]),"%s [%u/4]",p->tab==0?"SYSTEM":p->tab==1?"STS SERVO":p->tab==2?"MG310 MOTOR":"CAMERA",p->tab+1U);
  v->camera=0;
  if(v->camera){m->ops->status_line(3,0,v->row[11],sizeof(v->row[11]));}
  else for(i=0;i<9;i++)m->ops->status_line(p->tab,i,v->row[2U+i],sizeof(v->row[2U+i]));
  return;
 }
 if(p->page==UI_EDIT){
  snprintf(v->row[2],sizeof(v->row[2]),"%s",m->ops->parameter_name(p->item));
  snprintf(v->row[4],sizeof(v->row[4]),"Draft: %ld",(long)p->draft);v->highlight[4]=1;
  snprintf(v->row[6],sizeof(v->row[6]),"Range:%ld..%ld",(long)p->min,(long)p->max);
  snprintf(v->row[8],sizeof(v->row[8]),"Step:%ld  Orig:%ld",(long)p->step,(long)p->original);
  strcpy(v->row[10],"K3:Apply K4:Discard");return;
 }
 if(p->page==UI_CONFIRM){
  snprintf(v->row[2],sizeof(v->row[2]),"%s",p->kind?m->ops->default_name(p->item):m->ops->command_name(p->item));
  strcpy(v->row[4],"CANCEL");strcpy(v->row[6],"EXECUTE");v->highlight[p->selected?6:4]=1;
  strcpy(v->row[9],"Check load / power first");return;
 }
 count=Count(m,p->page);
 for(i=0;i<UI_LIST_ROWS && p->top+i<count;i++){
  uint8_t id=(uint8_t)(p->top+i),row=(uint8_t)(2U+i);
  if(p->page==UI_PARAMS||p->page==UI_FACTORY){
   memset(&param,0,sizeof(param));r=m->ops->parameter_read(id,&param);
   if(r==UI_ACCEPTED || (p->page==UI_FACTORY && r==UI_OFFLINE && param.max>0))snprintf(v->row[row],sizeof(v->row[row]),"%s:%ld",m->ops->parameter_name(id),(long)(p->page==UI_FACTORY?param.factory:param.value));
   else snprintf(v->row[row],sizeof(v->row[row]),"%s:%s",m->ops->parameter_name(id),ResultName(r));
  }else if(p->page==UI_COMMANDS)snprintf(v->row[row],sizeof(v->row[row]),"%s",m->ops->command_name(CommandItem(m,p,id)));
  else if(p->page==UI_EXEC_GROUP)snprintf(v->row[row],sizeof(v->row[row]),"%s",groups[id]);
  else if(p->page==UI_HELP)snprintf(v->row[row],sizeof(v->row[row]),"%s",m->ops->help_name(id));
  else if(p->page==UI_IMAGE)snprintf(v->row[row],sizeof(v->row[row]),"%s",image_items[id]);
  else snprintf(v->row[row],sizeof(v->row[row]),"%s",id?m->ops->default_name((uint8_t)(id-1U)):"View factory values");
  v->highlight[row]=(uint8_t)(id==p->selected);
 }
 snprintf(v->row[10],sizeof(v->row[10]),"Item %u/%u%s",p->selected+1U,count,p->page==UI_FACTORY?" (read only)":"");
}
