// Link-time declarations only. These stubs are never packaged or installed.
// SONAME dependencies resolve to Android's system libdl.so/libc.so at runtime.
#if defined(STUB_DL)
extern "C" void *dlopen(const char *,int){return nullptr;}
extern "C" void *dlsym(void *,const char *){return nullptr;}
#else
extern "C" void *malloc(unsigned long){return nullptr;}
extern "C" void free(void *){}
extern "C" int snprintf(char *,unsigned long,const char *,...){return 0;}
#endif
