
#include "Core.h"

#include "USB.h"
#include "Console.h"
#include "LED.h"


int main(void)
{
	CORE_Init();
	USB_Init();
	Console_Init();
	LED_Init();

	while(1)
	{
		LED_Write(LED_Color_Red);
		CORE_Delay(1000);
		LED_Write(LED_Color_Green);
		CORE_Delay(1000);
		LED_Write(LED_Color_Blue);
		CORE_Delay(1000);
	}
}

