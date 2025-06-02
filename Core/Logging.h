#ifndef LOGGING_H
#define LOGGING_H

#include "STM32X.h"
#include <stdio.h>

/*
 * PUBLIC DEFINITIONS
 */


#define LOG_LEVEL_INFO			0
#define LOG_LEVEL_WARN			1
#define LOG_LEVEL_ERROR			2
#define LOG_LEVEL_SILENT		3

#ifndef LOG_MIN_LEVEL
#define LOG_MIN_LEVEL			LOG_LEVEL_INFO
#endif


// Macro sludge to convert from the Log_x type to Log_Print.
#ifdef LOG_PRINT_LEVEL
#define LOG_PRINT_LEVEL_FMT(_level)				_level,
#else
#define LOG_PRINT_LEVEL_FMT(_level)
#endif

#ifdef LOG_PRINT_FILE
#define LOG_PRINT_FILE_FMT						__FILE_NAME__,
#else
#define LOG_PRINT_FILE_FMT
#endif

#ifdef LOG_PRINT_SOURCE
#define LOG_PRINT_SOURCE_FMT					LOG_SOURCE,
#else
#define LOG_PRINT_SOURCE_FMT
#endif

#define LOG_PRINT_FMT(_level, ...)				Log_Print(LOG_PRINT_LEVEL_FMT(_level) LOG_PRINT_FILE_FMT LOG_PRINT_SOURCE_FMT __VA_ARGS__)

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

// Common Log_Print function.
// Dont use this directly.
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
		const char * fmt, ...);


// Implementaion of Log_Info
#if (LOG_MIN_LEVEL <= LOG_LEVEL_INFO)
#define Log_Info(...)			LOG_PRINT_FMT(LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define Log_Info(...)			((void)0)
#endif

// Implementaion of Log_Warn
#if (LOG_MIN_LEVEL <= LOG_LEVEL_WARN)
#define Log_Warn(...)			LOG_PRINT_FMT(LOG_LEVEL_WARN, __VA_ARGS__)
#else
#define Log_Warn(...)			((void)0)
#endif

// Implementaion of Log_Error
#if (LOG_MIN_LEVEL <= LOG_LEVEL_ERROR)
#define Log_Error(...)			LOG_PRINT_FMT(LOG_LEVEL_ERROR, __VA_ARGS__)
#else
#define Log_Error(...)			((void)0)
#endif

/*
 * EXTERN DECLARATIONS
 */


#endif // LOGGING_H
