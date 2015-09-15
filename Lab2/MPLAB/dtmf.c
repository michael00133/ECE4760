/*
 * File:	dtmf.c
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

// Threading Library
// config.h sets SYSCLK 64 MHz
#define SYS_FREQ 64000000
#include "pt_cornell_TFT.h"

static struct pt pt_blink, pt_keyboard, pt_dds;

int p1, p2;
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

//==================== Keyboard ==================== //
// handles the keypad state machine and debounce logic
static PT_THREAD (protothread_keyboard(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
	PT_YIELD_TIME_msec(30);
    }
    PT_END(pt);
} // Keyboard

//===================== DDS ======================== //
// sets the DDS parameters and communicates to the DAC
static PT_THREAD (protothread_dds(struct pt *pt))
{
    PT_BEGIN(pt);
    while (1) {
    }
    PT_END(pt);
} //DDS
//===================== Main ======================= //
void main(void) {
    SYSTEMConfigPerformance(PBCLK);

    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    // initialize timer periods as zero
    p1 = 0; p2 = 0;

    // initialize the SPI channel
    SpiChannel spiChn=SPI_CHANNEL2;

    PT_setup();
    INTEnableSystemMultiVectoredInt();

    
    // initialize the threads
    PT_INIT(&pt_blink);
    PT_INIT(&pt_keyboard);
    PT_INIT(&pt_dds);
 
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);

    // initialize timer2 and timer3
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_64, p1);
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_3);
    DisableIntT2;

    OpenTimer3(T3_ON | T2_SOURCE_INT | T3_PS_1_64, p2);
    ConfigIntTimer3(T3_INT_ON | T2_INT_PRIOR_4);
    DisableIntT3;

    // initialize MOSI
    PPSOutput(2, RPB5, SDO2);			// MOSI for DAC
    mPORTBSetPinsDigitalOut(BIT_4);		// CS for DAC

    tft_setRotation(0); //240x320 vertical display

  
    //round-robin scheduler for threads
    while(1) {
        PT_SCHEDULE(protothread_blink(&pt_blink));
        PT_SCHEDULE(protothread_keyboard(&pt_keyboard));
    }
    
} //main
