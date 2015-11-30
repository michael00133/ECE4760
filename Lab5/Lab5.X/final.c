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

// MDDFS (SD) Card libraries
#include <stdio.h>
#include "FSIO.h"
#include "plib.h"
#include <GenericTypeDefs.h>

// Threading Library
// config.h sets SYSCLK 40 MHz
#define SYS_FREQ 40000000
#include "pt_cornell_TFT.h"
#define DAC_config_chan_A 0b0011000000000000

// === the fixed point macros ========================================
typedef signed int fix16 ;
#define multfix16(a,b) ((fix16)(((( signed long long)(a))*(( signed long long)(b)))>>16)) //multiply two fixed 16:16
#define float2fix16(a) ((fix16)((a)*65536.0)) // 2^16
#define fix2float16(a) ((float)(a)/65536.0)
#define fix2int16(a)    ((int)((a)>>16))
#define int2fix16(a)    ((fix16)((a)<<16))
#define divfix16(a,b) ((fix16)((((signed long long)(a)<<16)/(b)))) 
#define sqrtfix16(a) (float2fix16(sqrt(fix2float16(a)))) 
#define absfix16(a) abs(a)

//===== Wav File Read
#define allGood             0
#define incorrectHeaderSize 1
#define riff                2
#define wav                 3
#define fmt                 4
#define notPCM              5
#define stackSize           128     // cyclic buffer/stack

#define order 5 //order of nlms filter
#define mu 0.006 //stepsize
UINT8 receiveBuffer[100];

static struct pt pt_refresh,pt_adc;

volatile unsigned int DAC_data ;// output value
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this DAC

int fs=44100; //sampling rate for ADC
char buffer[60];
//ADC value
 int adc_9;
int timeElapsed =0 ;
 int input=0;//flag for which input ADC is reading 0 for noise, 1 for signal+noise
 int ref[order];
 int primary;
 int weights[order];
 int desired;
int i;
 

