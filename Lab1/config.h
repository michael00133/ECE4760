/*
 * File:   config.h
 * Author: Syed Tahmid Mahbub
 * Modified by: Michael Nguyen
 *
 * Created on October 10, 2014
 */

#ifndef CONFIG_H
#define	CONFIG_H

#include "plib.h"
// serial stuff
#include <stdio.h>

// sets SYSCLK to 64MHz
#pragma config FNOSC = FRCPLL, POSCMOD = HS
#pragma config FPLLIDIV = DIV_2, FPLLMUL = MUL_16
#pragma config FPBDIV = DIV_2, FPLLODIV = DIV_1
#pragma config FWDTEN = OFF, JTAGEN = OFF, FSOSCEN = OFF

#endif	/* CONFIG_H */

