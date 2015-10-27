/**
 * This is a very small example that shows how to use
 * protothreads. The program consists of two protothreads that wait
 * for each other to toggle a semaphore.
 *
 * Modified by Bruce Land to make a more compliant example
 * Aug 2014
 */

/* We must always include pt.h in our protothreads code. */
#include <plib.h>
#include "pt.h"
#include "pt-sem.h"
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
#include "pt_cornell_1_1.h"

// UART parameters
#define BAUDRATE 9600 // must match PC end
#define clrscr() printf( "\x1b[2J")
#define home()   printf( "\x1b[H")
#define pcr()    printf( '\r')
#define crlf     putchar(0x0a); putchar(0x0d);
#define backspace 0x7f // make sure your backspace matches this!
#define max_chars 32 // for input buffer
#define timer2rate 30000 //ticks per 1msec

// === thread structures ============================================
// semaphores for controlling two threads
// for guarding the UART and for allowing stread blink control
static struct pt_sem control_t1, control_t2, send_sem ;
// thread control structs
static struct pt pt1, pt2, pt3, pt4, pt_input, pt_output ;
// turn threads 1 and 2 on/off and set timing
int cntl_blink = 1 ;
static int wait_t1 = 1000 ;// mSec
// control thread 4
static int wait_t2 = 500 ;// mSec
static int run_t4 = 1 ;
// Number of ticks
volatile int capture1 = 0;
int capcount=0;
// rpm
volatile int rpm;
// macro to time a thread execution interveal
#define PT_YIELD_TIME(delay_time) \
    do { static int time_thread; \
    PT_YIELD_UNTIL(pt, milliSec >= time_thread); \
    time_thread = milliSec + delay_time ;} while(0);

// macros to manipulate a semaphore without blocking
#define PT_SEM_SET(s) (s)->count=1
#define PT_SEM_CLEAR(s) (s)->count=0
#define PT_SEM_READ(s) (s)->count
//====================================================================
// === Timer 2 interrupt handler =====================================
// ipl2 means "interrupt priority level 2"
// ASM output is 47 instructions for the ISR
volatile int milliSec ;
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
   
    // clear the interrupt flag
    mT2ClearIntFlag();
    // keep time
    milliSec++ ;
}

// estimate microSec since start up
long long uSec(void)
{
    return (long long)milliSec * 1000 + (long long)ReadTimer2()/30 ;
}

//===================== Capture ISR =============== //
void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl3) C1Handler(void) {
     capture1 = mIC1ReadCapture();
     
     rpm= (timer2rate/capture1)*(60000/7)*capcount;
         //Clear timer and sets pin 7 as input
     if (capcount>7){
        WriteTimer2(0); 
        capcount=0;
     }
     // clear the timer interrupt flag
     mIC1ClearIntFlag();
}

static PT_THREAD (protothread_rpm(struct pt *pt))
{
    PT_BEGIN(pt);
    

  PT_END(pt);
} // calculate rpm

//====================================================================
// build a string from the UART2 /////////////
//////////////////////////////////////////////
char term_buffer[max_chars];
int num_char;
int GetSerialBuffer(struct pt *pt)
{
    static char character;
    // mark the beginnning of the input thread
    PT_BEGIN(pt);
    num_char = 0;
    //memset(term_buffer, 0, max_chars);

    while(num_char < max_chars)
    {
        // get the character
        // yield until there is a valid character so that other
        // threads can execute
        PT_YIELD_UNTIL(pt, UARTReceivedDataIsAvailable(UART2));
       // while(!UARTReceivedDataIsAvailable(UART2)){};
        character = UARTGetDataByte(UART2);
        PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
        UARTSendDataByte(UART2, character);

        // unomment to check backspace character!!!
        //printf("--%x--",character );

        // end line
        if(character == '\r'){
            term_buffer[num_char] = 0; // zero terminate the string
            //crlf; // send a new line
            PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
            UARTSendDataByte(UART2, '\n');
            break;
        }
        // backspace
        else if (character == backspace){
            PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
            UARTSendDataByte(UART2, ' '); // write a blank over the previous character
            PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
            UARTSendDataByte(UART2, backspace); // go back one position
            num_char--;
            // check for buffer underflow
            if (num_char<0) {num_char = 0 ;}
        }
        else  {term_buffer[num_char++] = character ;} 
         //if (character == backspace)

    } //end while(num_char < max_size)

    // kill this input thread, to allow spawning thread to execute
    PT_EXIT(pt);
    // and indicate the end of the thread
    PT_END(pt);
}

