#include "Console.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef CONSOLE_UART
#include "UART.h"
#endif
#ifdef CONSOLE_USB
#include "USB.h"
#include "usb/cdc/USB_CDC.h"
#endif

/*
 * PRIVATE DEFINITIONS
 */

#ifndef CONSOLE_BAUD
#define CONSOLE_BAUD		115200
#endif

#ifndef CONSOLE_TX_BFR
#define CONSOLE_TX_BFR		128
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

static struct {
	bool enabled;
#ifdef CONSOLE_RX_BFR
	struct {
		char bfr[CONSOLE_RX_BFR];
		uint32_t size;
		uint32_t start;
	} rx;
#endif
} gConsole;

/*
 * PUBLIC FUNCTIONS
 */

void Console_Init(void)
{
#ifdef CONSOLE_UART
	UART_Init(CONSOLE_UART, CONSOLE_BAUD, UART_Mode_Default);
#endif
	gConsole.enabled = true;
#ifdef CONSOLE_RX_BFR
	gConsole.rx.size = 0;
	gConsole.rx.start = 0;
#endif
}

void Console_Deinit(void)
{
#ifdef CONSOLE_UART
	UART_Deinit(CONSOLE_UART);
#endif
	gConsole.enabled = false;
}

void Console_Prints(const char * str)
{
	if (Console_IsEnabled())
	{
		Console_Write(str, strlen(str));
		Console_Write("\r\n", 2);
	}
}

void Console_Printf(const char * fmt, ...)
{
	if (Console_IsEnabled())
	{
		va_list va;
		va_start(va, fmt);
		char bfr[CONSOLE_TX_BFR];
		uint32_t written = vsnprintf(bfr, sizeof(bfr)-2, fmt, va);
		va_end(va);
		bfr[written++] = '\r';
		bfr[written++] = '\n';
		Console_Write(bfr, written);
	}
}

#ifdef CONSOLE_RX_BFR
const char * Console_Scans(void)
{
	if (Console_IsEnabled())
	{
		uint32_t search_start = gConsole.rx.size;

		if (gConsole.rx.start)
		{
			if (gConsole.rx.start < gConsole.rx.size)
			{
				// Our start point is not at zero. Fix this.
				gConsole.rx.size -= gConsole.rx.start;
				memmove(gConsole.rx.bfr, gConsole.rx.bfr + gConsole.rx.start, gConsole.rx.size);
			}
			else
			{
				gConsole.rx.size = 0;
			}

			gConsole.rx.start = 0;
			search_start = 0;
		}

		// Read any data into our buffer.
		gConsole.rx.size += Console_Read(gConsole.rx.bfr + gConsole.rx.size, sizeof(gConsole.rx.bfr) - gConsole.rx.size);

		if (gConsole.rx.size > search_start)
		{
			// Check new characters for a line ending
			for (uint32_t i = search_start; i < gConsole.rx.size; i++)
			{
				char ch = gConsole.rx.bfr[i];
				if (ch == '\n' || ch == '\r')
				{
					uint32_t last_start = gConsole.rx.start;
					gConsole.rx.start = i + 1;
					if (i > last_start)
					{
						// We found a line. Terminate the string and return.
						gConsole.rx.bfr[i] = 0;
						return gConsole.rx.bfr + last_start;
					}
				}
			}

			if (gConsole.rx.size == sizeof(gConsole.rx.bfr))
			{
				// Overflow occurred. Abandon ship.
				gConsole.rx.size = 0;
				gConsole.rx.start = 0;
			}
		}
	}
	return NULL;
}

uint32_t Console_Scanf(const char * fmt, ...)
{
	const char * line = Console_Scans();
	if (line)
	{
		va_list va;
		va_start(va, fmt);
		int parsed = vsscanf(line, fmt, va);
		va_end(va);
		return parsed;
	}
	return 0;
}

#endif

bool Console_IsEnabled(void)
{
#ifdef CONSOLE_USB
	return gConsole.enabled && USB_IsEnumerated();
#else
	return gConsole.enabled;
#endif
}

void Console_Write(const char * bfr, uint32_t size)
{
#ifdef CONSOLE_UART
	UART_Write(CONSOLE_UART, (uint8_t*)bfr, size);
#endif
#ifdef CONSOLE_USB
	USB_CDC_Write((uint8_t*)bfr, size);
#endif
}

uint32_t Console_Read(char * bfr, uint32_t size)
{
#ifdef CONSOLE_UART
	return UART_Read(CONSOLE_UART, (uint8_t*)bfr, size);
#endif
#ifdef CONSOLE_USB
	return USB_CDC_Read((uint8_t*)bfr, size);
#endif
}

/*
 * PRIVATE FUNCTIONS
 */

/*
 * INTERRUPT ROUTINES
 */
