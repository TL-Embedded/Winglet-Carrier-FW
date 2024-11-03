#include "AT.h"

#include "Core.h"
#include "UART.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * PRIVATE DEFINITIONS
 */

#ifndef AT_CMD_PREFIX
#define AT_CMD_PREFIX		"AT"
#endif
#ifndef AT_CMD_SUFFIX
#define AT_CMD_SUFFIX		"\r\n"
#endif
#ifndef AT_TX_BFR
#define AT_TX_BFR			128
#endif
#ifndef AT_RX_BFR
#define AT_RX_BFR			128
#endif
#ifndef AT_TIMEOUT_DEFAULT
#define AT_TIMEOUT_DEFAULT			300
#endif

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static void AT_ExpectClear(void);
static AT_Status_t AT_ExpectNext(char ** line, bool last_line);

/*
 * PRIVATE VARIABLES
 */

struct {
	bool command_sent;
	uint32_t command_start;
	uint32_t timeout;
	uint8_t line_count;
	uint32_t line_start;
	uint32_t line_head;
	char line_bfr[AT_RX_BFR];
} gAT;

/*
 * PUBLIC FUNCTIONS
 */

void AT_Init(void)
{
	UART_Init(AT_UART, AT_BAUD, UART_Mode_Default);
	AT_StartCommand();
}

void AT_Deinit(void)
{
	UART_Deinit(AT_UART);
}

// Resets the state of the Expect parsers and Command senders.
void AT_StartCommand(void)
{
	gAT.command_sent = false;
	gAT.command_start = CORE_GetTick();
	gAT.timeout = AT_TIMEOUT_DEFAULT;
	AT_ExpectClear();
}

void AT_SetTimeout(uint32_t timeout)
{
	gAT.timeout = timeout;
}

bool AT_GetTimeout(void)
{
	uint32_t elapsed = CORE_GetTick() - gAT.command_start;
	return elapsed >= gAT.timeout;
}

// Sends a single command
void AT_Command(const char * cmd)
{
	if (gAT.command_sent) { return; }

	UART_WriteStr(AT_UART, AT_CMD_PREFIX);
	UART_WriteStr(AT_UART, cmd);
	UART_WriteStr(AT_UART, AT_CMD_SUFFIX);

	gAT.command_sent = true;
}

// Sends a single formatted command
void AT_Commandf(const char * cmd, ...)
{
	if (gAT.command_sent) { return; }

	va_list va;
	va_start(va, cmd);
	char bfr[AT_TX_BFR];
	vsnprintf(bfr, sizeof(bfr), cmd, va);
	va_end(va);
	AT_Command(bfr);
}

// Sends a series of raw bytes
void AT_CommandRaw(const uint8_t * bfr, uint32_t size)
{
	if (gAT.command_sent) { return; }
	UART_Write(AT_UART, bfr, size);
	gAT.command_sent = true;
}

// Expects a single "OK".
// - AT_Ok: "OK" has been found.
// All other response codes are valid.
AT_Status_t AT_ExpectOk(void)
{
	return AT_ExpectMatch("OK");
}

// Expects a one-line reply followed by "OK".
// - AT_Ok: "OK" has been found. Reply stored in *response.
// All other response codes are valid.
AT_Status_t AT_ExpectResponse(char ** response)
{
	AT_Status_t r;
	if (gAT.line_count == 0)
	{
		r = AT_ExpectNext(response, false);
		if (r != AT_Ok) { return r; }
	}
	// Note, we may have reentered this function, so we always need to set the response ptr.
	*response = gAT.line_bfr;
	return AT_ExpectOk();
}

// Expects a one-line reply followed by "OK".
// The one-line reply will be parsed according to the formar string and argcount
// - AT_Ok: "OK" has been found. Parsed args stored in varargs
// - AT_Unexpected: Minimum number of args not parsed.
// All other response codes are valid.
AT_Status_t AT_ExpectResponsef(uint32_t min_args, const char * fmt, ...)
{
	char * line;
	AT_Status_t r = AT_ExpectResponse(&line);
	if (r == AT_Ok)
	{
		va_list va;
		va_start(va, fmt);
		int code = vsscanf(line, fmt, va);
		va_end(va);

		if (code < min_args) { r = AT_Unexpected; }
	}
	return r;
}

