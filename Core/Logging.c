#include "Logging.h"
#include "Core.h"

#include <stdio.h>
#include <stdarg.h>

#include "Console.h"

/*
 * PRIVATE DEFINITIONS
 */

#ifndef LOG_TX_BFR
#define LOG_TX_BFR		128
#endif

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */


#ifdef LOG_PRINT_LEVEL
const char * Console_GetLevelString(uint8_t level);
#endif

/*
 * PRIVATE VARIABLES
 */

/*
 * PUBLIC FUNCTIONS
 */

void Log_Print(
#ifdef LOG_PRINT_LEVEL
		uint8_t level,
#endif
#ifdef LOG_PRINT_FILE
		const char * file,
#endif
#ifdef LOG_PRINT_SOURCE
		const char * src,
#endif
		const char * fmt, ...)
{

	if (!Console_IsEnabled()) { return; }

	char bfr[LOG_TX_BFR];
	uint32_t written;

#if defined(LOG_PRINT_TIMESTAMP) || defined(LOG_PRINT_LEVEL) || defined(LOG_PRINT_SOURCE) || defined(LOG_PRINT_FILE)
#ifdef LOG_PRINT_TIMESTAMP
	uint32_t t = CORE_GetTick();
#endif
	written = snprintf(bfr, sizeof(bfr),

#if defined(LOG_PRINT_TIMESTAMP)
			"[%03d.%03d] "
#endif
#if defined(LOG_PRINT_LEVEL)
			"%s"
#endif
#if defined(LOG_PRINT_FILE)
#if defined(LOG_PRINT_LEVEL)
			" "
#endif
			"'%s'"
#endif
#if defined(LOG_PRINT_SOURCE)
#if defined(LOG_PRINT_LEVEL) || defined(LOG_PRINT_FILE)
			" "
#endif
			"%s"
#endif
#if defined(LOG_PRINT_LEVEL) || defined(LOG_PRINT_FILE) || defined(LOG_PRINT_SOURCE)
			": "
#endif

#if defined(LOG_PRINT_TIMESTAMP)
			,(int)(t / 1000), (int)(t % 1000)
#endif
#if defined(LOG_PRINT_LEVEL)
			,Console_GetLevelString(level)
#endif
#if defined(LOG_PRINT_FILE)
			,file
#endif
#if defined(LOG_PRINT_SOURCE)
			,src
#endif
			);
	Console_Write(bfr, written);
#endif //defined(LOG_PRINT_TIMESTAMP) || defined(LOG_PRINT_LEVEL) || defined(LOG_PRINT_SOURCE) || defined(LOG_PRINT_FILE)

	va_list va;
	va_start(va, fmt);
	written = vsnprintf(bfr, sizeof(bfr)-2, fmt, va);
	va_end(va);
	bfr[written++] = '\r';
	bfr[written++] = '\n';
	Console_Write(bfr, written);
}

/*
 * PRIVATE FUNCTIONS
 */

#ifdef LOG_PRINT_LEVEL
const char * Console_GetLevelString(uint8_t level)
{
	switch (level)
	{
	default:
	case LOG_LEVEL_INFO:
		return " INFO";
	case LOG_LEVEL_WARN:
		return " WARN";
	case LOG_LEVEL_ERROR:
		return "ERROR";
	}
}
#endif //LOG_PRINT_LEVEL


/*
 * INTERRUPT ROUTINES
 */