//====================================================================
// === send a string to the UART2 ====================================
//char send_buffer[max_chars];
int num_send_chars ;
/**int PutSerialBuffer(struct pt *pt)
{
    PT_BEGIN(pt);
    num_send_chars = 0;
    while (send_buffer[num_send_chars] != 0){
        PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
        UARTSendDataByte(UART2, send_buffer[num_send_chars]);
        num_send_chars++;
    }
    // kill this output thread, to allow spawning thread to execute
    PT_EXIT(pt);
    // and indicate the end of the thread
    PT_END(pt);
}**/

// === Thread 1 ======================================================
/**
 * The first protothread function. A protothread function must always
 * return an integer, but must never explicitly return - returning is
 * performed inside the protothread statements.
 *
 * The protothread function is driven by the main loop further down in
 * the code.
 */
static PT_THREAD (protothread1(struct pt *pt))
{
    //# define wait_t1 1000 // mSec
    // mark beginning of thread
    PT_BEGIN(pt);

    /* We loop forever here. */
    while(1) {
        //stop until thread 2 signals
        
        PT_SEM_WAIT(pt, &control_t1);

        mPORTAToggleBits(BIT_0);

        // need to wait because three threads can print and send_buffer is shared
        //PT_SEM_WAIT(pt, &send_sem);
        //sprintf(send_buffer, "thread 1 %d %lld \n\r", milliSec,uSec()); //uSec()
        //PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output) );
        //PT_SEM_SIGNAL(pt, &send_sem);
       // printf("thread 1 %d %lld\n", milliSec, uSec());

         // tell thread 2 to go
        PT_SEM_SIGNAL(pt, &control_t2);
        
        // Allow thread 3 to control blinking
        PT_YIELD_UNTIL(pt, cntl_blink) ;

        // This is a locally written macro using the timer ISR
        // to program a yield time
       PT_YIELD_TIME(wait_t1) ;

      

        // never exit while
  } // END WHILE(1)

  // mark end the thread
  PT_END(pt);
} // thread 1

// === Thread 2 ======================================================
//
static PT_THREAD (protothread2(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
            
            //stop until thread 1 signals
            PT_SEM_WAIT(pt, &control_t2);
            mPORTAToggleBits(BIT_1);

            // need to wait because three threads can print and send_buffer is shared
            //PT_SEM_WAIT(pt, &send_sem);
           // sprintf(send_buffer, "thread 2 %d\n\r", milliSec);
            //PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output) );
            //PT_SEM_SIGNAL(pt, &send_sem);

            // tell thread 1 to go
            PT_SEM_SIGNAL(pt, &control_t1);

            // This is a locally written macro using the timer ISR
            // to program a yield time
            //PT_YIELD_TIME(wait_t2) ;

            

            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 2

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
            PT_SPAWN(pt, &pt_input, GetSerialBuffer(&pt_input) );
            // returns when the thead dies
            // in this case, when <enter> is pushed
            // now parse the string

             sscanf(term_buffer, "%s %d", cmd, &value);
             if (cmd[0]=='t' && cmd[1]=='1') { wait_t1 = value ;} // set the blink time
             if (cmd[0]=='t' && cmd[1]=='2') { wait_t2 = value ;} // set the blink time
             if (cmd[0] == 'g' && cmd[1]=='1') { cntl_blink = 1 ;} // make it blink
             if (cmd[0] == 's' && cmd[1]=='1') {cntl_blink = 0 ;} // make it stop
             if (cmd[0] == 'g' && cmd[1]=='2') {run_t4 = 1 ;}  // make thread 4 blink
             if (cmd[0] == 's' && cmd[1]=='2') {run_t4 = 0 ;}  // make thread 4 stop
             if (cmd[0] == 'k') {PT_EXIT(pt);} // kill this input thread (see also MAIN)
             if (cmd[0] == 'p') { // print the two blink times
                 sprintf(PT_send_buffer, "t1=%d t2=%d \n\r", wait_t1, wait_t2);
                 PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output) );
             }
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

	
	
