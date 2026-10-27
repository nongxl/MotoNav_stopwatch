#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <initializer_list>
#include "../libraries/MotoNavIpcStackGuard/src/IpcStackPolicy.h"
int main() {
  using MotoNavIpcStack::requestedBytes;
  assert(requestedBytes("ipc0",1024,24,25,0,0)==4096);
  assert(requestedBytes("ipc1",1024,24,25,1,1)==4096);
  assert(requestedBytes("ipc0",8192,24,25,0,0)==8192);
  assert(requestedBytes("ipc0",4096,24,25,0,0)==4096);
  assert(requestedBytes("ipc0",1024,24,25,1,0)==1024);
  assert(requestedBytes("ipc0",1024,23,25,0,0)==1024);
  assert(requestedBytes("ipc0",1024,24,25,0,9)==1024);
  assert(requestedBytes("ipc2",1024,24,25,2,2)==1024);
  assert(requestedBytes("ipc0-extra",1024,24,25,0,0)==1024);
  assert(requestedBytes("i",1024,24,25,0,0)==1024);
  assert(requestedBytes("",1024,24,25,0,0)==1024);
  assert(requestedBytes(nullptr,1024,24,25,0,0)==1024);
  for(const char *name:{"flush","maphelper","loopTask","nimble_host","btdm"})
    for(unsigned size:{1024u,2048u,4096u,8192u})
      assert(requestedBytes(name,size,24,25,0,0)==size);
}
