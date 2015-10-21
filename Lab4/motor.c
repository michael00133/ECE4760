/*
 * File:	motor.c
 * Author:	Michael Nguyen
 *              Tri Hoang
 *              Elias Wang
 * Target PIC:	PIC32MX250F128B
 */

// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"

//extra libraries and defines
#include <math.h>
#include <stdint.h>

// Threading Library
// config.h sets SYSCLK 40 MHz
#define SYS_FREQ 40000000
#include "pt_cornell_TFT.h"

static struct pt pt_refresh;
char buffer[60];
volatile int timeElapsed = 0;

//======================= Refresh ========================= //
//Does Ball calculations and Draws the necessary elements on the screen 
static PT_THREAD (protothread_refresh(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
        int minutes = timeElapsed/60;
        int seconds = timeElapsed%60;
        // draw sys_time
        tft_fillRoundRect(0,10, 320, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(2);
        sprintf(buffer,"Time Elapsed: %02d:%02d", minutes,seconds);
        tft_writeString(buffer);
        PT_YIELD_TIME_msec(1000);
        timeElapsed++ ;
    }
    PT_END(pt);
} // blink

//===================== Main ======================= //
void main(void) {
    //SYSTEMConfigPerformance(PBCLK);
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    PT_setup();
            
    // initialize the threads
   // PT_INIT(&pt_calculate);
    PT_INIT(&pt_refresh);
   // PT_INIT(&pt_adc);
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    
    INTEnableSystemMultiVectoredInt();

    tft_setRotation(0); //240x320 horizontal display
  
    //round-robin scheduler for threads
    while(1) {
      //  PT_SCHEDULE(protothread_calculate(&pt_calculate));
        PT_SCHEDULE(protothread_refresh(&pt_refresh));
       // PT_SCHEDULE(protothread_adc(&pt_adc));
    }
    
} //main

