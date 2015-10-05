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

// Define a struct for balls
typedef struct Ball Ball;
struct Ball {
    signed short xpos;
    signed short ypos;
    signed char xvel;
    signed char yvel;
    uint8_t delay;
    Ball *b;
} ball;

static struct pt pt_calculate, pt_refresh;
char buffer[60];
struct Ball *head;
uint8_t ballradius = 5;
uint8_t delay_master = 3;
uint8_t refreshRate;
uint8_t collisions = 0;
//============== Create a ball ================//
struct Ball *Ball_create(signed short xp, signed short yp, signed char xv, signed char yv,  uint8_t d, Ball *bb) {
    
    struct Ball *ba = malloc(sizeof(struct Ball));
    if(ba == NULL)
        return NULL;
    
    ba->xpos = xp;
    ba->ypos = yp;
    ba->xvel = xv;
    ba->yvel = yv;
    ba->delay = d;
    ba->b = bb;
    
    return ba;
}

//======================= Refresh ========================= //
static PT_THREAD (protothread_refresh(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
        PT_YIELD_TIME_msec(10);
	    struct Ball *ti = head;
        while(ti != NULL){
            tft_fillCircle(ti->xpos, ti->ypos, ballradius, ILI9340_WHITE);
            ti = ti->b;
        }
   }
    PT_END(pt);
} // blink

//==================== Calculate ===================== //
static PT_THREAD (protothread_calculate (struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
        PT_YIELD_TIME_msec(refreshRate);
        

        struct Ball *ti = head;
        struct Ball *tj = head;
        while(ti !=NULL){
            //Calculates the collisions between every ball
            while(tj != NULL) {
                signed short rij_x = ti->xpos - tj->xpos;
                signed short rij_y = ti->ypos - tj->ypos;
                signed short mag_rij = pow(rij_x,2) + pow(rij_y,2);
                //Checks if ti and tj are not pointing to the same ball,
                //If they close enough for a collision and there is no collision
                //delay.
                if(ti != tj && ti->delay + tj->delay == 0 && mag_rij < 4*pow(ballradius,2)) {
                    signed short vij_x = ti->xvel - tj->xvel;
                    signed short vij_y = ti->yvel - tj->yvel;
                    signed short temp = pow(2*ballradius,2);
                    signed short deltaVi_x = -1*(rij_x * (rij_x * vij_x+ rij_y*vij_y))/temp;
                    signed short deltaVi_y = -1*(rij_y * (rij_x * vij_x+ rij_y*vij_y))/temp;
                    
                    //Updates the velocity
                    ti->xvel = ti->xvel + deltaVi_x;
                    ti->yvel = ti->yvel + deltaVi_y;
                    
                    tj->xvel = tj->xvel - deltaVi_x;
                    tj->yvel = tj->yvel - deltaVi_y;
                    
                    ti->delay = delay_master;
                    tj->delay = delay_master;
                    collisions++;
                }
                tj = tj->b;
            }
            //"Clears" the image of the last ball
            tft_fillCircle(ti->xpos,ti->ypos,ballradius,ILI9340_BLACK);
            
            //Updates the new position of the ball
            ti->xpos = ti->xpos + ti->xvel * refreshRate/10;
            ti->ypos = ti->ypos + ti->yvel * refreshRate/10;
            
            //checks for wall collisions
            if(ti->xpos > 240 || ti->xpos < 0) 
                ti->xvel = -1*ti->xvel;
            if(ti->ypos > 320 || ti->ypos < 0)
                ti->yvel = -1*ti->yvel;
            
            //Decrement the collide delay
            if(ti->delay > 0)
                ti->delay = ti->delay -1;
            ti = ti->b;
            tj = head;
        }
   }
    PT_END(pt);
}
//===================== Main ======================= //
void main(void) {
    SYSTEMConfigPerformance(PBCLK);

    ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

    PT_setup();
    refreshRate = 10; //msec
    
    //Temporary Random Ball generator
    head = Ball_create(50, 50, 2, 0,  0, NULL);
    int i = 1;
    for(i = 1; i < 15; i++) {
        signed char troll1 = rand() % 5;
        signed char troll2 = rand() % 5;
        struct Ball *temp = Ball_create(i*10,i*10,troll1,troll2,0,NULL);
        temp->b = head;
        head = temp;
    }
        
    // initialize the threads
    PT_INIT(&pt_calculate);
    PT_INIT(&pt_refresh);
    // initialize the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    
    INTEnableSystemMultiVectoredInt();

    tft_setRotation(0); //240x320 vertical display
  
    //round-robin scheduler for threads
    while(1) {
        PT_SCHEDULE(protothread_calculate(&pt_calculate));
        PT_SCHEDULE(protothread_refresh(&pt_refresh));
    }
    
} //main

