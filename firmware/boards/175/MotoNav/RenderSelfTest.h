#pragma once
// Runs before BLE starts, and is compiled out of the normal firmware.
void renderTestRoute(bool dense=false) {
  routeN=dense?6000:1200;
  routeSegmentCache.clear();
  for(int i=0;i<routeN;i++) {
    routeX[i]=dense?(i%80-40):(i*3-1800);
    routeY[i]=dense?(i/80%80-40):(int)(120*sinf(i*.02f));
  }
  for(int bi=0;bi*RB_STEP<routeN-1;bi++) {
    int a=bi*RB_STEP,b=min(routeN-1,a+RB_STEP);
    int32_t* box=rbBox+bi*4;
    box[0]=box[2]=routeX[a];box[1]=box[3]=routeY[a];
    for(int i=a+1;i<=b;i++) {
      box[0]=min(box[0],routeX[i]);box[1]=min(box[1],routeY[i]);
      box[2]=max(box[2],routeX[i]);box[3]=max(box[3],routeY[i]);
    }
  }
}
float renderTestX=0,renderTestY=0,renderTestScale=.6f,renderTestAngle=.4f;
void renderTestRange(bool before) {
  float cs=cosf(renderTestAngle),sn=sinf(renderTestAngle);
  if(before)drawRouteRangeBaseline(0,routeN-1,renderTestX,renderTestY,renderTestScale,cs,sn,600,13,9,0x3187,0xfa9c);
  else drawRouteRange(0,routeN-1,renderTestX,renderTestY,renderTestScale,cs,sn,600,13,9,0x3187,0xfa9c);
}
void runFpsSelfTest() {
  Serial.printf("RENDER BENCH BEGIN version=%s internal=%u psram=%u\n",FW_VERSION,
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  const int oldTheme=themeIdx;const bool oldNight=nightOn;
  uint16_t* image=gfx->getFramebuffer();
  uint16_t* ref=(uint16_t*)ps_malloc(LCD_WIDTH*LCD_HEIGHT*2);
  if(!image || !ref || !rasterBuf){free(ref);Serial.println("RENDER BENCH FAIL no memory");return;}
  bool passed=true;
  themeIdx=1;nightOn=false;
  rasW=rasH=1200;rasBW=75;rasMpp=2;rasX0=-1200;rasY0=1200;
  rasHasPal=false;rasterViceValid=false;rasterBackValid=false;
  memset(rasterBuf,0,RASTER_BUF_BYTES);
  for(int y=0;y<1200;y++) {
    if((y&31)==0)delay(0);
    for(int x=0;x<1200;x++) {
      uint8_t id=abs(3*y-x-1200)<20?1:(x%79<40?4:5);
      uint32_t k=((uint32_t)((y>>4)*rasBW+(x>>4))<<8)|((y&15)<<4)|(x&15);
      rasterBuf[k>>1]|=(k&1)?id<<4:id;
    }
  }
  for(int i=0;i<16;i++)RM.pal[i]=(uint16_t)(0x1543+i*3357);
  for(int pose=0;pose<8;pose++) {
    RM.cx=pose>5?1050:0;RM.cy=pose>5?800:0;
    RM.sc=pose&1?.38f:.6f;RM.cs=cosf(.47f*pose);RM.sn=sinf(.47f*pose);RM.pix2=pose==5;
    memset(ref,0x39,LCD_WIDTH*LCD_HEIGHT*2);RM.fb=ref;parallelRows(rasterRowsBefore,20,446);
    memset(image,0x39,LCD_WIDTH*LCD_HEIGHT*2);RM.fb=image;parallelRows(rasterRows,20,446);
    bool same=memcmp(ref,image,LCD_WIDTH*LCD_HEIGHT*2)==0;passed&=same;
    Serial.printf("RENDER PIXELS raster pose=%d exact=%d\n",pose,same);
    if(pose<2)for(int mode=0;mode<2;mode++) {
      uint32_t total=0;
      for(int i=0;i<16;i++){uint32_t at=micros();parallelRows(mode?rasterRows:rasterRowsBefore,20,446);total+=micros()-at;delay(1);}
      Serial.printf("RENDER RASTER pose=%d new=%d mean_us=%u\n",pose,mode,(unsigned)(total/16));
    }
  }
  for(int test=0;test<7;test++) {
    themeIdx=test==6?ST_MINIMAL:(test%3==0?1:test%3==1?ST_MINIMAL:0);
    renderTestX=test>=3 && test<6?-700:0;
    renderTestY=test>=3 && test<6?100:0;
    renderTestScale=test>=3 && test<6?.38f:.6f;
    renderTestAngle=test>=3 && test<6?.73f*test:.4f;
    renderTestRoute(test==6);
    memset(ref,0x39,LCD_WIDTH*LCD_HEIGHT*2);gfx->setFramebuffer(ref);renderTestRange(true);
    memset(image,0x39,LCD_WIDTH*LCD_HEIGHT*2);gfx->setFramebuffer(image);renderTestRange(false);
    bool same=memcmp(ref,image,LCD_WIDTH*LCD_HEIGHT*2)==0;passed&=same;
    Serial.printf("RENDER PIXELS route case=%d style=%d points=%d exact=%d\n",test,themeIdx,routeN,same);
    if(test<3)for(int mode=0;mode<2;mode++) {
      uint32_t total=0;
      for(int i=0;i<8;i++){memset(image,0x39,LCD_WIDTH*LCD_HEIGHT*2);uint32_t at=micros();renderTestRange(mode==0);total+=micros()-at;delay(1);}
      Serial.printf("RENDER ROUTE style=%d new=%d mean_us=%u\n",themeIdx,mode,(unsigned)(total/8));
    }
  }
  renderTestRoute();
  renderTestX=renderTestY=0;renderTestScale=.6f;renderTestAngle=.4f;
  bool lengths=true;
  for(int repeat=0;repeat<2;repeat++)for(int i=0;i<routeN-1;i++) {
    float dx=routeX[i+1]-routeX[i],dy=routeY[i+1]-routeY[i];
    float before=sqrtf(dx*dx+dy*dy),after=segLen(i);
    lengths&=memcmp(&before,&after,sizeof(float))==0;
  }
  passed&=lengths;Serial.printf("RENDER SEGMENTS exact=%d bytes=%u\n",lengths,(unsigned)sizeof(routeSegmentCache));
  volatile float sink=0;
  for(int mode=0;mode<2;mode++) {
    routeSegmentCache.clear();uint32_t at=micros();
    for(int repeat=0;repeat<200;repeat++)for(int i=400;i<550;i++) {
      if(mode)sink+=segLen(i);
      else {float dx=routeX[i+1]-routeX[i],dy=routeY[i+1]-routeY[i];sink+=sqrtf(dx*dx+dy*dy);}
    }
    Serial.printf("RENDER SEGMENTS new=%d total_us=%u sum=%d\n",mode,(unsigned)(micros()-at),(int)sink);sink=0;
  }
  // Temporary scheduling experiment; normal priority is restored before BLE begins.
  RM.pix2=false;RM.cx=RM.cy=0;RM.sc=.6f;RM.cs=1;RM.sn=0;
  if(helperTask && fbBuf[1])for(int style:{1,ST_MINIMAL}) for(int mode:{0,1,0,1}) {
    themeIdx=style;
    vTaskPrioritySet(helperTask,3);flushWait();xSemaphoreGive(flushDone);
    uint32_t at=micros();
    for(int frame=0;frame<12;frame++) {
      RM.fb=gfx->getFramebuffer();parallelRows(mode?rasterRows:rasterRowsBefore,20,446);
      renderTestRange(mode==0);
      flushAsync();
    }
    flushWait();xSemaphoreGive(flushDone);
    Serial.printf("RENDER PIPE style=%d mode=%d frame_us=%u\n",style,mode,(unsigned)((micros()-at)/12));
  }
  gfx->setFramebuffer(fbBuf[fbCur]);free(ref);
  routeN=0;routeProg=-1;routeSegmentCache.clear();themeIdx=oldTheme;nightOn=oldNight;
  rasterValid=false;rasterBackValid=false;rasW=rasH=rasBW=0;
  drawWaiting(WAIT_NOLINK);flushSync();needFlush=false;
  Serial.printf("RENDER BENCH END exact=%d internal=%u psram=%u\n",passed,
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
