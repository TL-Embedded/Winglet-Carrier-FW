#include "LED.h"

#include "GPIO.h"

/*
 * PRIVATE DEFINITIONS
 */

#ifndef LED_ACTIVE
#define LED_ACTIVE			GPIO_PIN_RESET
#endif

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

/*
 * PRIVATE VARIABLES
 */

/*
 * PUBLIC FUNCTIONS
 */

void LED_Init(void)
{
#ifdef LED_PORT_COMMON
	GPIO_EnableOutput(LED_R_PIN | LED_G_PIN | LED_B_PIN, !LED_ACTIVE);
#else
	GPIO_EnableOutput(LED_R_PIN, !LED_ACTIVE);
	GPIO_EnableOutput(LED_G_PIN, !LED_ACTIVE);
	GPIO_EnableOutput(LED_B_PIN, !LED_ACTIVE);
#endif
}

void LED_Deinit(void)
{
#ifdef LED_PORT_COMMON
	GPIO_Deinit(LED_R_PIN | LED_G_PIN | LED_B_PIN);
#else
	GPIO_Deinit(LED_R_PIN);
	GPIO_Deinit(LED_G_PIN);
	GPIO_Deinit(LED_B_PIN);
#endif
}

void LED_Write(LED_Color_t color)
{
	GPIO_Write(LED_R_PIN, (color & LED_Color_Red) ? LED_ACTIVE : !LED_ACTIVE );
	GPIO_Write(LED_G_PIN, (color & LED_Color_Green) ? LED_ACTIVE : !LED_ACTIVE );
	GPIO_Write(LED_B_PIN, (color & LED_Color_Blue) ? LED_ACTIVE : !LED_ACTIVE );
}

/*
 * PRIVATE FUNCTIONS
 */

/*
 * INTERRUPT ROUTINES
 */
