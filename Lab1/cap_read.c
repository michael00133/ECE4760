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
short capture1 = 0;

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
// Discharges and begins measurement of C1INA
static PT_THREAD (protothread_capture(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
    // sets pin 7 to an output
    PPSOutput(1, RPB3, C1INA);  
    PT_YIELD_TIME_msec(1);

    //begin timer and sets pin 7 as input
    PPSInput(1, C1INA, RPB3);
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_32, 0xffffffff);
    capture1 = 0;

    //reset the timer somehow here?
    CloseTimer2();	
    }
    PT_END(pt);
} // capture

//===================== Capture ISR =============== //
void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl3) C1Handler(void)
{
     capture1 = mIC1ReadCapture();
     // clear the timer interrupt flag
     mIC1ClearIntFlag();
}

//===================== Main ======================= //
void main(void) {
    SYSTEMConfigPerformance(PBCLK);

    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    PT_setup();
    INTEnableSystemMultiVectoredInt();

    // initialize the threads
    PT_INIT(&pt_blink);
    PT_INIT(&pt_capture);
    
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);

    tft_setRotation(0); //240x320 vertical display

    // initialize the comparator
    CMP1Open(CMP_ENABLE | CMP_OUTPUT_ENABLE | CMP1_NEG_INPUT_IVREF);

    // initialize the input capture, uses timer2
    OpenCapture1( IC_EVERY_RISE_EDGE | IC_INT_1CAPTURE | IC_TIMER2_SRC | IC_ON);
    ConfigIntCapture1(IC_INT_ON | IC_INT_PRIOR_3 | IC_INT_SUB_PRIOR_3 );
    INTClearFlag(INT_IC1);

    // initialize the input/output I/O
    PPSOutput(1, RPB3, C1INA);		//initially an output
    PPSOutput(4, RPB9, C1OUT);		//set up output of comparator for debugging
    PPSInput(3, IC1, RPB2);		//Either Pin 6 or Pin 24 idk
   
    //round-robin scheduler for threads
    while(1) {
	PT_SCHEDULE(protothread_blink(&pt_blink));
	PT_SCHEDULE(protothread_capture(&pt_capture));
    }
    
} //main
