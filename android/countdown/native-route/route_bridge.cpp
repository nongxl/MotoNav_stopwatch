#include <jni.h>

// This reader is pinned by the Java adapter to the exact 9.5.13 library hashes.
// It observes the active route, using the same result copy and accessors as HMI.
// The reader never calculates routes. A separate, explicit simulation command
// below uses the already selected path and the same provider engine.
extern "C" {
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
void *malloc(unsigned long);
void free(void *);
int snprintf(char *, unsigned long, const char *, ...);
}
using ObjFn=void(*)(void *);
static ObjFn pathDtor;
struct alignas(8) NativePath {
    unsigned char data[80];
    NativePath()=default;
    NativePath(const NativePath&)=delete;
    ~NativePath(){auto destroy=__atomic_load_n(&pathDtor,__ATOMIC_ACQUIRE);if(destroy)destroy(this);}
};
static void *allocateBytes(void *,unsigned long n){return n<=64*1024*1024 ? malloc(n) : nullptr;}
static void releaseBytes(void *,void *p){free(p);}
static void allocatorDtor(void *){}
static void *allocatorVtable[]={reinterpret_cast<void*>(allocatorDtor),reinterpret_cast<void*>(allocatorDtor),
    reinterpret_cast<void*>(allocateBytes),reinterpret_cast<void*>(releaseBytes)};
struct alignas(8) NativeVector {
    void *data=nullptr;
    unsigned allocated=0,used=0;
    void **allocator=allocatorVtable;
    unsigned char flags=0x11,padding[7]={};
    ~NativeVector(){free(data);}
};
static_assert(sizeof(NativePath)==80 && sizeof(NativeVector)==32,"pinned AArch64 layout");
struct alignas(8) NativeResult { unsigned char data[48]; };
struct alignas(8) NativeSegment { unsigned char data[80]; };
struct Point { int x,y; };
struct DoublePoint { double x,y; };
struct Text {
    char *data;
    unsigned long n=0,cap=4*1024*1024;
    Text():data(static_cast<char*>(malloc(4*1024*1024))){if(data)data[0]=0;}
    ~Text(){free(data);}
    template<typename... A> bool add(const char *format,A...args){
        if(!data||n>=cap)return false;
        int size=snprintf(data+n,cap-n,format,args...);
        if(size<0||static_cast<unsigned long>(size)>=cap-n)return false;
        n+=size;return true;
    }
};
static jstring result(JNIEnv *env,const char *s){return env->NewStringUTF(s);}
static bool valid(double lon,double lat){return lon==lon && lat==lat && lon>=-180 && lon<=180 && lat>=-90 && lat<=90 && (lon!=0||lat!=0);}

