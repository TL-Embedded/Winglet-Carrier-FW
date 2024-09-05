#ifndef CONSOLE_H
#define CONSOLE_H

#include "STM32X.h"
#include <stdio.h>

/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

void Console_Init(void);
void Console_Deinit(void);

bool Console_IsEnabled(void);

// Printing
void Console_Prints(const char * str);
void Console_Printf(const char * fmt, ...)
	_ATTRIBUTE ((__format__ (__printf__, 1, 2)));

#ifdef CONSOLE_RX_BFR
// Scanning
const char * Console_Scans(void);
uint32_t Console_Scanf(const char * fmt, ...)
	_ATTRIBUTE ((__format__ (__scanf__, 1, 2)));
#endif

// Provides raw read/write functions as a common interface to other modules.
void Console_Write(const char * bfr, uint32_t size);
uint32_t Console_Read(char * bfr, uint32_t size);

/*
 * EXTERN DECLARATIONS
 */


#endif // CONSOLE_H
