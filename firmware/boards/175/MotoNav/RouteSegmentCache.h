#pragma once
#include <stdint.h>
#include <math.h>

// Small exact cache, rather than subtracting rounded cumulative distances.
// Direct mapping bounds RAM use; collisions only recompute the original expression.
template<int Slots> class RouteSegmentCache {
  int16_t tag[Slots];
  float value[Slots];
public:
  void clear() { for(int i=0;i<Slots;i++)tag[i]=-1; }
  float length(int index,const int32_t* x,const int32_t* y) {
    int slot=index%Slots;
    if(tag[slot]!=index) {
      float dx=x[index+1]-x[index],dy=y[index+1]-y[index];
      value[slot]=sqrtf(dx*dx+dy*dy);
      tag[slot]=(int16_t)index;
    }
    return value[slot];
  }
};
