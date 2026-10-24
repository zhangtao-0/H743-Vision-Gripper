#include "ui_profile.h"
#include <stddef.h>
typedef char ProfileSizeMustBe160[(sizeof(UiProfile)==160U)?1:-1];
uint32_t UiProfile_Crc(const UiProfile *p){
 const uint8_t *b=(const uint8_t *)p;uint32_t crc=0xFFFFFFFFUL,i,j;
 for(i=0;i<offsetof(UiProfile,crc);i++){crc^=b[i];for(j=0;j<8;j++)crc=(crc>>1)^((0U-(crc&1U))&0xEDB88320UL);}
 return ~crc;
}
uint8_t UiProfile_Valid(const UiProfile *p){
 union{uint32_t bits;float value;} cpr;
 if(p->magic!=UI_PROFILE_MAGIC||p->version!=UI_PROFILE_VERSION||p->flags>1U||p->crc!=UiProfile_Crc(p))return 0;
 if(!p->flags)return 1;
 if(p->reserved[0] && (p->reserved[0]!=UI_PROFILE_VIEW_TAG || p->reserved[1]>63U))return 0;
 cpr.bits=p->cpr_bits;
 if((cpr.bits&0x7F800000UL)==0x7F800000UL || !(cpr.value>0 && cpr.value<=1000000.0f))return 0;
 return p->motor_target<=65535U && p->kp_milli<=100000U && p->ki_milli<=100000U && p->kd_milli<=100000U;
}
int UiProfile_Latest(const UiProfile *a,const UiProfile *b){
 uint8_t va=UiProfile_Valid(a),vb=UiProfile_Valid(b);
 if(!va)return vb?1:-1;if(!vb)return 0;
 return (int32_t)(b->sequence-a->sequence)>0?1:0;
}
