/*
 * File:	final.c
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




#define order 3 //order of nlms filter
#define mu 7*pow(10,-16)       //stepsize


static struct pt pt_refresh;

volatile unsigned int DAC_data ;// output value
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this DAC

int fs=16000; //sampling rate for ADC
char buffer[60];
//ADC value
 int adc_9;
int timeElapsed =1;
 
 //primary in pin 24(muxA) ref in pin 7(muxB)
 int ref[order];
 int primary;

 float weights[order];
 int desired;
int i;

// Equalizer data structs
#define equal_order 18
#define fixed_point 64
int desired_buf[equal_order];
int weights_eq[5];
int fb_tot[equal_order];
int fb0[equal_order] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int fb1[equal_order] = {-57,	98,	76,	76,	84,	93,	102, 109, 112, 112,	109, 102, 93, 84, 76, 76, 98, -57};
int fb2[equal_order] = {-63, -152, -35, -73, -24, 5, 49, 80,	100, 100, 80, 49, 5, -24, -73, -35, -152, -63};
int fb3[equal_order] = {135,	14,	-28,	-77,	-104,	-90,	-36,	30,	76,	76,	30,	-36,	-90,	-104,	-77,	-28,	14,	135};
int fb4[equal_order] = {-63,	104,	95,	-178,	-36,	-442,	386,	386,	-442,	-36,	-178,	95,	104,	-63,	0,	0,	0,	0};
int  innerproduct_eq(int* a, int* b);
void update_eq(int* a, int b);
void update_fb(int* a, int b);

int  innerproduct(int* a, int* b);
float  innerproductf(int* a, float* b);
void update(int* array, int new);


//==================== Calculate ===================== //
static PT_THREAD (protothread_refresh (struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
        // yield time 1 second
        
        
        int minutes = timeElapsed/60;
        int seconds = timeElapsed%60;
        // draw sys_time
        tft_fillRoundRect(0,10, 320, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(2);
        sprintf(buffer,"%02d:%02d",minutes,seconds);
        tft_writeString(buffer);
        
        PT_YIELD_TIME_msec(1000);
        timeElapsed++ ;
        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
}


void __ISR(_TIMER_3_VECTOR, ipl3) Timer3Handler(void){
    mT3ClearIntFlag();
    primary=((ReadADC10(0)));
    int temp=(ReadADC10(1));
    update(ref,temp);
    AcquireADC10(); // not needed if ADC_AUTO_SAMPLING_ON below
 
}

void __ISR(_TIMER_4_VECTOR, ipl2) Timer4Handler(void){
    mT4ClearIntFlag();
    //LMS filter 
   desired=(int)(primary-(innerproductf(ref,weights)));
   
   update_eq(desired_buf,desired);
    for(i=0;i<order;i++) {
        
       weights[i]=weights[i]+(float)(mu*ref[i]*desired);
        
    }
	//equalizer
   int temp=innerproduct_eq(fb_tot,desired_buf)/fixed_point;
   //check range to prevent clipping
   if (temp>0 && temp <4096) {
    DAC_data=temp;
   }
   //DAC_data=(int)(innerproductf(ref,weights));
    // CS low to start transaction
     mPORTBClearBits(BIT_4); // start transaction
    // test for ready
     while (TxBufFullSPI2());
     // write to spi2 
     WriteSPI2(DAC_config_chan_A | DAC_data); //data output pin 14
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
     // CS high
     mPORTBSetBits(BIT_4); // end transaction
}


//===================== Main ======================= //
void main(void) {
    //SYSTEMConfigPerformance(PBCLK);
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    ANSELA = 0; ANSELB = 0; // Disable analog inputs
    CM1CON = 0; CM2CON = 0; CM3CON = 0;     // Disable analog comparators

    PT_setup();
    
   
    int weights_eq[5] = {fixed_point, 0, 0, 0, 0};
    //CONFIGS!!!!!!
        
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, SYS_FREQ/(fs));
    ConfigIntTimer3(T3_INT_ON | T3_INT_PRIOR_3);
    mT3ClearIntFlag();
    
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, SYS_FREQ/(fs));
    ConfigIntTimer4(T4_INT_ON | T4_INT_PRIOR_2);
    mT4ClearIntFlag();
        
    // initialize MOSI
    PPSOutput(2, RPB5, SDO2);			// MOSI for DAC
    mPORTBSetPinsDigitalOut(BIT_4);		// CS for DAC
    mPORTBSetBits(BIT_4);              // initialize CS as high

    // the ADC ///////////////////////////////////////
    // configure and enable the ADC
	CloseADC10();	// ensure the ADC is off before setting the configuration

	// define setup parameters for OpenADC10
	// Turn module on | ouput in integer | trigger mode auto | enable autosample
        // ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
        // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
        // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
        #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON //

	// define setup parameters for OpenADC10
	// ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
	#define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_2 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_ON
        //
	// Define setup parameters for OpenADC10
        // use peripherial bus clock | set sample time | set ADC clock divider
        // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
        // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
        #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

	// define setup parameters for OpenADC10
	// set AN11(pin 24) and AN5(pin 7) as analog inputs
	//#define PARAM4	ENABLE_AN5_ANA | ENABLE_AN11_ANA// pin 24
    #define PARAM4	ENABLE_AN11_ANA | ENABLE_AN5_ANA
	// define setup parameters for OpenADC10
	// do not assign channels to scan
    #define PARAM5 SKIP_SCAN_ALL
	//#define PARAM5	SKIP_SCAN_AN0 | SKIP_SCAN_AN1 |SKIP_SCAN_AN2 |SKIP_SCAN_AN2 |SKIP_SCAN_AN4 |SKIP_SCAN_AN6 |SKIP_SCAN_AN7 |SKIP_SCAN_AN8 |SKIP_SCAN_AN9 |SKIP_SCAN_AN10 |SKIP_SCAN_AN12 

	// use ground as neg ref for A | use AN11 for input A     
	// configure to sample AN11 
	SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11 | ADC_CH0_NEG_SAMPLEB_NVREF | ADC_CH0_POS_SAMPLEB_AN5  ); // configure to sample AN4 
	//SetChanADC10(ADC_CH0_NEG_SAMPLEB_NVREF | ADC_CH0_POS_SAMPLEB_AN5); // configure to sample AN4 
	OpenADC10( PARAM1, PARAM2, PARAM3, PARAM4, PARAM5 ); // configure ADC using the parameters defined above

	EnableADC10(); // Enable the ADC
  ///////////////////////////////////////////////////////
    int iter = 0;
    for(iter = 0; iter<order; iter++) {
        update(ref,0);
    }
    iter = 0;
    for(iter = 0; iter<equal_order; iter++) {
        update_fb(desired_buf,0);
        update_fb(fb_tot, weights_eq[0]*fb0[iter] + (weights_eq[1]*fb1[iter] + weights_eq[2]*fb2[iter] + weights_eq[3]*fb3[iter] + weights_eq[4]*fb4[iter])/fixed_point);
    }
    // initialize the threads
    PT_INIT(&pt_refresh);
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    
    INTEnableSystemMultiVectoredInt();

    tft_setRotation(1); //240x320 horizontal display
  
    //round-robin scheduler for threads
    while(1) {
        
        PT_SCHEDULE(protothread_refresh(&pt_refresh));
     
    }
    
} //main

// add new value to array and shift

void update(int* array, int new) {
    for (i=1;i<order;i++){
        array[i-1]=array[i];
    }
    array[order-1]=new;
}

// in this update, the most recent value is stored in the 0th index
void update_eq(int* array, int new) {
    for (i=equal_order-1;i>1;i--){
        array[i]=array[i-1];
    }
    array[0]=new;
}
void update_fb(int* array, int new) {
    for (i=1;i<equal_order;i++){
        array[i-1]=array[i];
    }
    array[equal_order-1]=new;
}
//calculate inner products
int innerproduct(int* a, int* b) {
    int sum=0;
    for (i=0;i<order;i++) {
        sum=sum+a[i]*b[i];
    }
    return sum;
}

int innerproduct_eq(int* a, int* b) {
    int sum=0;
    for (i=0;i<equal_order;i++) {
        sum=sum+a[i]*b[i];
    }
    return sum;
}

float innerproductf(int* a, float* b) {
    float sum=0;
    for (i=0;i<order;i++) {
        sum=sum+a[i]*b[i];
    }
    return sum;
}