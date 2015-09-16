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
volatile int storage[12];
volatile int dds_state;
volatile int recent;
volatile int num;
volatile int repeat;
volatile double ramp;
volatile int downup;

//look up table for sinusoid
uint16_t table [256];

// Debouncing State
volatile int state = 0;   

// Timer Periods and table look up indices
volatile int f1, f2, inc1, inc2;
volatile uint32_t ind1, ind2;
int fs = 10000;
int inc_const;
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
            sprintf(buffer,"Num: %d, %d, %d", i, num, dds_state);
        }
        else {
            sprintf(buffer,"No Button Pressed");
        }
        if (i==10) {
            sprintf(buffer,"*");
        }
        if (i==11) { 
            sprintf(buffer,"#");
        }
        recent = i;
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
         PT_YIELD_TIME_msec(30);
         if(recent != -1 && f1 == 0 && f2 == 0) {
             
             if(recent == 3 || recent == 6 || recent == 9 || recent == 11)
                 f1 = 1477;
             else if(recent == 2 || recent ==  5 || recent == 8 || recent == 0)
                 f1 = 1336;
             else if(recent == 1 || recent == 4 || recent == 7 || recent == 10)
                 f1 = 1209;
             
             if(recent == 1 || recent == 2 || recent == 3)
                 f2 = 697;
             else if(recent == 4 || recent == 5 || recent == 6)
                 f2 = 770;
             else if(recent == 7 || recent == 8 || recent == 9)
                 f2 = 852;
             else if(recent == 10 || recent == 0 || recent == 11)
                 f2 = 941;

             
             inc1 = f1 * inc_const;
             inc2 = f2 * inc_const;
             
             if (recent == 10)
                 dds_state = 1;
             else if (recent == 11){
                 dds_state = 2;
                 repeat = 0;
             }
             
             if (num < 11 && recent != 11 && recent != 10) {
                num ++;
                storage[num] = recent;
             }
             PT_YIELD_TIME_msec(5);
             //signal to ramp
             downup = 1;
             ramp = 0;
         }
         else if (state == 0 && dds_state == 1) {
             num = -1;
             dds_state = 0;
         }
         else if (state == 0 && dds_state == 2) {
             //signal to decay
             downup = -1;
             ramp = 1;
             PT_YIELD_TIME_msec(5);
             f1 = 0; f2 = 0;
             PT_YIELD_TIME_msec(65);
             if(repeat <= num && num != -1) {
                if(storage[repeat] == 3 || storage[repeat] == 6 || storage[repeat] == 9 || storage[repeat] == 11)
                    f1 = 1477;
                else if(storage[repeat] == 2 || storage[repeat] ==  5 || storage[repeat] == 8 || storage[repeat] == 0)
                    f1 = 1336;
                else if(storage[repeat] == 1 || storage[repeat] == 4 || storage[repeat] == 7 || storage[repeat] == 10)
                    f1 = 1209;

                if(storage[repeat] == 1 || storage[repeat] == 2 || storage[repeat] == 3)
                    f2 = 697;
                else if(storage[repeat] == 4 || storage[repeat] == 5 || storage[repeat] == 6)
                    f2 = 770;
                else if(storage[repeat] == 7 || storage[repeat] == 8 || storage[repeat] == 9)
                    f2 = 852;
                else if(storage[repeat] == 10 || storage[repeat] == 0 || storage[repeat] == 11)
                    f2 = 941;
                
                 inc1 = f1 * inc_const;
                 inc2 = f2 * inc_const;
                 repeat++;
             PT_YIELD_TIME_msec(5);
             //signal to ramp
             downup = 1;
             ramp = 0;
             }
             else {
                 num = -1;
                 dds_state = 0;
             }
             PT_YIELD_TIME_msec(80);
         }
         else if(recent == -1 && state == 0) {
             //signal to decay
             downup = -1;
             ramp = 1;
             PT_YIELD_TIME_msec(5);
             f1 = f2 = 0;
         }
    }
    PT_END(pt);
} //DDS
//== Timer 3 interrupt handler ===========================================

void __ISR(_TIMER_3_VECTOR, ipl3) Timer3Handler(void){
    mT3ClearIntFlag();
    // generate  ramp
    if (f1 != 0 || f2 != 0) {
        uint8_t t1, t2;
        t1 = (ind1>>24);
        t2 = (ind2>>24);
        DAC_data = (int)(ramp*0.5*(table[t1] + table[t2])); // for testing
        ind1 += inc1;
        ind2 += inc2;
    
    }
    else {
        DAC_data = 0.99*DAC_data;
    }
    
    if(ramp < 1 && downup == 1)
        ramp += 200.0/((double)fs);
    else if(ramp > 0 && downup == -1)
        ramp -= 200.0/((double)fs);
    else if(downup == 1)
        ramp = 1;
    else
        ramp = 0;
    
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
    recent = -1;
    inc_const = (256*pow(2,24))/fs;
    num = -1;
    downup = 0;
    
    int i;
    for(i = 0; i < 256; i++) {
        table[i] = (uint16_t)pow(2,11)*(1.0+cos(2*3.14159 * ((double)i/256.0)));
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

/*int* calc_f(int t) {
    int f_calc [2];
             if(t == 3 || t == 6 || t == 9 || t == 11)
                 f_calc[0] = 1477;
             else if(t == 2 || t ==  5 || t == 8 || t == 0)
                 f_calc[0] = 1336;
             else if(t == 1 || t == 4 || t == 7 || t == 10)
                 f_calc[0] = 1209;
             
             if(t == 1 || t == 2 || t == 3)
                 f_calc[1] = 697;
             else if(t == 4 || t == 5 || t == 6)
                 f_calc[1] = 770;
             else if(t == 7 || t == 8 || t == 9)
                 f_calc[1] = 852;
             else if(t == 10 || t == 0 || t == 11)
                 f_calc[1] = 941;
             return f_calc;
}*/