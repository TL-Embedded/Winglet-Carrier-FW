#ifndef M24xx_H
#define M24xx_H

#include "STM32X.h"


/*
 * PUBLIC DEFINITIONS
 */

#if (M24XX_SERIES == 0)
#define M24XX_SIZE      128
#else
#define M24XX_SIZE     (M24XX_SERIES * 128)
#endif

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

// This only checks for the presence of the EEPROM.
bool M24xx_Init(void);

// Write data to the EEPROM
bool M24xx_Write(uint32_t pos, const uint8_t * bfr, uint32_t size);

// Read data from the EEPROM
bool M24xx_Read(uint32_t pos, uint8_t * bfr, uint32_t size);

/*
 * EXTERN DECLARATIONS
 */

#endif //M24xx_H
