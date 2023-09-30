#ifndef MAIN_DEFINES_H_
#define MAIN_DEFINES_H_

#include "aemo.h"

#define VERSION 	"0.10"

// Keep a trailing history of up to 15 entries
#define HISTORY		15

#define GPIO_OUT1	16
#define GPIO_OUT2	17

#define IDLE 	0
#define FETCH 	1

extern struct aemo aemo_history[HISTORY];
extern uint8_t aemo_idx;

#endif