/*
 * File:	main.c
 * Author:	Michael Nguyen
 * 		Tri Hoang
 * 		Elias Wang
 * Target PIC:	PIC32MX250F128B
 */

// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"

// Threading Library
// config.h sets 40 MHz
#define SYS_FREQ 40000000
#include "pt_cornell_TFT.h"

/*Code goes here */