extern "C" JNIEXPORT jstring JNICALL Java_com_yuminaga_motonav_NativeRouteBridge_readNative(JNIEnv *env,jclass,jlong previous,jboolean planned){
    static void *hsl=nullptr,*gbl=nullptr;
    if(!hsl)hsl=dlopen("libhsl.so",6);
    if(!gbl)gbl=dlopen("libGbl.so",6);
    if(!hsl||!gbl)return result(env,"{\"error\":\"engine-not-loaded\"}");
    auto guide=reinterpret_cast<void*(*)()>(dlsym(hsl,"_ZN3hsl3HSL14GetGuideHandleEv"));
    auto planning=reinterpret_cast<void*(*)()>(dlsym(hsl,"_ZN3hsl3HSL14GetRouteHandleEv"));
    auto ctorResult=reinterpret_cast<ObjFn>(dlsym(hsl,"_ZN3hsl15RouteResultDataC1Ev"));
    auto dtorResult=reinterpret_cast<ObjFn>(dlsym(hsl,"_ZN3hsl15RouteResultDataD1Ev"));
    auto current=reinterpret_cast<NativePath(*)(const void*)>(dlsym(hsl,"_ZNK3hsl15RouteResultData10GetCurPathEv"));
    auto pathCount=reinterpret_cast<int(*)(const void*)>(dlsym(hsl,"_ZNK3hsl15RouteResultData12GetPathCountEv"));
    auto pathIndex=reinterpret_cast<int(*)(const void*)>(dlsym(hsl,"_ZNK3hsl15RouteResultData15GetCurPathIndexEv"));
    __atomic_store_n(&pathDtor,reinterpret_cast<ObjFn>(dlsym(gbl,"_ZN2bl8PathInfoD1Ev")),__ATOMIC_RELEASE);
    auto validPath=reinterpret_cast<bool(*)(const void*)>(dlsym(gbl,"_ZNK2bl8PathInfo7IsValidEv"));
    auto pathId=reinterpret_cast<unsigned(*)(const void*)>(dlsym(gbl,"_ZNK2bl8PathInfo9GetPathIDEv"));
    auto length=reinterpret_cast<unsigned(*)(const void*)>(dlsym(gbl,"_ZNK2bl8PathInfo9GetLengthEv"));
    auto segCount=reinterpret_cast<unsigned(*)(const void*)>(dlsym(gbl,"_ZNK2bl8PathInfo15GetSegmentCountEv"));
    auto ctorSeg=reinterpret_cast<ObjFn>(dlsym(gbl,"_ZN2bl11SegmentInfoC1Ev"));
    auto dtorSeg=reinterpret_cast<ObjFn>(dlsym(gbl,"_ZN2bl11SegmentInfoD1Ev"));
    auto getSeg=reinterpret_cast<bool(*)(const void*,unsigned,void*)>(dlsym(gbl,"_ZNK2bl8PathInfo14GetSegmentInfoEjRNS_11SegmentInfoE"));
    auto getPoints=reinterpret_cast<bool(*)(const void*,NativeVector*)>(dlsym(gbl,"_ZNK2bl11SegmentInfo9GetPointsERN4dice6vectorINS1_7Coord2DIiEENS1_12IrrAllocatorIS4_EEEE"));
    auto segmentLength=reinterpret_cast<unsigned(*)(const void*)>(dlsym(gbl,"_ZNK2bl11SegmentInfo9GetLengthEv"));
    auto convert=reinterpret_cast<void(*)(double,double,double*,double*)>(dlsym(gbl,"_ZN2bl15IUtilMapPosture11MapToLonLatEddRdS1_"));
    auto getLights=reinterpret_cast<bool(*)(const void*,NativeVector*)>(dlsym(gbl,"_ZNK2bl8PathInfo19GetAllTrafficLightsERN4dice6vectorINS1_7Coord2DIdEENS1_12IrrAllocatorIS4_EEEE"));
    if(!guide||!ctorResult||!dtorResult||!current||!__atomic_load_n(&pathDtor,__ATOMIC_ACQUIRE)||!validPath||!pathId||!length||!segCount||!ctorSeg||!dtorSeg||!getSeg||!getPoints||!convert)
        return result(env,"{\"error\":\"symbols-unavailable\"}");
    void *manager=planned&&planning?planning():guide();if(!manager)return result(env,"{\"error\":\"guide-not-ready\"}");
    // GuideLayerImpl::GetCurResultData uses guide control 0x40 / result 0x3d0.
    // Planned paths use HSL::GetRouteHandle's current request (0x58), whose
    // GetCurResultData (0x90) is also used by IRouteRequest::GetCurPath (0x505528).
    // Both are copies; no navigation starts to obtain a preview.
    void **managerVtable=*static_cast<void***>(manager);
    auto core=reinterpret_cast<void*(*)(void*)>(managerVtable[(planned?0x58:0x40)/8]);
    void *control=core(manager);if(!control)return result(env,"{\"error\":\"guide-control-unavailable\"}");
    void **controlVtable=*static_cast<void***>(control);
    auto copyResult=reinterpret_cast<bool(*)(void*,void*)>(controlVtable[(planned?0x90:0x3d0)/8]);
    NativeResult nativeResult;ctorResult(&nativeResult);
    if(!copyResult(control,&nativeResult)){dtorResult(&nativeResult);return result(env,"{\"error\":\"route-not-ready\"}");}
    int alternatives=pathCount?pathCount(&nativeResult):0,selected=pathIndex?pathIndex(&nativeResult):-1;
    NativePath path=current(&nativeResult);dtorResult(&nativeResult);
    if(!validPath(&path))return result(env,"{\"error\":\"invalid-active-path\"}");
    unsigned id=pathId(&path),metres=length(&path),segments=segCount(&path);
    if(previous>=0&&static_cast<unsigned long long>(previous)==id)return result(env,"{\"unchanged\":true}");
    if(segments==0||segments>4096||metres==0||metres>5000000)return result(env,"{\"error\":\"route-size\"}");
    Text json;if(!json.add("{\"id\":%u,\"metres\":%u,\"segments\":%u,\"alternatives\":%d,\"selected\":%d,\"points\":[",id,metres,segments,alternatives,selected))return result(env,"{\"error\":\"memory\"}");
    unsigned count=0;Point first={0,0},last={0,0};bool good=true;
    unsigned starts[4096]={},lengths[4096]={};
    for(unsigned i=0;i<segments&&good;i++){
        NativeSegment segment;ctorSeg(&segment);
        {
            NativeVector points;
            if(!getSeg(&path,i,&segment)||!getPoints(&segment,&points)||points.used>65535||points.used>points.allocated)good=false;
            starts[i]=count?count-1:0;
            if(good&&segmentLength)lengths[i]=segmentLength(&segment);
            if(good){
                auto p=static_cast<Point*>(points.data);
                for(unsigned j=0;j<points.used;j++){
                    if(count&&p[j].x==last.x&&p[j].y==last.y)continue;
                    // Route shape integers use milliarcseconds (3,600,000/degree),
                    // unlike the map renderer's P20 coordinates. Endpoint values
                    // were cross-checked against this engine's POI/light coordinates.
                    double lon=p[j].x/3600000.0,lat=p[j].y/3600000.0;
                    if(!valid(lon,lat)||count>=40000||!json.add("%s[%.7f,%.7f]",count?",":"",lat,lon)){good=false;break;}
                    if(count==0)first=p[j];count++;last=p[j];
                }
            }
        }
        dtorSeg(&segment);
    }
    if(!good||count<2)return result(env,"{\"error\":\"route-points\"}");
    json.add("%s","],\"lights\":[");unsigned lightCount=0;
    if(getLights){
        NativeVector lights;
        if(getLights(&path,&lights)&&lights.used<=2048&&lights.used<=lights.allocated){
            auto p=static_cast<DoublePoint*>(lights.data);
            for(unsigned i=0;i<lights.used;i++){
                double lon=p[i].x,lat=p[i].y;
                if(!valid(lon,lat))convert(p[i].x,p[i].y,&lon,&lat);
                if(valid(lon,lat)){if(!json.add("%s[%.7f,%.7f]",lightCount?",":"",lat,lon))break;lightCount++;}
            }
        }
    }
    if(!json.add("%s","],\"segmentStarts\":["))return result(env,"{\"error\":\"output-size\"}");
    for(unsigned i=0;i<segments;i++)if(!json.add("%s%u",i?",":"",starts[i]))return result(env,"{\"error\":\"output-size\"}");
    json.add("%s","],\"segmentMetres\":[");
    for(unsigned i=0;i<segments;i++)if(!json.add("%s%u",i?",":"",lengths[i]))return result(env,"{\"error\":\"output-size\"}");
    if(!json.add("],\"count\":%u,\"lightCount\":%u,\"coordinateScale\":3600000,\"rawFirst\":[%d,%d],\"rawLast\":[%d,%d]}",count,lightCount,first.x,first.y,last.x,last.y))return result(env,"{\"error\":\"output-size\"}");
    return result(env,json.data);
}

