
#include "Core.h"

#include "USB.h"


int main(void)
{
	CORE_Init();
	USB_Init();

	while(1)
	{
		USB_CDC_WriteStr("yo dog\r\n");
		CORE_Delay(1000);
	}
}

