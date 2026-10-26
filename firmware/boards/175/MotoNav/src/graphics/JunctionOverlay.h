#pragma once
// 右侧竖排路口灯绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// Keep the map centre free: one compact row per signal on the right edge.
void drawTrafficLightLab(uint32_t now) {
  const bool preview=trafficLightPreviewUntil && (int32_t)(trafficLightPreviewUntil-now)>0;
  const bool pending=!preview && trafficLightFrame.valid(now) && !trafficLightLab.valid(now);
  const bool other=!preview && trafficLightLab.phases[3].valid(now);
  const int rows=other?4:3,x=CX+91,y=140,w=96,h=40,gap=3;
  static int previousRows=0;
  if(previousRows>rows)navDirty=true;
  previousRows=rows;
  gfx->fillRoundRect(x-2,y-14,w+4,rows*(h+gap)+12,8,0x1082);
  printCentered(preview?String("PREVIEW"):String("LIGHTS"),x+w/2,y-11,1,C_WHITE);
  uint32_t signature=preview?0xFFFF:0;
  for(int i=0;i<rows;++i) {
    const TrafficLightPhase &p=trafficLightLab.phases[i];
    const bool present=!preview&&p.valid(now);
    const bool known=present&&(p.status==1||p.status==4||p.status==-1);
    // Missing right-lamp data is a route cue only; do not create a phase/countdown.
    const bool rightMissing=!preview && !pending && i==2 && !known;
    const uint16_t color=!known?C_GREY:p.status==1?C_RED:p.status==4?C_GREEN:0xFFE0;
    const int ry=y+i*(h+gap);
    gfx->fillRoundRect(x,ry,w,h,7,0x1082);
    gfx->drawRoundRect(x,ry,w,h,7,color);
    if(i==1) {
      gfx->fillRect(x+13,ry+16,4,13,color);
      gfx->fillTriangle(x+15,ry+6,x+6,ry+19,x+24,ry+19,color);
    } else if(i==0) {
      gfx->fillRect(x+19,ry+15,4,14,color);gfx->fillRect(x+9,ry+15,14,4,color);
      gfx->fillTriangle(x+3,ry+17,x+13,ry+8,x+13,ry+26,color);
    } else if(i==2) {
      gfx->fillRect(x+8,ry+15,4,14,color);gfx->fillRect(x+8,ry+15,14,4,color);
      gfx->fillTriangle(x+28,ry+17,x+18,ry+8,x+18,ry+26,color);
    } else gfx->fillCircle(x+15,ry+17,6,color);
    unsigned seconds=present?p.seconds(now):0;
    if(rightMissing)gfx->fillCircle(x+64,ry+15,10,C_GREY);
    else printCentered(known&&seconds?String(seconds):String("--"),x+64,ry+3,3,color);
    const char* label=i==0?"LEFT":i==1?"STRAIGHT":i==2?"RIGHT":"OTHER";
    printCentered(String(label),x+64,ry+29,1,C_WHITE);
    if(present&&p.wait>0)printCentered(String("W")+p.wait,x+15,ry+30,1,C_GREY);
    signature=signature*31+(present?((p.status+2)*1000+seconds):0);
  }
  static uint32_t previous=0;
  if(signature!=previous) {
    Serial.printf("TL JUNCTION DRAW layout=right-stack preview=%d count=%u L=%u S=%u R=%u inNav=%d map=%d\n",preview,trafficLightLab.count(now),
      trafficLightLab.phases[0].seconds(now),trafficLightLab.phases[1].seconds(now),trafficLightLab.phases[2].seconds(now),inNav,mapModeActive());
    previous=signature;
  }
  needFlush=true;
}
