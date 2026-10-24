#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_core.h"
#include "ui_input.h"
#include "ui_profile.h"
static int writes,actions;static int32_t values[25];static UiResult gate=UI_ACCEPTED;
#include "ui_names_fixture.h"
static const char *Name(uint8_t i){return i<25?params[i]:"?";}
static const char *CommandName(uint8_t i){return i<20?commands[i]:"?";}
static const char *DefaultName(uint8_t i){return i<5?defaults[i]:"?";}
static UiResult Read(uint8_t i,UiParameter *p){p->value=values[i];p->min=i==12?20:0;p->max=400;p->step=5;p->factory=50;return UI_ACCEPTED;}
static UiResult Write(uint8_t i,int32_t v,int32_t expected){if(values[i]!=expected)return UI_STALE;writes++;values[i]=v;return gate;}
static uint8_t last_run;
static UiResult Run(uint8_t id){last_run=id;if(gate==UI_ACCEPTED)actions++;return gate;}
static void Status(uint8_t tab,uint8_t line,char *out,uint32_t size){snprintf(out,size,"TAB%u VALUE%u",tab,line);}
static unsigned helps,last_action;
static uint8_t Group(uint8_t id){if((id>=8&&id<=10)||id==17)return 0;if(id>=3&&id<=6)return 1;if(id==1||id==2||(id>=11&&id<=16)||id==19)return 2;if(id==7)return 3;return 4;}
static const char *HelpName(uint8_t i){return i<9?help_names[i]:"?";}
static UiResult Help(uint8_t id){last_action=id;helps++;return UI_ACCEPTED;}
static const UiOperations ops={25,20,5,Name,Read,Write,CommandName,Run,DefaultName,Run,Status,Group,9,HelpName,Help};
static void Times(UiModel *m,UiEvent e,int n){while(n--)UiCore_Event(m,e);}
static void Dump(const char *name,UiModel *m){UiView v;unsigned i;UiCore_View(m,&v);printf("SCENE|%s\n",name);for(i=0;i<UI_ROWS;i++)printf("ROW|%u|%u|%u|%s\n",i,v.chinese[i],v.highlight[i],v.row[i]);}
int main(void){
 UiModel m,original;UiInput input;UiProfile a,b,c;unsigned i,j;union{float f;uint32_t bits;} cpr;
 for(i=0;i<25;i++)values[i]=100;
 UiCore_Init(&m,&ops);Dump("home",&m);UiCore_Event(&m,UI_OK);Times(&m,UI_NEXT,9);assert(UiCore_Current(&m)->selected==9 && UiCore_Current(&m)->top==4);
 UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_NEXT);original=m;assert(UiCore_Current(&m)->draft==105);Dump("edit",&m);
 UiCore_Event(&m,UI_SHORTCUT);assert(m.depth==2 && m.path[0].selected==6 && UiCore_Current(&m)->page==UI_DEBUG);Dump("debug",&m);
 Times(&m,UI_NEXT,2);UiCore_Event(&m,UI_SHORTCUT);assert(m.depth==original.depth && !memcmp(m.path,original.path,sizeof(m.path)) && writes==0 && actions==0);
 UiCore_Event(&m,UI_OK);assert(values[9]==105 && writes==1 && UiCore_Current(&m)->page==UI_PARAMS);
 UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_NEXT);UiCore_Event(&m,UI_SHORTCUT);values[9]=200;UiCore_Event(&m,UI_SHORTCUT);UiCore_Event(&m,UI_OK);assert(m.result==UI_STALE && writes==1 && UiCore_Current(&m)->page==UI_EDIT);
 UiCore_Event(&m,UI_BACK);UiCore_Event(&m,UI_SHORTCUT);UiCore_Event(&m,UI_BACK);assert(m.depth==1&&!m.has_saved && m.path[0].selected==6);UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_SHORTCUT);assert(m.depth==1);
 UiCore_Init(&m,&ops);UiCore_Event(&m,UI_NEXT);UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_OK);assert(UiCore_Current(&m)->page==UI_CONFIRM&&!UiCore_Current(&m)->selected);Dump("confirm",&m);
 UiCore_Event(&m,UI_SHORTCUT);UiCore_Event(&m,UI_SHORTCUT);UiCore_Event(&m,UI_OK);assert(actions==0);UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_NEXT);gate=UI_BUSY;UiCore_Event(&m,UI_OK);assert(actions==0 && !UiCore_Current(&m)->selected);gate=UI_ACCEPTED;UiCore_Event(&m,UI_NEXT);UiCore_Event(&m,UI_SHORTCUT);UiCore_Event(&m,UI_SHORTCUT);assert(actions==0 && UiCore_Current(&m)->selected==1);UiCore_Event(&m,UI_OK);assert(actions==1);
 UiCore_Init(&m,&ops);Times(&m,UI_NEXT,2);UiCore_Event(&m,UI_OK);Dump("defaults",&m);UiCore_Event(&m,UI_OK);assert(UiCore_Current(&m)->page==UI_FACTORY);UiCore_Event(&m,UI_OK);assert(writes==1);
 UiCore_Init(&m,&ops);UiCore_Event(&m,UI_OK);Times(&m,UI_NEXT,12);values[12]=10;UiCore_Event(&m,UI_OK);
 assert(UiCore_Current(&m)->original==10 && UiCore_Current(&m)->draft==20);UiCore_Event(&m,UI_BACK);assert(values[12]==10);
 UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_OK);assert(values[12]==20 && writes==2);
 UiCore_Init(&m,&ops);Times(&m,UI_NEXT,4);UiCore_Event(&m,UI_OK);assert(UiCore_Current(&m)->page==UI_HELP);UiCore_Event(&m,UI_OK);assert(helps==1&&last_action==0);Dump("help",&m);
 UiCore_Init(&m,&ops);Times(&m,UI_NEXT,5);UiCore_Event(&m,UI_OK);assert(UiCore_Current(&m)->page==UI_IMAGE);Dump("image_menu",&m);
 UiCore_Event(&m,UI_OK);assert(UiCore_Current(&m)->page==UI_VIEW);{UiView v;UiCore_View(&m,&v);assert(v.camera);}
 original=m;UiCore_Event(&m,UI_SHORTCUT);{UiView v;UiCore_View(&m,&v);assert(!v.camera);}UiCore_Event(&m,UI_SHORTCUT);assert(!memcmp(m.path,original.path,sizeof(m.path)));
 UiCore_Event(&m,UI_BACK);assert(UiCore_Current(&m)->page==UI_IMAGE);
 UiCore_Init(&m,&ops);Times(&m,UI_NEXT,1);UiCore_Event(&m,UI_OK);UiCore_Event(&m,UI_OK);{UiView v;UiCore_View(&m,&v);assert(strstr(v.row[2],"stop")||strstr(v.row[2],"STOP"));assert(strstr(v.row[3],"open"));}
 UiCore_Init(&m,&ops);Times(&m,UI_NEXT,6);UiCore_Event(&m,UI_OK);assert(UiCore_Current(&m)->page==UI_DEBUG);UiCore_Event(&m,UI_OK);assert(last_run==20);UiCore_Event(&m,UI_NEXT);UiCore_Event(&m,UI_OK);assert(last_run==21);UiCore_Event(&m,UI_BACK);assert(m.depth==1);
 puts("PASS UI v2: categorized actions/help, explicit image ownership, shortcut restores image page, filtered command mapping");
 UiInput_Init(&input);
 assert(UiInput_Sample(&input,10,1,0,0,0)==UI_NONE);assert(UiInput_Sample(&input,100,0,0,0,0)==UI_PREV);
 assert(UiInput_Sample(&input,200,0,1,0,0)==UI_NONE);assert(UiInput_Sample(&input,250,0,0,0,0)==UI_NEXT);
 UiInput_Sample(&input,300,0,0,1,0);assert(UiInput_Sample(&input,400,0,0,0,0)==UI_OK);
 UiInput_Sample(&input,500,0,0,0,1);assert(UiInput_Sample(&input,600,0,0,0,0)==UI_BACK);
 UiInput_Sample(&input,700,0,0,1,0);assert(UiInput_Sample(&input,710,0,0,1,1)==UI_SHORTCUT);
 assert(UiInput_Sample(&input,800,0,0,1,0)==UI_NONE);assert(UiInput_Sample(&input,810,0,0,1,1)==UI_NONE);
 assert(UiInput_Sample(&input,900,0,0,0,0)==UI_NONE);
 UiInput_Sample(&input,1000,0,0,1,0);assert(UiInput_Sample(&input,3000,0,0,0,0)==UI_OK);
 UiInput_Sample(&input,0xFFFFFF00U,1,0,0,0);assert(UiInput_Sample(&input,0x200U,0,0,0,0)==UI_PREV);
 memset(&a,0,sizeof(a));a.magic=UI_PROFILE_MAGIC;a.version=UI_PROFILE_VERSION;a.sequence=1;a.flags=1;cpr.f=1061.268f;a.cpr_bits=cpr.bits;a.crc=UiProfile_Crc(&a);assert(UiProfile_Valid(&a));
 memset(&b,255,sizeof(b));assert(UiProfile_Latest(&a,&b)==0);b=a;b.sequence=2;b.crc=UiProfile_Crc(&b);assert(UiProfile_Latest(&a,&b)==1);
 for(i=0;i<sizeof(b);i++){c=b;((unsigned char*)&c)[i]^=1;assert(!UiProfile_Valid(&c));assert(UiProfile_Latest(&a,&c)==0);}
 for(i=0;i<sizeof(b);i+=32){memset(&c,255,sizeof(c));memcpy(&c,&b,i);assert(UiProfile_Latest(&a,&c)==0);}
 c=b;c.flags=0;c.sequence=3;c.crc=UiProfile_Crc(&c);assert(UiProfile_Latest(&b,&c)==1 && UiProfile_Valid(&c));
 a.sequence=0xFFFFFFFFU;a.crc=UiProfile_Crc(&a);b.sequence=0;b.crc=UiProfile_Crc(&b);assert(UiProfile_Latest(&a,&b)==1);
 c=b;c.reserved[0]=UI_PROFILE_VIEW_TAG;c.reserved[1]=64;c.crc=UiProfile_Crc(&c);assert(!UiProfile_Valid(&c));c.reserved[1]=63;c.crc=UiProfile_Crc(&c);assert(UiProfile_Valid(&c));
 c=b;c.version=2;c.crc=UiProfile_Crc(&c);assert(!UiProfile_Valid(&c));
 for(j=0;j<1000;j++){UiCore_Event(&m,(UiEvent)(1U+j%5U));assert(m.depth>=1&&m.depth<=UI_DEPTH);UiView v;UiCore_View(&m,&v);for(i=0;i<UI_ROWS;i++)assert(v.row[i][UI_COLUMNS]==0);}
 puts("PASS navigation: exact shortcut restore, normal ancestry, scrolling, stale edit, cancel/confirm, read-only view");
 puts("PASS keys: release-only, chord suppression/latch, four dedicated keys; holds retain their assigned role, uint32 wrap");
 puts("PASS records: CRC corruption, partial Flashwords, tombstone, sequence wrap, schema version");return 0;
}
