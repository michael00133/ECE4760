/*
 * File:	game.c
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

#define dmaChn 0
#define dmaChn2 1
#define dmaChn3 2
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

// Define a struct for balls
typedef struct Ball Ball;
struct Ball {
    int xpos;
    int ypos;
    int xvel;
    int yvel;
    int color;
    int8_t delay;
    Ball *b;
} ;

static struct pt pt_calculate, pt_refresh, pt_adc;
char buffer[60];

//Points to the head of the linked list of balls
struct Ball *head;
//ADC value
volatile int adc_9;

//Drag divisor to simulate friction between the ball and table
int drag = 1000;
//Scale is used to convert float point notation to fixed point
int scale = 1024;
//Define Ball radius and time between collisions
uint8_t ballradius = 2;
uint8_t delay_master = 10;

//Parameters for the paddle
uint8_t half_paddle_length = 15;
uint8_t paddle_ypos = 105;
uint8_t paddle_xpos = 6;
//Paddle velocity;
int paddle_v;
//Paddle drag coefficient
int paddle_drag=5;
//keeps track of the frames per second
uint8_t frames = 0;

//these are used to control when balls are made and how many are made
uint8_t numBalls = 0;
uint8_t maxBalls = 50;
uint8_t ballgen = 0;

//Constants
int dist;

//DMA Parameters
//#define sine_table_size 64


volatile unsigned int phase, incr, DAC_value; // DDS variables
//volatile int CVRCON_setup; // stores the voltage ref config register after it is set up

int score = 0;
int timeElapsed =0 ;
//============== Create a ball ================//
struct Ball *Ball_create(int xp, int yp, int xv, int yv, int color,  uint8_t d, Ball *bb) {
    
    struct Ball *ba = malloc(sizeof(struct Ball));
    if(ba == NULL)
        return NULL;
    
    ba->xpos = xp*scale;
    ba->ypos = yp*scale;
    ba->xvel = xv*scale;
    ba->yvel = yv*scale;
    ba->color = color;
    ba->delay = d;
    ba->b = bb;
    
    return ba;
}
//======================= Refresh ========================= //
//Does Ball calculations and Draws the necessary elements on the screen 
static PT_THREAD (protothread_refresh(struct pt *pt))
{
    PT_BEGIN(pt);
    PT_YIELD_TIME_msec(100);
    //waits for the scoreboard to be set up
    while(1) {
        
        while (timeElapsed <=60) {
            
            PT_YIELD_TIME_msec(10);
            DmaChnDisable(dmaChn);
            DmaChnDisable(dmaChn2);
            //Generates a new ball at a given interval
            if(ballgen >= 10) {
                int troll1 = -((rand()) % 2)-1;
                int troll2 = ((rand()) % 6) - 3;
                struct Ball *temp = Ball_create(320,120,troll1,troll2,(numBalls+1)*500,0,NULL);
                temp->b = head;
                head = temp;
                ballgen = 0;
                numBalls++;
            }
            else
                ballgen ++;

            //collision calculations
            struct Ball *ti = head;
            struct Ball *tj = NULL;
            if(ti != NULL)
                tj = ti->b;
            while(ti !=NULL){
                //Calculates the collisions between every ball
                while(tj != NULL) {
                    int rij_x = ti->xpos - tj->xpos;
                    int rij_y = ti->ypos - tj->ypos;
                    int mag_rij = pow(rij_x,2) + pow(rij_y,2);
                    //Checks if ti and tj are not pointing to the same ball,
                    //If they close enough for a collision and there is no collision
                    //delay.
                    if( ti->delay + tj->delay == 0 && mag_rij < dist) {
                        int vij_x = ti->xvel - tj->xvel;
                        int vij_y = ti->yvel - tj->yvel;
                        if (mag_rij==0) {
                            mag_rij=dist;
                        }
                        int deltaVi_x = (int)((-1*(rij_x) * ((((rij_x * vij_x)+ (rij_y*vij_y)) << 7)/mag_rij)) >> 7);
                        int deltaVi_y = (int)((-1*(rij_y) * ((((rij_x * vij_x)+ (rij_y*vij_y)) << 7)/mag_rij)) >> 7);
                        /*
                        tft_fillRoundRect(0,30, 320, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
                        tft_setCursor(0, 30);
                        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(2);
                        sprintf(buffer,"%d:%d", (-1*(rij_x)/128 * (128*((rij_x * vij_x)+ (rij_y*vij_y))/mag_rij)), mag_rij);
                        tft_writeString(buffer);
                        */
                        //Updates the velocity
                        ti->xvel = ti->xvel + deltaVi_x;
                        ti->yvel = ti->yvel + deltaVi_y;

                        tj->xvel = tj->xvel - deltaVi_x;
                        tj->yvel = tj->yvel - deltaVi_y;

                        ti->delay = delay_master;
                        tj->delay = delay_master;
                    }
                    tj = tj->b;
                }

                //checks for wall collisions

                if(ti->xpos >= 320*scale || ti->xpos <= 0) 
                    ti->xvel = -1*ti->xvel;
                if(ti->ypos >= 240*scale || ti->ypos <= 35*scale) {
                    ti->yvel = -1*ti->yvel;
                    if (ti->xpos > 120*scale && ti->xpos < 200*scale) { //check for catch bin
                        ti->delay=-1; //set to -1 to indicate +1 point
                    }
                }
                //calculates the drag

                if(ti->xvel > 0)
                    ti->xvel = ti->xvel - ti->xvel/drag;
                else
                    ti->xvel = ti->xvel + ti->xvel/drag;
                if(ti->yvel > 0)
                    ti->yvel = ti->yvel - ti->yvel/drag;
                else
                    ti->yvel = ti->yvel - ti->yvel/drag;

                // Check for paddle Collisions
                if(abs(paddle_xpos-ti->xpos/scale) <= ballradius && ti->delay == 0)
                    if(ti->ypos/scale > paddle_ypos - half_paddle_length && ti->ypos/scale < paddle_ypos + half_paddle_length) {
                        ti->xvel = -1*ti->xvel;
                        ti->yvel = ti->yvel + paddle_drag*paddle_v;
                        ti->delay=delay_master;
                    }
                //Decrement the collide delay
                if(ti->delay > 0)
                    ti->delay = ti->delay -1;


                //iterates through the next set
                ti = ti->b;
                if(ti != NULL)
                    tj = ti->b;

                //removes the last element if the limit is reached
                if(numBalls > maxBalls && tj->b == NULL) { 
                    tft_fillCircle(tj->xpos/scale,tj->ypos/scale,ballradius,ILI9340_BLACK); //erases from the screen
                    ti->b = NULL;
                    numBalls--;
                    score++;
                    //free(tj);
                }

            }
            // Calculates position of the paddle and draw
            //TODO: Calculate paddle position
            tft_drawLine(paddle_xpos,paddle_ypos - half_paddle_length, paddle_xpos, paddle_ypos + half_paddle_length, ILI9340_BLACK);
            paddle_v=paddle_ypos;
            paddle_ypos=(adc_9*240)/1023;
            paddle_v=paddle_ypos-paddle_v;
            tft_drawLine(paddle_xpos,paddle_ypos - half_paddle_length, paddle_xpos, paddle_ypos + half_paddle_length, ILI9340_WHITE);

            // Now it calculates the new position
            ti = head;
            tj = head;
            while(ti != NULL){
                //"Clears" the image of the last ball
                tft_fillCircle(ti->xpos/scale,ti->ypos/scale,ballradius,ILI9340_BLACK);

                //Updates the new position of the ball
                ti->xpos = ti->xpos + ti->xvel;
                ti->ypos = ti->ypos + ti->yvel;

                //ensures the positions are within bounds
                //If the pos is less than 0 then we remove it
                //delay must also not be -1 (ie >=0)
                if(ti->xpos > paddle_xpos && ti->delay != -1) {
                    if(ti->xpos > 320*scale)
                        ti->xpos = 320*scale;

                    if(ti->ypos > 240*scale)
                        ti->ypos = 240*scale;
                    else if(ti->ypos < 35*scale)
                        ti->ypos = 35*scale;

                    if(ti->delay > 0)
                         tft_fillCircle(ti->xpos/scale, ti->ypos/scale, ballradius, ILI9340_WHITE);
                    else
                        tft_fillCircle(ti->xpos/scale, ti->ypos/scale, ballradius, ti->color);
                }
                else { //REMOVES THE BALL IF IT CROSSES THE BOUNDARY
                    if (ti->delay==-1) { //check if went into catch bins
                        score++;
                        DmaChnEnable(dmaChn2);
                    }
                    else {
                        	DmaChnEnable(dmaChn);
                            
                        score--;
                    }
                    if(ti == head)
                        head = head->b;
                    else
                        tj->b = ti->b;
                    numBalls--;
                    //free(ti);
                }
                tj = ti;//what does this do?
                ti = ti->b;
            }
            frames ++;
       }
       
       tft_fillRoundRect(0,35, 320, 205, 1, ILI9340_BLACK);// x,y,w,h,radius,color
       DmaChnDisable(dmaChn);
       DmaChnDisable(dmaChn2);
       DmaChnEnable(dmaChn3);
       while (1) {
           
            tft_setCursor(20, 120);
            tft_setTextColor(ILI9340_WHITE); tft_setTextSize(4);
            sprintf(buffer,"Game Over!");
            tft_writeString(buffer);
       }
   }
    PT_END(pt);
} // blink

