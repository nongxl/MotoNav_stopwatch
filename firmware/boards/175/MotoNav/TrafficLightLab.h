#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

// A short lease for neutral card geometry. It contains no traffic-light phase.
struct TrafficLightFrame {
  int key=0;
  uint32_t at=0,ttl=0;
  void clear(){key=0;ttl=0;}
  bool valid(uint32_t now)const{return key>0 && (uint32_t)(now-at)<ttl;}
  bool receive(const char* text,uint32_t now){
    if(!text || strncmp(text,"#TLA,",5)!=0){clear();return false;}
    const char* p=text+5;char* end=nullptr;errno=0;
    if(*p<'0'||*p>'9'){clear();return false;}
    long id=strtol(p,&end,10);
    if(errno==ERANGE||id>INT_MAX||*end!=','){clear();return false;}
    p=end+1;errno=0;
    if(*p<'0'||*p>'9'){clear();return false;}
    long life=strtol(p,&end,10);
    if(errno==ERANGE||*end!='\0'||(id==0?life!=0:life<=0||life>1500)){clear();return false;}
    key=id;ttl=(uint32_t)life;at=now;return true;
  }
};

struct TrafficLightPhase {
  int source=0,status=0,dir=0,wait=0,show=0;
  uint32_t remainingMs=0,ttl=0,at=0,countAt=0;
  bool active=false;
  void clear(){active=false;source=0;}
  bool valid(uint32_t now)const{return active&&(uint32_t)(now-at)<ttl;}
  void update(int src,int st,int ms,int d,int w,int sh,int life,uint32_t now){
    uint32_t age=(uint32_t)(now-countAt);
    uint32_t oldRemaining=age<remainingMs?remainingMs-age:0;
    bool same=valid(now)&&src==source&&st==status&&d==dir;
    int32_t error=(int32_t)ms-(int32_t)oldRemaining;
    if(!same||error>1800||error< -1800||ms==0){remainingMs=(uint32_t)ms;countAt=now;}
    source=src;status=st;dir=d;wait=w;show=sh;ttl=(uint32_t)life;at=now;active=true;
  }
  unsigned seconds(uint32_t now)const{
    uint32_t age=(uint32_t)(now-countAt);
    return valid(now)&&age<remainingMs?(remainingMs-age+999)/1000:0;
  }
};

// One atomic upstream snapshot. Missing directions are removed immediately.
// No accumulation across packets, phase prediction or reconnect persistence.
struct TrafficLightLab {
  TrafficLightPhase phases[4]; // left, straight, right, unidentified
  int source=0;
  static int index(int d){return d==1?0:d==4?1:d==2?2:d==0?3:-1;}
  void clear(){for(auto &p:phases)p.clear();source=0;}
  bool valid(uint32_t now)const{for(const auto &p:phases)if(p.valid(now))return true;return false;}
  unsigned count(uint32_t now)const{unsigned n=0;for(const auto &p:phases)if(p.valid(now))++n;return n;}
  static bool integer(const char* &p,int &value,bool last){
    if(!p||!(*p=='-'||(*p>='0'&&*p<='9')))return false;
    errno=0;char* end=nullptr;long v=strtol(p,&end,10);
    if(end==p||errno==ERANGE||v<INT_MIN||v>INT_MAX)return false;
    if(last?*end!='\0':*end!=',')return false;
    value=(int)v;p=last?end:end+1;return true;
  }
  static bool values(int d,int st,int ms,int w,int sh,int life){
    return index(d)>=0&&st>=-1&&st<=255&&ms>=0&&ms<=999000&&w>=0&&w<=999&&sh>=0&&sh<=255&&life>0&&life<=3000;
  }
  bool receive(const char* text,uint32_t now){
    if(!text){clear();return false;}
    if(strcmp(text,"#TL,0")==0){clear();return true;}
    int src=0,n=0,rows[4][6]={};
    if(strncmp(text,"#TLS,",5)==0){
      const char* p=text+5;
      if(!integer(p,src,false)||src!=1||!integer(p,n,false)||n<1||n>4){clear();return false;}
      unsigned mask=0;
      for(int i=0;i<n;++i){
        for(int j=0;j<6;++j)if(!integer(p,rows[i][j],i==n-1&&j==5)){clear();return false;}
        if(!values(rows[i][0],rows[i][1],rows[i][2],rows[i][3],rows[i][4],rows[i][5])){clear();return false;}
        unsigned bit=1U<<index(rows[i][0]);
        if(mask&bit){clear();return false;}mask|=bit;
      }
    } else {
      // Backward-compatible authoritative single-light snapshot.
      if(strncmp(text,"#TL,",4)!=0){clear();return false;}
      const char* p=text+4;
      if(!integer(p,src,false)||!integer(p,rows[0][1],false)||!integer(p,rows[0][2],false)||
          !integer(p,rows[0][0],false)||!integer(p,rows[0][3],false)||!integer(p,rows[0][4],false)||!integer(p,rows[0][5],true)||
          (src!=1&&src!=2)||!values(rows[0][0],rows[0][1],rows[0][2],rows[0][3],rows[0][4],rows[0][5])){clear();return false;}
      n=1;
    }
    unsigned mask=0;
    for(int i=0;i<n;++i){int *v=rows[i];int slot=index(v[0]);mask|=1U<<slot;phases[slot].update(src,v[1],v[2],v[0],v[3],v[4],v[5],now);}
    for(int i=0;i<4;++i)if(!(mask&(1U<<i)))phases[i].clear();
    source=src;return true;
  }
};
