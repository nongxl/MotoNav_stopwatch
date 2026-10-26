#pragma once
#include <stdint.h>
namespace RingFastColors {
static constexpr int First=211*64+32,Last=236*64,Count=Last-First+1;
struct Key {bool track,trip,prog,over;int halfWidth;uint16_t color;};
static inline bool same(const Key& a,const Key& b) {
  return a.track==b.track && a.trip==b.trip && a.prog==b.prog && a.over==b.over && a.halfWidth==b.halfWidth && a.color==b.color;
}
static inline int coverage(int r,int a,int b) {
  int x=r-(a*64-32),y=(b*64+32)-r,c=x<y?x:y;
  return c<=0?0:c>=64?64:c;
}
static inline uint16_t blend(uint16_t a,uint16_t b,unsigned t) {
  unsigned s=256-t;
  return (uint16_t)(((((a>>11)*s+(b>>11)*t)>>8)<<11)|
      (((((a>>5)&63)*s+((b>>5)&63)*t)>>8)<<5)|(((a&31)*s+(b&31)*t)>>8));
}
static inline uint16_t color(int r,int phase,const Key& key,int ringRadius,int tripHalf,
    uint16_t track,uint16_t tripDone,uint16_t tripLeft,uint16_t red) {
  uint16_t c=0;int a;
  if(key.track && (a=coverage(r,ringRadius-5,ringRadius+5)))c=a>=64?track:blend(c,track,a*4);
  if(key.trip && (a=coverage(r,ringRadius-tripHalf,ringRadius+tripHalf))) {
    uint16_t tc=(phase&2)?tripDone:tripLeft;c=a>=64?tc:blend(c,tc,a*4);
  }
  if(key.prog && (phase&1) && (a=coverage(r,ringRadius-key.halfWidth,ringRadius+key.halfWidth)))
    c=a>=64?key.color:blend(c,key.color,a*4);
  if(key.over && (a=coverage(r,228,233)))c=a>=64?red:blend(c,red,a*4);
  return c;
}
}