// Provider main thread only. Java checks the exact library hashes, owner token,
// request deadline and duplicate sequence before entering here.
// The stock NextSimulateSpeed method copies GuideParam via vtable +0x50,
// changes SimulateSpeedInfo.speed at GuideParam+0xf4, and passes the unmodified
// speed-info block at +0xe8 to vtable +0x40. Reuse that setter, retaining every
// other option. The sole owned array is destroyed exactly as the stock caller.
struct alignas(8) GuideConfigCopy { unsigned char bytes[512]; };
static int readSimulationSpeed(void *service,void *get,void (*release)(void *)) {
    auto config=reinterpret_cast<GuideConfigCopy(*)(void*)>(get)(service);
    int speed=*reinterpret_cast<int*>(config.bytes+0xf4);
    void *owned=*reinterpret_cast<void**>(config.bytes+0x140);
    if(owned)release(owned);
    return speed;
}
static int applySimulationSpeed(void *hsl,void *operate,int requested) {
    if(requested!=60 && requested!=100 && requested!=120)return -1;
    void *operation=reinterpret_cast<void*(*)()>(operate)();
    if(!operation)return -2;
    auto base=reinterpret_cast<unsigned long>(operate)-0xb07914;
    if(reinterpret_cast<unsigned long>((*static_cast<void***>(operation))[0x118/8])!=base+0x9c8b7c)return -3;
    void *service=*reinterpret_cast<void**>(static_cast<unsigned char*>(operation)+0x240);
    auto release=reinterpret_cast<void(*)(void*)>(dlsym(hsl,"_ZdaPv"));
    if(!service||!release)return -4;
    void *get=(*static_cast<void***>(service))[0x50/8],*set=(*static_cast<void***>(service))[0x40/8];
    if(!get||!set)return -5;
    auto config=reinterpret_cast<GuideConfigCopy(*)(void*)>(get)(service);
    int previous=*reinterpret_cast<int*>(config.bytes+0xf4);
    if(previous<0||previous>10000){void *p=*reinterpret_cast<void**>(config.bytes+0x140);if(p)release(p);return -6;}
    *reinterpret_cast<int*>(config.bytes+0xf4)=requested;
    reinterpret_cast<void(*)(void*,const void*)>(set)(service,config.bytes+0xe8);
    void *owned=*reinterpret_cast<void**>(config.bytes+0x140);if(owned)release(owned);
    return readSimulationSpeed(service,get,release);
}
extern "C" JNIEXPORT jstring JNICALL Java_com_yuminaga_motonav_NativeRouteBridge_setSimulationSpeedNative(JNIEnv *env,jclass,jint requested){
    void *hsl=dlopen("libhsl.so",6);if(!hsl)return result(env,"engine-unavailable");
    void *operate=dlsym(hsl,"_ZN14GuideNaviUtils17GetGuideOperationEv");if(!operate)return result(env,"symbols-unavailable");
    int actual=applySimulationSpeed(hsl,operate,requested);
    char response[96];snprintf(response,sizeof(response),actual==requested?"speed=%d":"speed-refused actual=%d",actual);
    return result(env,response);
}
extern "C" JNIEXPORT jstring JNICALL Java_com_yuminaga_motonav_NativeRouteBridge_startSimulationNative(JNIEnv *env,jclass,jlong expected,jint requestedSpeed){
    void *hsl=dlopen("libhsl.so",6),*gbl=dlopen("libGbl.so",6);
    if(!hsl||!gbl||expected<0)return result(env,"engine-unavailable");
    auto planning=reinterpret_cast<void*(*)()>(dlsym(hsl,"_ZN3hsl3HSL14GetRouteHandleEv"));
    auto operate=reinterpret_cast<void*(*)()>(dlsym(hsl,"_ZN14GuideNaviUtils17GetGuideOperationEv"));
    auto ctor=reinterpret_cast<ObjFn>(dlsym(hsl,"_ZN3hsl15RouteResultDataC1Ev"));
    auto dtor=reinterpret_cast<ObjFn>(dlsym(hsl,"_ZN3hsl15RouteResultDataD1Ev"));
    auto current=reinterpret_cast<NativePath(*)(const void*)>(dlsym(hsl,"_ZNK3hsl15RouteResultData10GetCurPathEv"));
    auto pathId=reinterpret_cast<unsigned(*)(const void*)>(dlsym(gbl,"_ZNK2bl8PathInfo9GetPathIDEv"));
    auto validPath=reinterpret_cast<bool(*)(const void*)>(dlsym(gbl,"_ZNK2bl8PathInfo7IsValidEv"));
    __atomic_store_n(&pathDtor,reinterpret_cast<ObjFn>(dlsym(gbl,"_ZN2bl8PathInfoD1Ev")),__ATOMIC_RELEASE);
    if(!planning||!operate||!ctor||!dtor||!current||!pathId||!validPath||!__atomic_load_n(&pathDtor,__ATOMIC_ACQUIRE))return result(env,"symbols-unavailable");
    void *manager=planning();if(!manager)return result(env,"request-manager-unavailable");
    auto requestGetter=reinterpret_cast<void*(*)(void*)>((*static_cast<void***>(manager))[0x58/8]);
    void *request=requestGetter(manager);if(!request)return result(env,"selected-request-unavailable");
    auto copy=reinterpret_cast<bool(*)(void*,void*)>((*static_cast<void***>(request))[0x90/8]);
    NativeResult data;ctor(&data);
    if(!copy(request,&data)){dtor(&data);return result(env,"selected-result-unavailable");}
    NativePath selected=current(&data);dtor(&data);
    if(!validPath(&selected)||pathId(&selected)!=static_cast<unsigned long long>(expected))return result(env,"selected-path-changed");
    void *operation=operate();if(!operation)return result(env,"guide-operation-unavailable");
    void *function=(*static_cast<void***>(operation))[0x80/8];
    // Verified GuideOperationImpl vtable: 0x80 is StartSimulate(IRouteRequest*)
    // at 0x9c6a2c, which enters StartGuide(mode=1). 0x78 starts real guidance;
    // 0x88 is familiar-road guidance and must not be mistaken for simulation.
    auto base=reinterpret_cast<unsigned long>(operate)-0xb07914;
    if(reinterpret_cast<unsigned long>(function)!=base+0x9c6a2c)return result(env,"simulation-interface-mismatch");
    bool started=reinterpret_cast<bool(*)(void*,void*)>(function)(operation,request);
    if(!started)return result(env,"engine-refused-simulation");
    int actual=applySimulationSpeed(hsl,reinterpret_cast<void*>(operate),requestedSpeed);
    if(actual!=requestedSpeed){
        // Refuse an unconfirmed fast simulation instead of displaying a fake speed.
        auto stop=(*static_cast<void***>(operation))[0xf8/8];
        if(reinterpret_cast<unsigned long>(stop)==base+0x9c6ab8)reinterpret_cast<bool(*)(void*,bool,bool)>(stop)(operation,false,false);
        char error[96];snprintf(error,sizeof(error),"simulation-speed-refused actual=%d",actual);return result(env,error);
    }
    char response[80];snprintf(response,sizeof(response),"started speed=%d",actual);
    return result(env,response);
}

extern "C" JNIEXPORT jstring JNICALL Java_com_yuminaga_motonav_NativeRouteBridge_stopSimulationNative(JNIEnv *env,jclass){
    void *hsl=dlopen("libhsl.so",6);if(!hsl)return result(env,"engine-unavailable");
    auto operate=reinterpret_cast<void*(*)()>(dlsym(hsl,"_ZN14GuideNaviUtils17GetGuideOperationEv"));
    if(!operate)return result(env,"symbols-unavailable");
    void *operation=operate();if(!operation)return result(env,"guide-operation-unavailable");
    void *stop=(*static_cast<void***>(operation))[0xf8/8];
    if(reinterpret_cast<unsigned long>(stop)!=reinterpret_cast<unsigned long>(operate)-0xb07914+0x9c6ab8)
        return result(env,"stop-interface-mismatch");
    bool stopped=reinterpret_cast<bool(*)(void*,bool,bool)>(stop)(operation,false,false);
    return result(env,stopped?"stopped":"engine-refused-stop");
}
