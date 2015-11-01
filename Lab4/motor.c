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
#define timer2rate 625000 //ticks per 1sec
#define max_rpm 3000 //RPM
static struct pt pt_timer, pt3, pt_PID, pt_input, pt_output;
char buffer[60];

// Number of ticks
volatile unsigned int capture1 = 0;
volatile int capcount=1;
volatile double s, p, i, d;
volatile int error[2] = {0,0};
volatile int error_integral=0;
// rpm
 volatile unsigned long rpm = 0;
//The actual period of the wave
int generate_period = 16383 ;
int pwm_on_time = 0 ;
int pwm_on_time2=0;


int timeElapsed =0 ;

//====================================================================


//===================== Capture ISR =============== //
void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl3) C1Handler(void) {
     capture1 = capture1 + mIC1ReadCapture();
     WriteTimer2(0); 
      

         //Clear timer and sets pin 7 as input
  
     
     capcount++;

     // clear the timer interrupt flag
     mIC1ClearIntFlag();
}
static PT_THREAD (protothread_PID (struct pt *pt))
{
     PT_BEGIN(pt);
     while(1) {
         if(capture1 != 0)
            rpm= 0.3*rpm + 0.7*(double)((timer2rate*60*capcount)/(capture1*7));  
         else
             rpm = rpm;
        capcount=0;
        capture1 = 0;
        
        // Calculate the error
        error[2] = error[1];
        error[1] = s - rpm;
        
        if(error[1]*error[2] >= 0)
            error_integral = error_integral + error[2];
        else
            error_integral = 0;
        
        double fix = p*error[2] + i*error_integral + d*(error[2]-error[1]);
        
        //ensure fix is within the bounds
        if (fix < 0)
            fix = 0;
        else if (fix > max_rpm)
            fix = (double)max_rpm;
        
        pwm_on_time = (int)(generate_period*fix)/(max_rpm);
        //Calculate the new PWM
        //pwm_on_time+=100;
        if (pwm_on_time > generate_period) pwm_on_time = 0;
        SetDCOC3PWM(pwm_on_time);
        pwm_on_time2= (int)(generate_period*rpm)/(max_rpm);
        SetDCOC2PWM(pwm_on_time2);
        PT_YIELD_TIME_msec(10);
     }
     PT_END(pt);
}
//==================== Calculate ===================== //
static PT_THREAD (protothread_timer (struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
        // yield time 1 second
        
        int minutes = timeElapsed/60;
        int seconds = (timeElapsed)%60;
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
static double value;
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

             sscanf(PT_term_buffer, "%s %f", cmd, &value);
             if (cmd[0] == 's') { s = value; }
             if (cmd[0] == 'p') { p = value; }
             if (cmd[0] == 'i') { i = value; }
             if (cmd[0] == 'd') { d = value; }
             if (cmd[0] == 'l' && cmd[1] == 's') { // print the two blink times
                 sprintf(PT_send_buffer, "S:%f P:%f I:%f D:%f \n\r",s,p,i,d);
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
    
    //default
    s = 0;
    p = 10;
    i = 2;
    d = 5.2;
    
// === init the USART i/o pins =========
  PPSInput (2, U2RX, RPB11); //Assign U2RX to pin RPB11 -- Physical pin 22 on 28 PDIP
  PPSOutput(4, RPB10, U2TX); //Assign U2TX to pin RPB10 -- Physical pin 21 on 28 PDIP
mPORTASetBits(BIT_0);
      mPORTASetPinsDigitalOut(BIT_0);
    
    

  // === init the uart2 ===================
  UARTConfigure(UART2, UART_ENABLE_PINS_TX_RX_ONLY);
  UARTSetLineControl(UART2, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
  UARTSetDataRate(UART2, pb_clock, BAUDRATE);
  UARTEnable(UART2, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
 
  printf("protothreads start..\n\r");
      // setup system wide interrupts  
  INTEnableSystemMultiVectoredInt();
  // ===Set up timer2 ======================
  // timer 2: on,  interrupts, internal clock, prescalar 1, toggle rate
  // run at 30000 ticks is 1 mSec
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_64, 0xffff);
  // set up the timer interrupt with a priority of 2
 //ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag


  // === Config timer and output compares to make pulses ========
  // set up timer3 to generate the wave period
  OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, generate_period);
  //ConfigIntTimer3(T3_INT_OFF | T3_INT_PRIOR_3);
  mT3ClearIntFlag(); // and clear the interrupt flag
  
  
// set up compare3 for PWM mode
  OpenOC3(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE , pwm_on_time, pwm_on_time); //
  // OC3 is PPS group 4, map to RPB9 (pin 18)
  PPSOutput(4, RPB9, OC3);
 // mPORTASetPinsDigitalOut(BIT_3);    //Set port as output -- not needed


  
// set up compare2 for PWM mode
  OpenOC2(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE , pwm_on_time2, pwm_on_time2); //
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(2, RPB5, OC2);
 // mPORTASetPinsDigitalOut(BIT_3);    //Set port as output -- not needed



    // initialize the input capture, uses timer2
    OpenCapture1( IC_EVERY_FALL_EDGE | IC_FEDGE_FALL | IC_INT_1CAPTURE | IC_CAP_16BIT  |IC_TIMER2_SRC | IC_ON);
    ConfigIntCapture1(IC_INT_ON | IC_INT_PRIOR_3 | IC_INT_SUB_PRIOR_3 );
    INTClearFlag(INT_IC1);
  
    PPSInput(3, IC1, RPB13);		// Pin 24 as ic1
    
    PT_setup();
       
    // initialize the threads
    PT_INIT(&pt_timer);
    PT_INIT(&pt3);
    PT_INIT(&pt_PID);
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
        PT_SCHEDULE(protothread_PID((&pt_PID)));
    }
    
} //main