int innerproduct(int* a, int* b);
void update(int* array, int new);
void setupAudioPWM(void);
void getFilename(char * buffer);
void configureHardware(UINT32 sampleRate);
void getParameters(UINT8 * bitsPerSample, UINT8 * numberOfChannels,
                        UINT32 * dataSize, UINT32 * sampleRate,
                        UINT32 * blockAlign);

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
        sprintf(buffer,"%02d:%02d  FPS:%d  Score:%d");
        tft_writeString(buffer);
        // draw catch bins
        tft_fillRoundRect(120,35, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_fillRoundRect(120,235, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_fillRoundRect(200,35, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_fillRoundRect(200,235, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color

        PT_YIELD_TIME_msec(1000);
        timeElapsed++ ;
        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
}

// === ADC Thread =============================================
// 

static PT_THREAD (protothread_adc(struct pt *pt))
{
    PT_BEGIN(pt);
 
             
    while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(2);
        
        // read the ADC from pin 24 (AN11)
        // read the first buffer position
        adc_9 = ReadADC10(0);   // read the result of channel 9 conversion from the idle buffer
        AcquireADC10(); // not needed if ADC_AUTO_SAMPLING_ON below
 
        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // animation thread

void __ISR(_TIMER_3_VECTOR, ipl3) Timer3Handler(void){
    mT3ClearIntFlag();
    if (input==0) { //noise from an10
        update(ref,ReadADC10(0));   // read the result of channel 9 conversion from the idle buffer;
    }
    else {
        primary=ReadADC10(0);   // read the result of channel 9 conversion from the idle buffer
    }
    AcquireADC10(); // not needed if ADC_AUTO_SAMPLING_ON below
    input=1^input;//toggle input
}

void __ISR(_TIMER_4_VECTOR, ipl2) Timer4Handler(void){
    mT2ClearIntFlag();
    //NLMS filter 
    desired=primary-innerproduct(ref,weights);
    for(i=1;i<order;i++) {
        weights[i]=(int)(weights[i]+((ref[i]*mu*desired)/(innerproduct(ref,ref)+1)));
    }
    DAC_data=desired;//this can be optimized
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

/* ADC ISR */
  /* Specify ADC Interrupt Routine, Priority Level = 6 */
/*
 void __ISR(_ADC_VECTOR, ipl6) AdcHandler(void)
 {
      
           // clear the interrupt flag
    mAD1ClearIntFlag();
 }
*/
//===================== Main ======================= //
void main(void) {
    //SYSTEMConfigPerformance(PBCLK);
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    PT_setup();
    
   
        
    //CONFIGS!!!!!!
        
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, SYS_FREQ/(2*fs));
    ConfigIntTimer3(T3_INT_ON | T3_INT_PRIOR_3);
    mT3ClearIntFlag();
    
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, SYS_FREQ/fs);
    ConfigIntTimer4(T4_INT_ON | T4_INT_PRIOR_2);
    mT2ClearIntFlag();
        
    // initialize MOSI
    PPSOutput(2, RPB5, SDO2);			// MOSI for DAC
    mPORTBSetPinsDigitalOut(BIT_4);		// CS for DAC
    mPORTBSetBits(BIT_4);              // initialize CS as high
    
    //play music stuff
     FSFILE * pointer;
//    FSFILE * pointer2;
//    char path[30];
//    char count = 30;
    SearchRec rec;
    UINT8 attributes = ATTR_MASK;   // file can have any attributes

    // audio stuff (WAV file)
    UINT8 bitsPerSample;
    UINT32 sampleRate;
    UINT8 numberOfChannels;
    UINT32 dataSize;
    UINT8 blockAlign;

    UINT8 audioStream[stackSize*4];
    UINT16 audioByte;
    UINT16 lc;
    UINT16 retBytes;
    UINT16 unsign_audio;
    
    // the ADC ///////////////////////////////////////
        // configure and enable the ADC
	CloseADC10();	// ensure the ADC is off before setting the configuration

	// define setup parameters for OpenADC10
	// Turn module on | ouput in integer | trigger mode auto | enable autosample
        // ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
        // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
        // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
        #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_OFF //

	// define setup parameters for OpenADC10
	// ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
	#define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_ON
        //
	// Define setup parameters for OpenADC10
        // use peripherial bus clock | set sample time | set ADC clock divider
        // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
        // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
        #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

	// define setup parameters for OpenADC10
	// set AN11(pin 24) and AN5(pin 7) as analog inputs
	#define PARAM4	ENABLE_AN11_ANA | ENABLE_AN5_ANA// pin 24

	// define setup parameters for OpenADC10
	// do not assign channels to scan
	#define PARAM5	SKIP_SCAN_ALL

	// use ground as neg ref for A | use AN11 for input A     
	// configure to sample AN11 
	SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11 | ADC_CH0_NEG_SAMPLEB_NVREF | ADC_CH0_POS_SAMPLEB_AN5  ); // configure to sample AN4 
	OpenADC10( PARAM1, PARAM2, PARAM3, PARAM4, PARAM5 ); // configure ADC using the parameters defined above

	EnableADC10(); // Enable the ADC
  ///////////////////////////////////////////////////////
        
    // initialize the threads
    PT_INIT(&pt_refresh);
    PT_INIT(&pt_adc);
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    
    INTEnableSystemMultiVectoredInt();

    tft_setRotation(1); //240x320 horizontal display
  
    //round-robin scheduler for threads
    while(1) {
        
        PT_SCHEDULE(protothread_refresh(&pt_refresh));
        PT_SCHEDULE(protothread_adc(&pt_adc));
    }
    
} //main

UINT8 getWavHeader(FSFILE * pointer) {

    if (FSfread(receiveBuffer, 1, 44, pointer) != 44) {
        return incorrectHeaderSize;
    }

    if ( (receiveBuffer[0] != 'R') |
            (receiveBuffer[1] != 'I') |
            (receiveBuffer[2] != 'F') |
            (receiveBuffer[3] != 'F') ) {
        return riff;
    }

    if ( (receiveBuffer[8] != 'W') |
            (receiveBuffer[9] != 'A') |
            (receiveBuffer[10] != 'V') |
            (receiveBuffer[11] != 'E') ) {
        return wav;
    }

    if ( (receiveBuffer[12] != 'f') |
            (receiveBuffer[13] != 'm') |
            (receiveBuffer[14] != 't') ) {
        return fmt;
    }

    if (receiveBuffer[20] != 1) {
        return notPCM;
    }

    return allGood;         // no errors

}

void getParameters(UINT8 * bitsPerSample, UINT8 * numberOfChannels,
                        UINT32 * dataSize, UINT32 * sampleRate,
                        UINT32 * blockAlign) {
    *bitsPerSample = receiveBuffer[34];
    *numberOfChannels = receiveBuffer[22];
    *sampleRate = (receiveBuffer[25] << 8) | receiveBuffer[24];

    *dataSize = (receiveBuffer[43] << 24) |
        (receiveBuffer[42] << 16) |
        (receiveBuffer[41] << 8) |
        receiveBuffer[40];
    
    *blockAlign = receiveBuffer[32];
}

// add new value to array and shift

void update(int* array, int new) {
    for (i=1;i<sizeof(array);i++){
        array[i-1]=array[i];
    }
    array[sizeof(array)-1]=new;
}

int innerproduct(int* a, int* b) {
    int sum=0;
    if (sizeof(a)!=sizeof(b)) {
        return 0;
    }
    for (i=1;i<sizeof(a);i++) {
        sum=sum+a[i]*b[i];
    }
}

