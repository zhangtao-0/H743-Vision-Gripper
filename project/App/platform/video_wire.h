#ifndef VIDEO_WIRE_H
#define VIDEO_WIRE_H
#include <stdint.h>
#define VIDEO_HEADER_SIZE 40U
/* CRC-32/ISO-HDLC, reflected 0xEDB88320; same as Python zlib.crc32. */
static inline uint32_t VideoCrc(uint32_t crc,const uint8_t *p,uint32_t size){
 uint32_t i;uint8_t b;
 for(i=0;i<size;i++){crc^=p[i];for(b=0;b<8;b++)crc=(crc>>1)^((crc&1U)?0xEDB88320UL:0U);}
 return crc;
}
static inline void VideoPut16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline void VideoPut32(uint8_t *p,uint32_t v){VideoPut16(p,(uint16_t)v);VideoPut16(p+2,(uint16_t)(v>>16));}
#endif
