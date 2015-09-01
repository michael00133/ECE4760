/*
 * File:	main.c
 * Author:	Michael Nguyen
 * 		Tri Hoang
 * 		Elias Wang
 * Target PIC:	PIC32MX250F128B
 */

// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"

// Threading Library
// config.h sets 40 MHz
#define SYS_FREQ 40000000
#include "pt_cornell_TFT.h"

static struct pt pt_blink;

//======================= Blink ========================= //
// Blinks a circle on the screen at a rate of 1 blink per second
static PT_THREAD (protothread_blink(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
    }
    PT_END(pt);
}

//===================== Main ======================= //
void main(void) {
    SYSTEMConfigPerformance(PBCLK);

    PT_setup();
    INTEnableSystemMultiVectoredInt();

    // initialize the threads
    PT_INIT(&pt_blink);
    
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);

    tft_setRotation(0); //240x320 vertical display

    //round-robin scheduler for threads
    while(1) {
	PT_SCHEDULE(protothread_blink(&pt_blink));
    }
    
} //end of main
