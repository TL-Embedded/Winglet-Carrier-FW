
#include "Core.h"

#include "GPIO.h"
#include "USB.h"
#include "Console.h"
#include "LED.h"
#include "UART.h"
#include "I2C.h"
#include "M24xx.h"


int main(void)
{
	CORE_Init();
	USB_Init();
	Console_Init();
	LED_Init();

	GPIO_EnableOutput(MODEM_PWR_EN, GPIO_PIN_RESET);
	GPIO_EnableOutput(MODEM_RESET, GPIO_PIN_RESET);
	GPIO_EnableOutput(MODEM_WAKE, GPIO_PIN_RESET);
	GPIO_EnableOutput(MODEM_DTR, GPIO_PIN_RESET);
	GPIO_EnableInput(MODEM_DCD, GPIO_Pull_None);

	while(1)
	{
		CORE_Idle();
	}
}

