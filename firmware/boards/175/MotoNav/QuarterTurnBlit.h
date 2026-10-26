#pragma once
#include <stdint.h>
namespace QuarterTurnBlit {
static inline uint16_t swap(uint16_t c){return (uint16_t)((c>>8)|(c<<8));}
// Read adjacent source pixels together for a strip of output rows. The output
// buffer is internal DMA memory, so its column-strided stores are inexpensive.
static inline void pack(uint16_t* out,const uint16_t* in,int width,int firstRow,int rows,int turn) {
  for(int x=0;x<width;x++) {
    const uint16_t* source=turn==1?in+(width-1-x)*width+firstRow:in+x*width+width-1-firstRow;
    for(int y=0;y<rows;y++)out[y*width+x]=swap(source[turn==1?y:-y]);
  }
}
}
