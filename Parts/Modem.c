
#include "Modem.h"
#include "M24xx.h"
#include "I2C.h"

/*
 * PRIVATE DEFINITIONS
 */

#define MODEM_ID_MAX			32

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static bool Modem_GetId(char * str);

/*
 * PRIVATE VARIABLES
 */

/*
 * PUBLIC FUNCTIONS
 */

bool Modem_Init(void)
{

}

void Modem_Deinit(void)
{

}

/*
 * PRIVATE FUNCTIONS
 */

static bool Wing_GetId(char * str)
{
	I2C_Init(M24XX_I2C, I2C_Mode_Fast);

	bool success = false;
	uint8_t size;
	if (M24XX_Init() && M24XX_Read(0, &size, 1))
	{
		if ( size < WINGLET_ID_MAX && M24XX_Read(1, str, size) )
		{
			str[size] = 0;
			success = true;
		}
	}

	I2C_Deinit(M24XX_I2C);
	return success;
}

/*
 * INTERRUPT ROUTINES
 */

