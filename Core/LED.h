#ifndef LED_H
#define LED_H

#include "STM32X.h"

/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

typedef enum {
	LED_Color_None 		= 0,
	LED_Color_Red 		= (1 << 0),
	LED_Color_Green 	= (1 << 1),
	LED_Color_Blue 		= (1 << 2),
	LED_Color_Yellow 	= LED_Color_Red | LED_Color_Green,
	LED_Color_Purple 	= LED_Color_Red | LED_Color_Blue,
	LED_Color_Cyan 		= LED_Color_Green | LED_Color_Blue,
} LED_Color_t;

/*
 * PUBLIC FUNCTIONS
 */

void LED_Init(void);
void LED_Deinit(void);
void LED_Write(LED_Color_t color);

/*
 * EXTERN DECLARATIONS
 */


#endif // LED_H
