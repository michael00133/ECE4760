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
// config.h sets SYSCLK 64 MHz
#define SYS_FREQ 64000000
#include "pt_cornell_TFT.h"

static struct pt pt_blink, pt_capture;

//======================= Blink ========================= //
// Blinks a circle on the screen at a rate of 1 blink per second
static PT_THREAD (protothread_blink(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
	
	PT_YIELD_TIME_msec(1000);
    	tft_drawCircle(10, 10, 10, ILI9340_WHITE);

	PT_YIELD_TIME_msec(1000);
	tft_drawCircle(10, 10, 10, ILI9340_BLACK); 
   }
    PT_END(pt);
} // blink

//===================== Capture ==================== //
// Discharges and begins capture
static PT_THREAD (protothread_capture(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
	
    }
    PT_END(pt);
} // capture

//===================== Main ======================= //
void main(void) {
    SYSTEMConfigPerformance(PBCLK);

    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    PT_setup();
    INTEnableSystemMultiVectoredInt();

    // initialize the threads
    PT_INIT(&pt_blink);
    
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);

    tft_setRotation(0); //240x320 vertical display

    // initialize the comparator
    CMP1Open(CMP_ENABLE | CMP_OUTPUT_ENABLE | CMP1_NEG_INPUT_IVREF);

    // initialize the input/output I/O
    PPSOutput(1, RPB3, C1INA);		//initially an output
    PPSOutput(4, RPB9, C1OUT);		//set up output of comparator for debugging
    //round-robin scheduler for threads
    while(1) {
	PT_SCHEDULE(protothread_blink(&pt_blink));
    }
    
} //main