//==================== Calculate ===================== //
static PT_THREAD (protothread_calculate (struct pt *pt))
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
        sprintf(buffer,"%02d:%02d  FPS:%d  Score:%d", minutes,seconds, frames, score);
        tft_writeString(buffer);
        // draw catch bins
        tft_fillRoundRect(120,35, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_fillRoundRect(120,235, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_fillRoundRect(200,35, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_fillRoundRect(200,235, 2, 5, 1, ILI9340_WHITE);// x,y,w,h,radius,color

        frames = 0;
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

//===================== Main ======================= //
void main(void) {
    //SYSTEMConfigPerformance(PBCLK);
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    PT_setup();
    
    head = NULL;
    dist = pow(2*(ballradius*scale),2);
    
        int timer_limit;
        int sine_table_size;
    // frequency settable to better than 2% relative accuracy at low frequency
        // frequency settable to 5% accuracy at highest frequency
        // Useful frequency range 10 Hz to 100KHz
        // >40 db attenuation of  next highest amplitude frequency component below 100 kHz
        // >35 db above 100 KHz
        #define F_OUT 80000
        if (F_OUT <= 5000 ){
            sine_table_size = 256 ;
            timer_limit = SYS_FREQ/(sine_table_size*F_OUT) ;
        }
        else if (F_OUT <= 10000 ){
            sine_table_size = 128 ;
            timer_limit = SYS_FREQ/(sine_table_size*F_OUT) ;
        }
        else if (F_OUT <= 20000 ){
            sine_table_size = 64 ;
            timer_limit = SYS_FREQ/(sine_table_size*F_OUT) ;
        }
        else if (F_OUT <= 100000 ){
            sine_table_size = 32 ;
            timer_limit = SYS_FREQ/(sine_table_size*F_OUT) ;
        }
        else {
            sine_table_size = 16 ;
            timer_limit = SYS_FREQ/(sine_table_size*F_OUT) ;
        }

         // build the sine lookup table
        // scaled to produce values between 0 and 63 (six bit)
        int i;
        unsigned char sine_table[sine_table_size];
        for (i = 0; i < sine_table_size; i++){
        //sine_table[i] =(unsigned char) (31.5 * sin((float)i*6.283/(float)sine_table_size) + 32); //
           // s =(unsigned char) (63.5 * sin((float)i*6.283/(float)sine_table_size) + 64); //7 bit
            unsigned char s =(unsigned char) (63.5 * sin((float)i*6.283/(float)sine_table_size) + 64); //7 bit
            //sine_table[i] = (s & 0x3f) | ((s & 0x40)<<1) ;
            sine_table[i] = 0x60 | s;
         }
        
        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // Uses macro to set timer tick to 2 microSec = 120 cycles 500 kHz DDS
        // peripheral at 60 MHz
        //--------------------
        //------------------
        // 64 LEVEL samples thru external DAC
        // interval=60, 32 samples, 31200 Hz, next harmonic 40 db down
        // interval=20, 32 samples, 90.8 KHz, next harmonic 45 db down
        // interval=600, 32 samples, 3170 Hz, next harmonic 32 db down
        // interval=300, 64 samples, 3170 Hz, next harmonic 38 db down
        // interval=150, 128 samples, 3170 Hz, next harmonic 43  db down
        OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, 400);
        OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, 1600);
        OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_1, 800);
        int CVRCON_setup;
        CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
        CVRCON_setup = CVRCON;
        

        // Open the desired DMA channel.
        
	// We enable the AUTO option, we'll keep repeating the sam transfer over and over.
	DmaChnOpen(dmaChn, 0, DMA_OPEN_AUTO);
    DmaChnSetTxfer(dmaChn, sine_table, (void*)&CVRCON, sine_table_size, 1, 1);
	DmaChnSetEventControl(dmaChn, DMA_EV_START_IRQ(_TIMER_3_IRQ));

    DmaChnOpen(dmaChn2, 0, DMA_OPEN_AUTO);
    DmaChnSetTxfer(dmaChn2, sine_table, (void*)&CVRCON, sine_table_size, 1, 1);
	DmaChnSetEventControl(dmaChn2, DMA_EV_START_IRQ(_TIMER_4_IRQ));
    
    DmaChnOpen(dmaChn3, 0, DMA_OPEN_AUTO);
    DmaChnSetTxfer(dmaChn3, sine_table, (void*)&CVRCON, sine_table_size, 1, 1);
	DmaChnSetEventControl(dmaChn3, DMA_EV_START_IRQ(_TIMER_1_IRQ));
    
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
	#define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
        //
	// Define setup parameters for OpenADC10
        // use peripherial bus clock | set sample time | set ADC clock divider
        // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
        // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
        #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

	// define setup parameters for OpenADC10
	// set AN11 and  as analog inputs
	#define PARAM4	ENABLE_AN11_ANA // pin 24

	// define setup parameters for OpenADC10
	// do not assign channels to scan
	#define PARAM5	SKIP_SCAN_ALL

	// use ground as neg ref for A | use AN11 for input A     
	// configure to sample AN11 
	SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11 ); // configure to sample AN4 
	OpenADC10( PARAM1, PARAM2, PARAM3, PARAM4, PARAM5 ); // configure ADC using the parameters defined above

	EnableADC10(); // Enable the ADC
  ///////////////////////////////////////////////////////
        
    // initialize the threads
    PT_INIT(&pt_calculate);
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
        PT_SCHEDULE(protothread_calculate(&pt_calculate));
        PT_SCHEDULE(protothread_refresh(&pt_refresh));
        PT_SCHEDULE(protothread_adc(&pt_adc));
    }
    
} //main

