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
    int xpos;
    int ypos;
    int xvel;
    int yvel;
    uint8_t delay;
    Ball *b;
} ball;

static struct pt pt_calculate, pt_refresh;
char buffer[60];
struct Ball *head;
//signed short max_velocity = 20;
uint8_t drag = 1000;
uint8_t scale = 1000;
uint8_t ballradius = 3;
uint8_t delay_master = 10;
uint8_t refreshRate;
//============== Create a ball ================//
struct Ball *Ball_create(int xp, int yp, int xv, int yv,  uint8_t d, Ball *bb) {
    
    struct Ball *ba = malloc(sizeof(struct Ball));
    if(ba == NULL)
        return NULL;
    
    ba->xpos = xp*scale;
    ba->ypos = yp*scale;
    ba->xvel = xv*scale;
    ba->yvel = yv*scale;
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
        struct Ball *tj = NULL;
        if(ti != NULL)
            tj = ti->b;
        
        while(ti !=NULL){
            //Calculates the collisions between every ball
            while(tj != NULL) {
                int rij_x = ti->xpos - tj->xpos;
                int rij_y = ti->ypos - tj->ypos;
                int mag_rij = pow(rij_x,2) + pow(rij_y,2);
                int temp = pow(2*(ballradius*scale),2);
                //Checks if ti and tj are not pointing to the same ball,
                //If they close enough for a collision and there is no collision
                //delay.
                if( ti->delay + tj->delay == 0 && mag_rij < temp) {
                    int vij_x = ti->xvel - tj->xvel;
                    int vij_y = ti->yvel - tj->yvel;
                    int deltaVi_x = -1*(rij_x * ((rij_x * vij_x)+ (rij_y*vij_y)))/temp;
                    int deltaVi_y = -1*(rij_y * ((rij_x * vij_x)+ (rij_y*vij_y)))/temp;
                    
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
            if(ti->xpos >= 240*scale || ti->xpos <= 0) 
                ti->xvel = -1*ti->xvel;
            if(ti->ypos >= 320*scale || ti->ypos <= 0)
                ti->yvel = -1*ti->yvel;
            
            //calculates the drag
            if(ti->xvel > 0)
                ti->xvel = ti->xvel - ti->xvel/drag;
            else
                ti->xvel = ti->xvel + ti->xvel/drag;
            if(ti->yvel > 0)
                ti->yvel = ti->yvel - ti->yvel/drag;
            else
                ti->yvel = ti->yvel - ti->yvel/drag;
            
            //Decrement the collide delay
            if(ti->delay > 0)
                ti->delay = ti->delay -1;
            
            //iterates through the next set
            ti = ti->b;
            if(ti != NULL)
                tj = ti->b;
        }
        
        // Now it calculates the new position and 
	    ti = head;
        while(ti != NULL){
            //"Clears" the image of the last ball
            tft_fillCircle(ti->xpos/scale,ti->ypos/scale,ballradius,ILI9340_BLACK);
            
            //Updates the new position of the ball
            ti->xpos = ti->xpos + ti->xvel;
            ti->ypos = ti->ypos + ti->yvel;
            
            //ensures the positions are within bounds
            if(ti->xpos > 240*scale)
                ti->xpos = 240*scale;
            else if(ti->xpos < 0)
                ti->xpos = 0;
            
            if(ti->ypos > 320*scale)
                ti->ypos = 320*scale;
            else if(ti->ypos < 0)
                ti->ypos = 0;
            
            if(ti->delay != 0)
                 tft_fillCircle(ti->xpos/scale, ti->ypos/scale, ballradius, 500);
            else
                tft_fillCircle(ti->xpos/scale, ti->ypos/scale, ballradius, ILI9340_YELLOW);
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
        int troll1 = (rand() % 5);
        int troll2 = (rand() % 5);
        struct Ball *temp = Ball_create(i*20,i*20,troll1,troll2,0,NULL);
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

