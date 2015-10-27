/*
 * File:	motor.c
 * Author:	Michael Nguyen
 * 		Tri Hoang
 * 		Elias Wang
 * Target PIC:	PIC32MX250F128B
 */

#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
#include "pt_cornell_1_2_1.h"
/**
// UART parameters
#define BAUDRATE 9600 // must match PC end
#define clrscr() printf( "\x1b[2J")
#define home()   printf( "\x1b[H")
#define pcr()    printf( '\r')
#define crlf     putchar(0x0a); putchar(0x0d);
#define backspace 0x7f // make sure your backspace matches this!
#define max_chars 32 // for input buffer
**/

char buffer[60];
int timeElapsed = 0;

// === thread structures ============================================

// thread control structs
static struct pt pt2, pt3, pt_input, pt_output ;

// === Thread 2 ======================================================
//
static PT_THREAD (protothread2(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
        int minutes = timeElapsed/60;
        int seconds = timeElapsed%60;
        // draw sys_time
        tft_fillRoundRect(0,10, 320, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(2);
        sprintf(buffer,"%02d:%02d", minutes,seconds);
        tft_writeString(buffer);
        timeElapsed++ ;
        PT_YIELD_TIME_msec(1000);
      } // END WHILE(1)
  PT_END(pt);
} // thread 2
/**
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
**/

// === Main  ======================================================
// set up UART, timer2, threads
// then schedule them as fast as possible

int main(void)
{
  SYSTEMConfig( sys_clock,  SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
  ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;
  /** // === init the USART i/o pins =========
  PPSInput (2, U2RX, RPB11); //Assign U2RX to pin RPB11 -- Physical pin 22 on 28 PDIP
  PPSOutput(4, RPB10, U2TX); //Assign U2TX to pin RPB10 -- Physical pin 21 on 28 PDIP

    

  // === init the uart2 ===================
  UARTConfigure(UART2, UART_ENABLE_PINS_TX_RX_ONLY);
  UARTSetLineControl(UART2, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
  UARTSetDataRate(UART2, pb_clock, BAUDRATE);
  UARTEnable(UART2, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
   * **/
  printf("protothreads start..\n\r");

  INTEnableSystemMultiVectoredInt();

  // init the threads
  PT_INIT(&pt2);
  //PT_INIT(&pt3);
  
      // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);

    tft_setRotation(0); //240x320 vertical display
  
  // schedule the threads
  while(1) {
    PT_SCHEDULE(protothread2(&pt2));
   // PT_SCHEDULE(protothread3(&pt3));
  }
} // mains