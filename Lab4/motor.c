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

#include "pt_cornell_1_2_1.h"

// UART parameters
#define BAUDRATE 9600 // must match PC end
#define clrscr() printf( "\x1b[2J")
#define home()   printf( "\x1b[H")
#define pcr()    printf( '\r')
#define crlf     putchar(0x0a); putchar(0x0d);
#define backspace 0x7f // make sure your backspace matches this!
#define max_chars 32 // for input buffer
#define timer2rate 625000 //ticks per 1msec

static struct pt pt_timer, pt3, pt_input, pt_output;
char buffer[60];

// Number of ticks
volatile unsigned int capture1 = 0;
volatile int capcount=1;
// rpm
 int rpm;

int score = 0;
int timeElapsed =0 ;

//====================================================================
// === Timer 2 interrupt handler =====================================
// ipl2 means "interrupt priority level 2"
// ASM output is 47 instructions for the ISR




//===================== Capture ISR =============== //
void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl3) C1Handler(void) {
     capture1 = mIC1ReadCapture();
      //WriteTimer2(0); 
      

         //Clear timer and sets pin 7 as input
  
     
     capcount++;

     // clear the timer interrupt flag
     mIC1ClearIntFlag();
}

//==================== Calculate ===================== //
static PT_THREAD (protothread_timer (struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
        // yield time 1 second
        
          
        rpm= (timer2rate/capture1)*(60/7)*capcount;  
        WriteTimer2(0); 
        capcount=0;
        int minutes = timeElapsed/60;
        int seconds = timeElapsed%60;
        // draw sys_time
        tft_fillRoundRect(0,10, 320, 64, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(2);
        sprintf(buffer,"%02d:%02d rpm: %d", minutes,seconds, rpm);
        tft_writeString(buffer);
        
        PT_YIELD_TIME_msec(1000);
        timeElapsed++ ;
        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
}

// === Thread 3 ======================================================
//
static char cmd[16];
static int value;
static PT_THREAD (protothread3(struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
            // send the prompt
            sprintf(PT_send_buffer,"cmd>");
            PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output) );
          //spawn a thread to handle terminal input
            // the input thread waits for input
            // -- BUT does NOT block other threads
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );
            // returns when the thead dies
            // in this case, when <enter> is pushed
            // now parse the string

             sscanf(PT_term_buffer, "%s %d", cmd, &value);
             
             if (cmd[0] == 'p') { // print the two blink times
                 sprintf(PT_send_buffer, "This is a test \n\r");
                 PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output) );
             }
            // never exit while
            //PT_YIELD_TIME_msec(100);
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

// === ADC Thread =============================================
// 

//===================== Main ======================= //
void main(void) {
	SYSTEMConfig(sys_clock, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

// === init the USART i/o pins =========
  PPSInput (2, U2RX, RPB11); //Assign U2RX to pin RPB11 -- Physical pin 22 on 28 PDIP
  PPSOutput(4, RPB10, U2TX); //Assign U2TX to pin RPB10 -- Physical pin 21 on 28 PDIP

    

  // === init the uart2 ===================
  UARTConfigure(UART2, UART_ENABLE_PINS_TX_RX_ONLY);
  UARTSetLineControl(UART2, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
  UARTSetDataRate(UART2, pb_clock, BAUDRATE);
  UARTEnable(UART2, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
 
  printf("protothreads start..\n\r");
    
  // ===Set up timer2 ======================
  // timer 2: on,  interrupts, internal clock, prescalar 1, toggle rate
  // run at 30000 ticks is 1 mSec
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_64, 0xffffffff);
  // set up the timer interrupt with a priority of 2
 // ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag

  // setup system wide interrupts  
  INTEnableSystemMultiVectoredInt();
  
  // === set up comparator =================

    // initialize the input capture, uses timer2
    OpenCapture1( IC_EVERY_FALL_EDGE | IC_FEDGE_FALL | IC_INT_1CAPTURE | IC_CAP_32BIT  |IC_TIMER2_SRC | IC_ON);
    ConfigIntCapture1(IC_INT_ON | IC_INT_PRIOR_3 | IC_INT_SUB_PRIOR_3 );
    INTClearFlag(INT_IC1);
  
    PPSInput(3, IC1, RPB13);		// Pin 24 as ic1
    
    PT_setup();
       
    // initialize the threads
    PT_INIT(&pt_timer);
    PT_INIT(&pt3);
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    
    INTEnableSystemMultiVectoredInt();

    tft_setRotation(1); //240x320 horizontal display
  
    //round-robin scheduler for threads
    while(1) {
        PT_SCHEDULE(protothread_timer(&pt_timer));
        PT_SCHEDULE(protothread3(&pt3));
    }
    
} //main

