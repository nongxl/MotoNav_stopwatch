#ifdef NDEBUG
#undef NDEBUG
#endif
// Executes the production gesture and weather pages using an in-memory display.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
using std::min; using std::max;
struct String : std::string {
  using std::string::string; using std::string::operator=;
  String()=default; String(const std::string& s):std::string(s){}
  String(int n):std::string(std::to_string(n)){}
  String(unsigned n):std::string(std::to_string(n)){}
  String(float n,int places):std::string(format(n,places)){}
  static std::string format(double n,int p){std::ostringstream s;s<<std::fixed<<std::setprecision(p)<<n;return s.str();}
  void toCharArray(char* b,size_t n)const{if(n){std::strncpy(b,c_str(),n);b[n-1]=0;}}
  int toInt()const{return std::atoi(c_str());}
};
String operator+(const std::string& s,int n){return String(s+std::to_string(n));}
constexpr float PI=3.14159265358979323846f;
constexpr int LCD_WIDTH=466,LCD_HEIGHT=466,CX=233,WAIT_READY=1,WAIT_NOLINK=0;
constexpr int MALLOC_CAP_SPIRAM=1;
constexpr uint16_t C_BLACK=0,C_CYAN=0x07ff,C_WHITE=0xffff,C_GREY=0x8410,C_ORANGE=0xfd20,C_RED=0xf800;
template<class T> T constrain(T n,T lo,T hi){return min(hi,max(lo,n));}
uint32_t fakeNow=100;
unsigned long millis(){fakeNow+=5;return fakeNow;}
void* heap_caps_malloc(size_t n,int){return std::malloc(n);}
void xSemaphoreGive(int){}
int flushDone=0;
struct Canvas {
  uint16_t pixels[466*466]{};
  void fillScreen(uint16_t c){std::fill(pixels,pixels+466*466,c);}
  uint16_t* getFramebuffer(){return pixels;}
  void drawPixel(int x,int y,uint16_t c){if(x>=0&&x<466&&y>=0&&y<466)pixels[y*466+x]=c;}
  void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)drawPixel(i,j,c);}
  void fillRoundRect(int x,int y,int w,int h,int,uint16_t c){fillRect(x,y,w,h,c);}
  void fillTriangle(int ax,int ay,int bx,int by,int cx,int cy,uint16_t c){
    for(int y=min({ay,by,cy});y<=max({ay,by,cy});y++)for(int x=min({ax,bx,cx});x<=max({ax,bx,cx});x++){
      int a=(x-ax)*(by-ay)-(y-ay)*(bx-ax),b=(x-bx)*(cy-by)-(y-by)*(cx-bx),d=(x-cx)*(ay-cy)-(y-cy)*(ax-cx);
      if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))drawPixel(x,y,c);
    }
  }
} canvas;
Canvas* gfx=&canvas;
struct Text {std::string value;int y;};
std::vector<Text> text;
void printCentered(const std::string& s,int,int y,int,uint16_t){text.push_back({s,y});}
void drawTextImg(int slot,int y,uint8_t){text.push_back({"bitmap-slot-"+std::to_string(slot),y});}
void aaDisc(float x,float y,float r,uint16_t c){for(int j=(int)(y-r);j<=y+r;j++)for(int i=(int)(x-r);i<=x+r;i++)if((i-x)*(i-x)+(j-y)*(j-y)<=r*r)canvas.drawPixel(i,j,c);}
void drawPageDots(){}
void drawRedRing(){}
void flushWait(){}
void flushAsync(){}
void drawNavFrame(){text.push_back({"navigation",0});}
void drawWaiting(int){text.push_back({"home",0});}
bool onWaitScreen=false,frameIsMap=false,bleConnected=true,bleAuthed=true,needFlush=false;
bool inNav=true,cruiseOn=false,callOn=false,pairOn=false,sliding=false,wxDirty=false;
int page=0,wxDetail=0,navDist=120,riderPrevF=0,riderPrevZ=0;
unsigned long pageAt=0;
bool musicDirty=false,musicHave=false,musicPlaying=false,hidMusicMode=false,musicLyrics=false,navDirty=false,catForce=false;
int musicAck=0,musicPressed=-1;
unsigned long musicAckUntil=0,hidTapAt=0;
String pendingEvt,navDir="L";
float rotCosF=1,rotSinF=0;
bool rotFree=false; int screenRot=0;
uint16_t* fbBuf[2]={nullptr,nullptr}; uint16_t* flushSrc=nullptr;
struct {int showW=0;} imgs[13];
struct {
  bool available=false;
  struct {bool known=false;int state=0;double elapsed=0,rate=1;} playback;
  unsigned long playbackAt=0;
  struct {String text;} values[4];
  int controlMask(){return 0;} bool controlReady(int){return false;} bool control(int){return false;}
} appleMedia;
namespace AppleMediaData { double duration(const char*){return 0;} }
bool hidMediaReady(){return false;}
struct {bool enqueue(int){return true;}} hidPulses;
#include "../boards/175/MotoNav/FixedPageLayout.h"
#include "../boards/175/MotoNav/src/pages/WeatherPage.h"
#include "../boards/175/MotoNav/src/pages/PageNavigation.h"

