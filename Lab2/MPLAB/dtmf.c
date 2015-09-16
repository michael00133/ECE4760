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
#include <stdint.h>

// Threading Library
// config.h sets SYSCLK 40 MHz
#define SYS_FREQ 40000000
#include "pt_cornell_TFT.h"
#define DAC_config_chan_A 0b0011000000000000

static struct pt pt_blink, pt_keyboard, pt_dds;
volatile unsigned int DAC_data ;// output value
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this DAC
char buffer[60];

//stores 12 digits
volatile char storage[12];
volatile int temp;
volatile int num;

//look up table for sinusoid
uint8_t table [256];

// Debouncing State
volatile int state = 0;   

// Timer Periods and table look up indices
volatile int f1, f2, ind1, ind2;
int fs = 10000;
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
    static int keypad, i, pattern;
    // order is 0 thru 9 then * ==10 and # ==11
    // no press = -1
    // table is decoded to natural digit order (except for * and #)
    // 0x80 for col 1 ; 0x100 for col 2 ; 0x200 for col 3
    // 0x01 for row 1 ; 0x02 for row 2; etc
    static int keytable[12]={0x108, 0x81, 0x101, 0x201, 0x82, 0x102, 0x202, 0x84, 0x104, 0x204, 0x88, 0x208};
    // init the keypad pins A0-A3 and B7-B9
    // PortA ports as digital outputs
    mPORTASetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2 | BIT_3);    //Set port as output
    // PortB as inputs
    mPORTBSetPinsDigitalIn(BIT_7 | BIT_8 | BIT_9);    //Set port as input

      while(1) {

        // read each row sequentially
        mPORTAClearBits(BIT_0 | BIT_1 | BIT_2 | BIT_3);
        pattern = 1; mPORTASetBits(pattern);
        
        // yield time
        PT_YIELD_TIME_msec(30);
   
        for (i=0; i<4; i++) {
            keypad  = mPORTBReadBits(BIT_7 | BIT_8 | BIT_9);
            if(keypad!=0) {keypad |= pattern ; break;}
            mPORTAClearBits(pattern);
            pattern <<= 1;
            mPORTASetBits(pattern);
        }
        
        state = debounce(state,keypad);
        
        // search for keycode
        if (keypad > 0 && state == 3){ // then button is pushed
            for (i=0; i<12; i++){
                if (keytable[i]==keypad) break;
            }
        }
        else i = -1; // no button pushed

        // draw key number
        tft_fillRoundRect(5,200, 300, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(5, 200);
        tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(2);
        if(i > -1 && i < 10) {
            sprintf(buffer,"Number Pressed: %d", i);
            temp = i;
        }
        else {
            sprintf(buffer,"No Button Pressed");
            temp = -1;
        }
        if (i==10) {
            sprintf(buffer,"*");
            temp = i;
        }
        if (i==11) { 
            sprintf(buffer,"#");
            temp = i;
        }
        
        tft_writeString(buffer);

        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // Keyboard

//===================== DDS ======================== //
// sets the DDS parameters and communicates to the DAC
static PT_THREAD (protothread_dds(struct pt *pt))
{
    PT_BEGIN(pt);
    while (1) {
         PT_YIELD_TIME_msec(65);
         if(temp == -1 && state == 0) {
             f1 = f2 = 0;
         }
         else if(temp != -1) {
             if(temp == 3 || temp == 6 || temp == 9 || temp == 11)
                 f1 = 1477;
             else if(temp == 2 || temp ==  5 || temp == 8 || temp == 0)
                 f1 = 1336;
             else if(temp == 1 || temp == 4 || temp == 7 || temp == 10)
                 f1 = 1209;
             else
                 f1 = 2000;
             
             if(temp == 1 || temp == 2 || temp == 3)
                 f2 = 697;
             else if(temp == 4 || temp == 5 || temp == 6)
                 f2 = 770;
             else if(temp == 7 || temp == 8 || temp == 9)
                 f2 = 852;
             else if(temp == 10 || temp == 0 || temp == 11)
                 f2 = 941;
             
             ind1 = 0; ind2 = 0;
         }
    }
    PT_END(pt);
} //DDS
//== Timer 3 interrupt handler ===========================================

void __ISR(_TIMER_3_VECTOR, ipl3) Timer3Handler(void){
    mT3ClearIntFlag();
    // generate  ramp
    if (f1 != 0 || f2 != 0) {
        DAC_data = 0.5*(table[ind1] + table[ind2]); // for testing
        ind1 = (ind1 + (256*f1)/fs)%256;
        ind2 = (ind2 + (256*f2)/fs)%256;
    }
    else {
        DAC_data = 0.8*DAC_data;
    }
    // CS low to start transaction
     mPORTBClearBits(BIT_4); // start transaction
    // test for ready
     while (TxBufFullSPI2());
     // write to spi2
     WriteSPI2(DAC_config_chan_A | DAC_data);
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
     // CS high
     mPORTBSetBits(BIT_4); // end transaction
}

//===================== Main ======================= //
void main(void) {
    SYSTEMConfigPerformance(PBCLK);

    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    // initialize timer periods as zero
    f1 = 0; f2 = 0;
    ind1 = 0; ind2 = 0;
    temp = NULL;
    
    num = 0;
    
    int i;
    for(i = 0; i < 256; i++) {
        table[i] = (uint8_t)pow(2,8)*0.5*(1.0+cos(2*3.14159 * ((double)i/256.0)));
    }

    PT_setup();
    
    // initialize the threads
    PT_INIT(&pt_blink);
    PT_INIT(&pt_keyboard);
    PT_INIT(&pt_dds);
 
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);

    // initialize timer3
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, SYS_FREQ/fs);
    ConfigIntTimer3(T3_INT_ON | T3_INT_PRIOR_3);
    mT3ClearIntFlag();

    // initialize MOSI
    PPSOutput(2, RPB5, SDO2);			// MOSI for DAC
    mPORTBSetPinsDigitalOut(BIT_4);		// CS for DAC
    mPORTBSetBits(BIT_4);              // initialize CS as high
    
    // divide Fpb by 2, configure the I/O ports. Not using SS in this example
        // 16 bit transfer CKP=1 CKE=1
        // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
        // For any given peripherial, you will need to match these
    SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
    
    INTEnableSystemMultiVectoredInt();

    tft_setRotation(0); //240x320 vertical display
  
    //round-robin scheduler for threads
    while(1) {
        PT_SCHEDULE(protothread_blink(&pt_blink));
        PT_SCHEDULE(protothread_keyboard(&pt_keyboard));
        PT_SCHEDULE(protothread_dds(&pt_dds));
    }
    
} //main

int debounce(int s, int keypad) {
            switch(s){
            case 0:         // button is not pushed
                if(keypad == 0) {
                    s = 0;
                }
                else {
                    s = 2;
                }
                break;
            case 1:        // button is possibly not pushed
                if(keypad == 0) {
                    s = 0;
                }
                else {
                    s = 3;
                }
                break;
            case 2:      // button is possibly pushed
                if(keypad == 0)
                    s = 0;
                else
                    s = 3;
                break;
            case 3:      // button is pushed
                if(keypad == 0) {
                    s = 1;
                }
                else {
                    s = 3;
                }
                break;
            default:
                s = 0;
        }
            return s;
}