#pragma once
// Temporary on-board measurements. Never run in the release build.
void fpsPattern(uint16_t* p) {
  for(int y=0;y<LCD_HEIGHT;y++)for(int x=0;x<LCD_WIDTH;x++)
    p[y*LCD_WIDTH+x]=(uint16_t)(((x*19+y*7)&31)<<11 | ((x*3+y*11)&63)<<5 | ((x*13+y*17)&31));
}
void runFpsSelfTest() {
  Serial.printf("FPS BENCH BEGIN deg=%d mhz=%d free=%u\n",screenDeg,qspiMHz,(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  uint16_t* image=gfx->getFramebuffer();
  uint16_t* reference=(uint16_t*)ps_malloc(LCD_WIDTH*LCD_HEIGHT*2);
  if(!reference || !image || !rasterBuf){free(reference);Serial.println("FPS BENCH no memory");return;}
  const int oldTheme=themeIdx,oldDeg=screenDeg;const bool oldNight=nightOn;
  bool passed=true;
  // No drawing helper is active during this isolated transport measurement.
  if(fastDev)for(int deg:{0,90,180,270,45}) {
    setScreenDeg(deg,false);fpsPattern(image);
    for(int mode=0;mode<2;mode++) {
      uint32_t total=0;
      for(int i=0;i<6;i++) {
        uint32_t at=micros();
        if(mode)fastPush(image);else fastPushBaseline(image);
        total+=micros()-at;delay(1);
      }
      Serial.printf("FPS PUSH deg=%d new=%d mean_us=%u\n",deg,mode,(unsigned)(total/6));
    }
  }
  setScreenDeg(oldDeg,false);themeIdx=1;nightOn=false;
  rasW=rasH=1200;rasBW=75;rasMpp=2;rasX0=-1200;rasY0=1200;
  rasHasPal=false;rasterViceValid=false;rasterBackValid=false;
  memset(rasterBuf,0,RASTER_BUF_BYTES);
  for(int y=0;y<1200;y++) {
    if((y&31)==0)delay(0);
    for(int x=0;x<1200;x++) {
      int id=0;
      if(abs(3*y-x-1200)<20 || abs(x-600)<7)id=1;
      else {int dx=x%83-41,dy=y%69-34;
        if(abs(dx*9+dy*4)<220 && abs(-dx*4+dy*9)<140)id=abs(dx*9+dy*4)>190?5:4;
        if(y>850 && x<550)id=3;}
      uint32_t k=((uint32_t)((y>>4)*rasBW+(x>>4))<<8)|((y&15)<<4)|(x&15);
      rasterBuf[k>>1]|=(k&1)?id<<4:id;
    }
  }
  for(int i=0;i<16;i++)RM.pal[i]=0xe6d9;
  RM.pal[1]=0x3187;RM.pal[2]=0x760d;RM.pal[3]=0x4e3b;
  RM.pal[4]=blendF(RM.pal[0],RM.pal[1],64);RM.pal[5]=blendF(RM.pal[0],RM.pal[1],112);
  for(int pose=0;pose<6;pose++) {
    RM.cx=pose>3?1050:0;RM.cy=pose>3?800:0;
    RM.sc=pose&1?.38f:.6f;RM.cs=cosf(.47f*pose);RM.sn=sinf(.47f*pose);RM.pix2=false;
    fpsPattern(reference);RM.fb=reference;parallelRows(rasterRowsBaseline,20,446);
    fpsPattern(image);RM.fb=image;parallelRows(rasterRows,20,446);
    bool same=memcmp(reference,image,LCD_WIDTH*LCD_HEIGHT*2)==0;passed&=same;
    Serial.printf("FPS PIXELS raster pose=%d exact=%d\n",pose,same);
    if(pose<2)for(int mode=0;mode<2;mode++) {
      uint32_t total=0;
      for(int i=0;i<20;i++){uint32_t at=micros();parallelRows(mode?rasterRows:rasterRowsBaseline,20,446);total+=micros()-at;delay(1);}
      Serial.printf("FPS RASTER pose=%d new=%d mean_us=%u\n",pose,mode,(unsigned)(total/20));
    }
  }
  ringTablesInit();
  for(int test=0;test<6;test++) {
    RP.track=test&1;RP.prog=test*.18f;RP.ph=test&1?5:7;RP.pcol=0xf258;RP.over=test>3;RP.trip=test==0?-1:test*.17f;
    tripUx=sinf(RP.trip*2*PI);tripUy=-cosf(RP.trip*2*PI);ringUx=sinf(RP.prog*2*PI);ringUy=-cosf(RP.prog*2*PI);
    prepareRingFastColors();
    fpsPattern(reference);RP.fb=reference;parallelRows(ringRowsBaseline,0,LCD_HEIGHT);
    fpsPattern(image);RP.fb=image;parallelRows(ringRows,0,LCD_HEIGHT);
    bool same=memcmp(reference,image,LCD_WIDTH*LCD_HEIGHT*2)==0;passed&=same;
    Serial.printf("FPS PIXELS ring case=%d exact=%d\n",test,same);
    if(test==3)for(int mode=0;mode<3;mode++) {
      uint32_t total=0;
      for(int i=0;i<20;i++){uint32_t at=micros();parallelRows(mode==0?ringRowsBaseline:mode==1?ringRows:ringRowsNoCache,0,LCD_HEIGHT);total+=micros()-at;delay(1);}
      Serial.printf("FPS RING new=%d mean_us=%u\n",mode,(unsigned)(total/20));
    }
  }
  drawVignette(178,211,150);
  fpsPattern(reference);VP.fb=reference;parallelRows(vignetteRowsBaseline,21,445);
  fpsPattern(image);VP.fb=image;parallelRows(vignetteRows,21,445);
  bool same=memcmp(reference,image,LCD_WIDTH*LCD_HEIGHT*2)==0;passed&=same;
  Serial.printf("FPS PIXELS vignette exact=%d\n",same);
  for(int mode=0;mode<2;mode++) {
    uint32_t total=0;
    for(int i=0;i<20;i++){uint32_t at=micros();parallelRows(mode?vignetteRows:vignetteRowsBaseline,21,445);total+=micros()-at;delay(1);}
    Serial.printf("FPS VIGNETTE new=%d mean_us=%u\n",mode,(unsigned)(total/20));
  }
  free(reference);themeIdx=oldTheme;nightOn=oldNight;setScreenDeg(oldDeg,false);
  rasterValid=false;rasterBackValid=false;rasW=rasH=rasBW=0;
  drawWaiting(WAIT_NOLINK);flushSync();needFlush=false;
  Serial.printf("FPS BENCH END exact=%d free=%u\n",passed,(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