void reset(int p=0,int detail=0){
  page=p;wxDetail=detail;dragging=false;sliding=false;dragV=0;pageAt=fakeNow;
  text.clear();wxDirty=false;musicDirty=false;pendingEvt="";
}
void swipe(int dx,int dy,bool finish=true){
  dragMove(-dx,-dy,millis()); // The hardware has both axis flips enabled.
  if(finish)dragEnd();
}
bool has(const std::string& value){for(const auto& s:text)if(s.value==value)return true;return false;}
int main(){
  // Every adjacent transition is reversible; outside edges are immovable.
  reset();swipe(350,0);assert(page==2);
  swipe(-350,0);assert(page==0);
  swipe(-350,0);assert(page==1&&wxDetail==0);
  swipe(0,350);assert(page==1&&wxDetail==1);
  swipe(0,350);assert(page==1&&wxDetail==1&&!dragging);
  swipe(350,0);assert(page==1&&wxDetail==1&&!dragging);
  swipe(0,-350);assert(page==1&&wxDetail==0);
  swipe(0,-350);assert(page==1&&wxDetail==2);
  swipe(0,-350);assert(page==1&&wxDetail==2&&!dragging);
  swipe(0,350);assert(page==1&&wxDetail==0);
  swipe(350,0);assert(page==0);
  swipe(0,350);assert(page==0&&!dragging);
  reset(2);swipe(350,0);assert(page==2&&!dragging);
  swipe(0,350);assert(page==2&&!dragging);

  // Preparing and cancelling a vertical drag must not change the actual detail.
  reset(1,0);swipe(0,24,false);assert(dragging&&wxDetail==0);
  dragV=0;dragEnd();assert(page==1&&wxDetail==0&&!sliding);
  reset(1,1);swipe(0,-80,false);dragAbort();assert(page==1&&wxDetail==1);

  // New warnings and screen taps never steal the selected weather page.
  reset(1,1);wxHave=true;
  parseWeatherExtra("#WX,42,1,3,2,123,0,40");assert(wxDetail==1&&wxDirty);
  weatherTap();assert(wxDetail==1);
  reset(1,0);parseWeatherExtra("#WX,42,2,4,2,124,0,40");assert(wxDetail==0);

  // Near a turn and after 15 seconds: user-selected pages remain visible.
  reset(2);fakeNow+=90000;musicTick();assert(page==2);
  reset(1,2);fakeNow+=90000;weatherTick();assert(page==1&&wxDetail==2);

  // Missing weather data retains the selected page and never invents no-rain.
  wxExtra=false;wxHave=false;reset(1,1);drawWeather();
  assert(has("RAIN / AIR")&&has("RAIN DATA UNAVAILABLE")&&!has("WEATHER"));
  reset(1,2);drawWeather();assert(has("ALERT DATA UNAVAILABLE"));
  wxExtra=true;wxHave=true;wxRainN=2;wxRain[0]=-1;wxRain[1]=-1;
  reset(1,1);drawWeather();assert(has("RAIN DATA UNAVAILABLE"));
  wxRain[0]=0;wxRain[1]=0;reset(1,1);drawWeather();
  assert(has("NEXT 2H  MAX 0.00 mm/5m"));
  wxRain[1]=40;reset(1,1);drawWeather();assert(has("NEXT 2H  MAX 0.40 mm/5m"));
  wxHave=false;wxExtra=false;reset(1,0);drawWeather();assert(has("WEATHER"));

  // Phone offline, no weather data: all five positions still remain accessible.
  bleConnected=false;bleAuthed=false;reset();swipe(-350,0);swipe(0,-350);
  assert(page==1&&wxDetail==2);swipe(0,350);swipe(350,0);swipe(350,0);assert(page==2);
  std::puts("PASS: fixed adjacency, gesture commit/cancel/abort, page persistence, offline access, and missing/rain/alert states");
}