// === Thread 4 ======================================================
// just blinks
static PT_THREAD (protothread4(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
            mPORTBToggleBits(BIT_0);
            // This is a locally written macro using the timer ISR
            // to program a yield time
            PT_YIELD_TIME(wait_t2) ;

            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 4




// === Main  ======================================================
// set up UART, timer2, threads
// then schedule them as fast as possible

int main(void)
{

  // Thread status variables
    int t1_status, t2_status, t3_status ;
  // === init the USART i/o pins =========
  PPSInput (2, U2RX, RPB11); //Assign U2RX to pin RPB11 -- Physical pin 22 on 28 PDIP
  PPSOutput(4, RPB10, U2TX); //Assign U2TX to pin RPB10 -- Physical pin 21 on 28 PDIP

  ANSELA =0; //make sure analog is cleared
  ANSELB =0;

  // === init the uart2 ===================
  UARTConfigure(UART2, UART_ENABLE_PINS_TX_RX_ONLY);
  UARTSetLineControl(UART2, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
  UARTSetDataRate(UART2, pb_clock, BAUDRATE);
  UARTEnable(UART2, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
  printf("protothreads start..\n\r");

  // ===Set up timer2 ======================
  // timer 2: on,  interrupts, internal clock, prescalar 1, toggle rate
  // run at 30000 ticks is 1 mSec
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, timer2rate);
  // set up the timer interrupt with a priority of 2
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag
  // init system time variable
  milliSec = 0;
  // setup system wide interrupts  
  INTEnableSystemMultiVectoredInt();
  
  // === set up comparator =================
    // initialize the comparator
    CMP1Open(CMP_ENABLE | CMP_OUTPUT_ENABLE | CMP1_NEG_INPUT_IVREF);
    // initialize the input capture, uses timer2
    OpenCapture1( IC_EVERY_RISE_EDGE | IC_FEDGE_RISE | IC_INT_1CAPTURE | IC_TIMER2_SRC | IC_ON);
    ConfigIntCapture1(IC_INT_ON | IC_INT_PRIOR_3 | IC_INT_SUB_PRIOR_3 );
    INTClearFlag(INT_IC1);

  // === set up i/o port pin ===============
  mPORTASetBits(BIT_0 | BIT_1 );	//Clear bits to ensure light is off.
  mPORTASetPinsDigitalOut(BIT_0 | BIT_1 );    //Set port as output
  mPORTBSetBits(BIT_0 );	//Clear bits to ensure light is off.
  mPORTBSetPinsDigitalOut(BIT_0 );    //Set port as output

    PPSInput(3, IC1, RPB13);		// Pin 24 as ic1
  
  
  // === now the threads ====================
  // init  the thread control semaphores
  PT_SEM_INIT(&control_t1, 0); // start blocked
  PT_SEM_INIT(&control_t2, 1); // start unblocked
  PT_SEM_INIT(&send_sem, 1); // start with ready to send

  // init the threads
  PT_INIT(&pt1);
  PT_INIT(&pt2);
  PT_INIT(&pt3);
  PT_INIT(&pt4);
  
  // schedule the threads
  while(1) {
    PT_SCHEDULE(protothread1(&pt1));
    PT_SCHEDULE(protothread2(&pt2));
    if (run_t4) PT_SCHEDULE(protothread4(&pt4));
    if (cmd[0] != 'k') PT_SCHEDULE(protothread3(&pt3));
  }
} // mains