// Expects a matching line. This will continously parse until the expected match is found.
// - AT_Ok: match has been found, and the line returned in *response
// - AT_Unexpected: expected string not found
// All other response codes are valid.
AT_Status_t AT_ExpectMatch(const char * expected)
{
	char * line;
	AT_Status_t r = AT_ExpectNext(&line, true);
	if (r == AT_Ok)
	{
		return (strcmp(line, expected) == 0) ? AT_Ok : AT_Unexpected;
	}
	return r;
}

// Expects a line matching the provided format.
// - AT_Ok: match has been found with at least min_args, and results stored in varargs
// - AT_Unexpected: Minimum number of args not parsed.
// All other response codes are valid.
AT_Status_t AT_ExpectMatchf(uint32_t min_args, const char * fmt, ...)
{
	char * line;
	AT_Status_t r = AT_ExpectNext(&line, true);
	if (r == AT_Ok)
	{
		va_list va;
		va_start(va, fmt);
		int code = vsscanf(line, fmt, va);
		va_end(va);

		if (code < min_args) { r = AT_Unexpected; }
	}
	return r;
}

// Expects a block of flat bytes
// - AT_Ok: requested bytes have been read
// - AT_Timeout: requested timeout has been exceeded
AT_Status_t AT_ExpectRaw(uint8_t * bfr, uint32_t size)
{
	// Using gAT.line_head as our index. Hopefully this is kept clean....
	uint32_t remaining = size - gAT.line_head;
	gAT.line_head += UART_Read(AT_UART, bfr + gAT.line_head, remaining);

	if (gAT.line_head == size)
	{
		gAT.line_head = 0;
		return AT_Ok;
	}
	else if (AT_GetTimeout())
	{
		gAT.line_head = 0;
		return AT_Timeout;
	}
	return AT_Pending;
}

/*
 * PRIVATE FUNCTIONS
 */

// Clears the state of the line parser, readying us for a fresh ExpectX call.
static void AT_ExpectClear(void)
{
	gAT.line_head = 0;
	gAT.line_count = 0;
	gAT.line_start = 0;
}

// Parses a single line, adding it to the buffer.
// - AT_Ok: a line has been returned via *line.
// - AT_Overflow: a buffer overflow occurred.
// - AT_Pending: no line found yet.
// This should not be called directly - use AT_ExpectNext instead.
static AT_Status_t AT_ParseLine(char ** line)
{
	uint32_t rx_size = UART_ReadCount(AT_UART);

	if (rx_size)
	{
		char * bfr = gAT.line_bfr;

		while (rx_size--)
		{
			char ch = UART_Pop(AT_UART);

			if (ch == '\n')
			{
				uint32_t line_start = gAT.line_start;
				if (gAT.line_head > line_start)
				{
					// End the line
					bfr[gAT.line_head++] = 0;
					gAT.line_count++;
					gAT.line_start = gAT.line_head;
					*line = bfr + line_start;
					return AT_Ok;
				}

				// No point stopping at an empty line.
				// Keep going.
			}
			else if (ch == '\r' || ch == '\0') {
				// discard these characters.
			}
			else
			{
				// Check for overflow (we at least need room for a null char)
				if (gAT.line_head >= sizeof(gAT.line_bfr) - 1)
				{
					return AT_Overflow;
				}
				bfr[gAT.line_head++] = ch;
			}
		}
	}
	return AT_Pending;
}

// This gets the next line, and tries to check for common problems.
// - AT_Ok: a line has been returned via *line.
// - AT_Unexpected: a buffer/line overflow occurred.
// - AT_Pending: no line found yet.
// - AT_Timeout: supplied timeout is exceeded.
// - AT_Error: an explicit "ERROR" string was found.
// This also tries to be clever and do cleanup if needed.
// - If this is the last line
// - If the parse failed
static AT_Status_t AT_ExpectNext(char ** line, bool last_line)
{
	// Get the next line.
	AT_Status_t r = AT_ParseLine(line);

	if (r == AT_Pending)
	{
		// Not timed out yet. Stay parsing.
		if (!AT_GetTimeout()) { return r; }

		r = AT_Timeout;
	}
	else if (r == AT_Ok)
	{
		if (strcmp(*line, "ERROR") == 0)
		{
			r = AT_Error;
		}
	}

	// Is this the end of the command sequence?
	if (last_line || r != AT_Ok)
	{
		AT_ExpectClear();
	}
	return r;
}





