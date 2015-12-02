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
#define AC_ZERO             2048    // since Dac resolution is 12-bits
#define PB_CLK              SYS_FREQ

volatile UINT16 LSTACK[stackSize];
volatile UINT16 RSTACK[stackSize];
volatile UINT32 BOS;
volatile UINT32 TOS;

volatile UINT32 msCounter = 0;
volatile UINT32 randomvar = 0;
volatile UINT32 CSlength = 0;
volatile UINT32 j = 0;
volatile UINT32 TIC=0, TOC=0;

volatile UINT32 bufferCounter = 0;
volatile UINT32 intCounter = 0;


#define order 10 //order of nlms filter
#define mu 0.006 //stepsize
UINT8 receiveBuffer[100];
char txtBuffer[100];

static struct pt pt_refresh,pt_adc;

volatile unsigned int DAC_data ;// output value
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this DAC

int fs=44100; //sampling rate for ADC
char buffer[60];
//ADC value
 int adc_9;
int timeElapsed =1;
 int input=0;//flag for which input ADC is reading 0 for noise, 1 for signal+noise
 //primary in pin 24(muxA) ref in pin 7(muxB)
 int ref[order];
 int primary;
 int weights[order];
 int desired;
int i;
 

int  innerproduct(int* a, int* b);
void update(int* array, int new);
void setupAudioPWM(void);
void getFilename(char * buffer);
void getParameters(UINT8 * bitsPerSample, UINT8 * numberOfChannels,
                        UINT32 * dataSize, UINT32 * sampleRate,
                        UINT32 * blockAlign);
UINT8 getWavHeader(FSFILE * pointer);

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
    /*
    if (input==0) { //noise from an5
        update(ref,ReadADC10(0));   // read the result of channel 9 conversion from the idle buffer;
    }
    else {
        primary=ReadADC10(0);   // read the result of channel 9 conversion from the idle buffer
    }
    */
    primary=ReadADC10(0);
    update(ref,ReadADC10(1));
    
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
    //DAC_data=song-desired;
    DAC_data=desired;
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
    
    ANSELA = 0; ANSELB = 0; // Disable analog inputs
    CM1CON = 0; CM2CON = 0; CM3CON = 0;     // Disable analog comparators

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
    SearchRec rec;
    UINT8  attributes = ATTR_MASK;   // file can have any attributes

    // audio stuff (WAV file)
    UINT8  bitsPerSample;
    UINT32 sampleRate;
    UINT8  numberOfChannels;
    UINT32 dataSize;
    UINT8  blockAlign;

    UINT8  audioStream[stackSize*4];
    UINT16 audioByte;
    UINT16 lc;
    UINT16 retBytes;
    UINT16 unsign_audio;
    
    // clear cyclic buffer
    BOS = 0;
    TOS = 0;
    for (j=0; j<stackSize; j++) {
        LSTACK[j] = 0;
        RSTACK[j] = 0;
    }
    
    while (!MDD_MediaDetect());
    while (!FSInit());
 
    pointer = FSfopen("music.wav","r");
    if (pointer != NULL) {

      //  printf("Opened \"%s\"\n\r\n\r",txtBuffer);

        if (getWavHeader(pointer) == allGood) {
            
            getParameters(&bitsPerSample, &numberOfChannels, &dataSize,
                            &sampleRate, &blockAlign);
            
            //config hardware using sample rate
            PR2 = (PB_CLK/sampleRate) - 1;
            mT2IntEnable(1);

            bufferCounter = 0;
            if (bitsPerSample < 8)
                bitsPerSample = 8;
            else if (bitsPerSample < 16)
                bitsPerSample = 16;

            TIC = msCounter;

            while (bufferCounter < dataSize) {

                retBytes = FSfread(audioStream, 1,
                                stackSize*blockAlign, pointer);

                for (lc = 0; lc < retBytes; lc += blockAlign) {
                    if (bitsPerSample == 16) {
                        audioByte = (audioStream[lc+1] << 4) |
                            (audioStream[lc] >> 4);
                        if (audioByte & 0x0800) {
                            unsign_audio = ~(audioByte - 1);
                            audioByte = AC_ZERO - unsign_audio;
                        }
                        else {
                            audioByte = AC_ZERO + audioByte;
                        }
                        LSTACK[BOS] = 0x3000 | audioByte;

                        if (numberOfChannels == 2) {
                            audioByte = (audioStream[lc+3] << 4) |
                                (audioStream[lc+2] >> 4);
                            if (audioByte & 0x0800) {
                                unsign_audio = ~(audioByte - 1);
                                audioByte = AC_ZERO - unsign_audio;
                            }
                            else {
                                audioByte = AC_ZERO + audioByte;
                            }
                        }
                        RSTACK[BOS] = 0xB000 | audioByte;
                    }
                    else {
                        audioByte = audioStream[lc] << 4;
                        LSTACK[BOS] = 0x3000 | audioByte;
                        if (numberOfChannels == 2) {
                            audioByte = audioStream[lc+1] << 4;
                        }

                        RSTACK[BOS] = 0xB000 | audioByte;
                    }

                    if (++BOS == stackSize) BOS = 0;

                    if (bitsPerSample == 16) {
                        while (BOS == (TOS-2));
                    }
                    else {
                        while (BOS == (TOS-2)) {
// for some reason, not putting a delay here makes the 8-bit part not work (?)
                            UINT32 temp = msCounter;
                            while ((msCounter - temp) < 2);
                        }
                    }

                }   // for (lc ... )...

                bufferCounter += retBytes;
                T2CONSET = 0x8000;
            }   // while (bufferCounter < dataSize) ...

            FSfclose(pointer);
        } // if (pointer != NULL), ie if read audio file correctly
    }

    T2CON = 0;
    TMR2 = 0;
    mT2IntEnable(0);
    mT2ClearIntFlag();
    
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

void getFilename(char * buffer) {
#define maxChars        20
#define _BACKSPACE_     8

    char c = 32;    // 32 = space
    INT8 bufferCounter = 0;

    while (DataRdyUART2() == 0);
    c = getcUART2();

    while ((c!= 10) & (c!= 13)) {

        putcUART2(c);
        while (DataRdyUART2() == 0);

        *(buffer + bufferCounter) = c;

        if (c == _BACKSPACE_) {
            if (bufferCounter > 1) {
                bufferCounter-=2;
            }
            else {
                bufferCounter = 0;
            }
        }

        if (++bufferCounter == maxChars)
            break;

        c = getcUART2();
    }

    printf("\n\r");

    *(buffer + bufferCounter) = '\0';   // terminate string
}

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

