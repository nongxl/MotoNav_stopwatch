#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../boards/175/MotoNav/IdlePowerOff.h"

IdlePowerOffTimer idlePowerOffTimer={0,false};
IdlePowerSupply idlePowerSupply={0,false,false};
bool pmuOK=true;
uint32_t testNow=0;
int status1=0,reads=0;
uint32_t millis(){return testNow;}
int pmuRead(uint8_t reg){assert(reg==0);reads++;return status1;}
#include "PowerPollUnderTest.inc"

bool tick(uint32_t now,int rawStatus,bool force=false,bool navigating=false,bool interacting=false){
  testNow=now;status1=rawStatus;idlePowerPollSupply(force);
  return idlePowerOffDueOnBattery(&idlePowerOffTimer,now,navigating,interacting,&idlePowerSupply);
}
void reset(){idlePowerOffTimer={0,false};idlePowerSupply={0,false,false};pmuOK=true;reads=0;}
int main(){
  reset();
  // VBUS-good protects even when the battery is full, absent, or not charging.
  for(uint32_t t=0;t<=1800000;t+=500)assert(!tick(t,(t&512)?0x20:0x28));
  assert(!tick(1800500,0x08));
  for(uint32_t t=1801000;t<2100500;t+=500)assert(!tick(t,0x08));
  assert(tick(2100500,0x08));

  reset();assert(!tick(0,0x08));
  // Reconnect USB at the deadline, before the 500ms cached read is due.
  assert(!tick(299500,0x08));assert(!tick(299999,0x08));
  testNow=300000;status1=0x28;idlePowerPollSupply(true);
  assert(!idlePowerOffDueOnBattery(&idlePowerOffTimer,testNow,false,false,&idlePowerSupply));

  reset();assert(!tick(0,0));assert(!tick(299999,0));
  assert(!tick(300000,-1,true)); // PMIC I/O failure cannot shut down on an unknown source.
  assert(!tick(301000,0,true));
  for(uint32_t t=301500;t<601000;t+=500)assert(!tick(t,0));
  assert(!tick(600999,0));assert(tick(601000,0,true));

  reset();pmuOK=false;assert(!tick(0,0));assert(!tick(900000,0));assert(reads==0);
  reset();assert(!tick(0,0));
  assert(!idlePowerOffDueOnBattery(&idlePowerOffTimer,300000,false,false,&idlePowerSupply));
  // A valid status is needed again after a stale sample.
  assert(!tick(300001,0,true));
  for(uint32_t t=300501;t<600001;t+=500)assert(!tick(t,0));
  assert(!tick(600000,0));assert(tick(600001,0,true));

  reset();assert(!tick(0,0x28));int first=reads;
  assert(!tick(100,0x28));assert(reads==first);
  assert(!tick(200,0x28,true));assert(reads==first+1);

  reset();assert(!tick(0xffff0000u,0));
  for(uint32_t offset=500;offset<300000;offset+=500)assert(!tick((uint32_t)(0xffff0000u+offset),0));
  assert(!tick((uint32_t)(0xffff0000u+299999u),0));
  assert(tick((uint32_t)(0xffff0000u+300000u),0,true));
  reset();assert(!tick(0,0));assert(!tick(300000,0,true,true));
  assert(!tick(600000,0,true,false,true));
  for(uint32_t t=600500;t<900000;t+=500)assert(!tick(t,0));
  assert(!tick(899999,0));assert(tick(900000,0,true));
  puts("PASS: plugged/full/no-battery, 5-minute post-unplug interval, deadline fresh read, I2C failures, stale samples, polling, navigation, interaction and clock wrap");
}
