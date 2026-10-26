#include <cassert>
#include <cmath>
#include <cstdio>
#include "../boards/175/MotoNav/TurnRingMotion.h"
bool close(float a, float b) {return std::fabs(a-b)<0.00001f;}
int main() {
  TurnRingMotion r;
  r.update(0,0.1f,true,100);
  assert(close(r.value(100),0.1f));
  r.update(r.value(1100),0.14f,false,1100);
  assert(close(r.value(1350),0.11f));
  assert(close(r.value(1600),0.12f));
  assert(close(r.value(1850),0.13f));
  assert(close(r.value(2100),0.14f));
  r.update(r.value(1600),0.14f,false,1600); // duplicate does not restart
  assert(close(r.value(1850),0.13f));
  assert(close(r.value(9999),0.14f)); // missing fixes never advance beyond target
  r.update(r.value(2100),0.16f,false,2100);
  assert(close(r.value(2350),0.15f)); // updated measured 500 ms cadence
  r.update(r.value(2400),0.3f,true,2400); // direction change/new approach
  assert(close(r.value(2400),0.3f));
  r.update(r.value(3400),0.32f,false,3400);
  r.update(r.value(3500),0.05f,false,3500); // same-direction consecutive turn
  assert(close(r.value(3500),0.05f));
  r.update(r.value(7000),0.5f,false,7000); // stale gap snaps to verified distance
  assert(close(r.value(7000),0.5f));
  r.update(.5f,2,true,7100); assert(r.value(7100)==1);
  r.update(1,-1,true,7200); assert(r.value(7200)==0);
  TurnRingMotion wrap;
  wrap.update(0,.2f,true,0xffffff00u);
  wrap.update(.2f,.4f,false,0x2e8u); // 1000 ms across millis wrap
  assert(close(wrap.value(0x4dcu),.3f));
  puts("PASS: continuous packet-cadence motion, duplicates, stale/stop, resets, bounds, clock wrap");
}
