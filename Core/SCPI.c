#include "SCPI.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * PRIVATE DEFINITIONS
 */

#define ASCII_BIT_LOWER				('a' - 'A')
#define IS_NULL_OR_WHITESPACE(_ch)	(((_ch) & (~' ')) == 0)
#define IS_WHITESPACE(_ch)			((_ch) == ' ')
#define IS_ALPHA(_ch)				(((_ch) & ~ASCII_BIT_LOWER) >= 'A' && ((_ch) & ~ASCII_BIT_LOWER) <= 'Z')
#define IS_NAME_END(_ch)			(IS_NULL_OR_WHITESPACE(_ch) || ((_ch) == '!' || (_ch) == '?'))

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static void SCPI_Error(SCPI_t * scpi);

static bool SCPI_ParseLine(SCPI_t * scpi, char * str);
static const SCPI_Node_t * SCPI_ParseNode(const SCPI_Node_t * nodes, uint32_t node_count, const char ** pattern, char ** str, SCPI_Arg_t ** args);
static bool SCPI_ParseArguments(SCPI_Arg_t * args, const char * name, char ** str);
static bool SCPI_MatchName(const char ** name, const char ** str);
static char * SCPI_GetToken(char ** str, bool terminate);

static bool SCPI_DecodeInt(const char * token, int32_t * value);
static bool SCPI_DecodeBool(const char * token, bool * arg);
static bool SCPI_DecodeNumber(const char * token, int32_t * value, int32_t precision);
static bool SCPI_DecodeUnit(char * token, int32_t * value, int32_t precision, const char * unit);
static bool SCPI_DecodeBytes(const char * token, uint8_t * dst, uint32_t * size);

static char SCPI_ToHex(uint8_t n);
static int32_t SCPI_FromHex(char x);

/*
 * PRIVATE VARIABLES
 */

/*
 * PUBLIC FUNCTIONS
 */

void SCPI_Init(SCPI_t * scpi, const SCPI_Node_t * nodes, uint32_t node_count, void (*write)(const uint8_t*, uint32_t))
{
	scpi->nodes = nodes;
	scpi->node_count = node_count;
	scpi->write = write;
	scpi->rx.size = 0;
}

void SCPI_Parse(SCPI_t * scpi, const uint8_t * data, uint32_t size)
{
	while (size--)
	{
		char ch = (char)*data++;
		switch (ch)
		{
		case '\r':
		case '\n':
			// Do not error on empty lines.
			if (scpi->rx.size == 0)
			{
				continue;
			}
			scpi->rx.bfr[scpi->rx.size] = 0;
			scpi->rx.size = 0;
			if (!SCPI_ParseLine(scpi, scpi->rx.bfr))
			{
				SCPI_Error(scpi);
			}
			break;
		default:
			// Leave room for a null char. The token parser will need this.
			if (scpi->rx.size >= sizeof(scpi->rx.bfr) - 2)
			{
				SCPI_Error(scpi);
			}
			else
			{
				scpi->rx.bfr[scpi->rx.size++] = ch;
			}
			break;
		}
	}
}

// Command handlers: Output
void SCPI_Reply_Printf(SCPI_t * scpi, const char * fmt, ...)
{
	char bfr[SCPI_BUFFER_SIZE];
	va_list va;
	va_start(va, fmt);
	uint32_t size = vsnprintf(bfr, sizeof(bfr)-3, fmt, va);
	va_end(va);
	bfr[size++] = '\r';
	bfr[size++] = '\n';
	bfr[size] = 0;
	scpi->write((uint8_t*)bfr, size);
}

void SCPI_Reply_Error(SCPI_t * scpi)
{
	SCPI_Reply_Printf(scpi, "ERROR");
}

void SCPI_Reply_Number(SCPI_t * scpi, int32_t value, uint32_t precision)
{
	bool negative = false;
	if (value < 0)
	{
		value = -value;
		negative = true;
	}

	int power = 1;
	for (uint32_t i = 0; i < precision; i++) { power *= 10; }

	int high = value / power;
	int low = value % power;

	const char * fmt = "-%d.%0*d";
	if (!negative) { fmt++; }
	SCPI_Reply_Printf(scpi, fmt, high, precision, low);
}

void SCPI_Reply_Bool(SCPI_t * scpi, bool value)
{
	SCPI_Reply_Printf(scpi, value ? "ON" : "OFF");
}

void SCPI_Reply_Int(SCPI_t * scpi, int32_t value)
{
	SCPI_Reply_Printf(scpi, "%d", value);
}

void SCPI_Reply_Bytes(SCPI_t * scpi, uint8_t * data, uint32_t size)
{
	uint8_t bfr[size * 2 + 3];
	uint32_t pos = 0;

	for (uint32_t i = 0; i < size; i++)
	{
		uint8_t b = *data++;
		bfr[pos++] = SCPI_ToHex(b >> 4);
		bfr[pos++] = SCPI_ToHex(b & 0x0F);
	}

	bfr[pos++] = '\r';
	bfr[pos++] = '\n';
	bfr[pos] = 0;
	scpi->write((uint8_t*)bfr, pos);
}

/*
 * PRIVATE FUNCTIONS: PARSING & EXECUTION
 */

static char SCPI_ToHex(uint8_t n)
{
	if (n > 9)
	{
		return 'a' - 10 + n;
	}
	return '0' + n;
}

static int32_t SCPI_FromHex(char x)
{
	if (x >= '0' && x <= '9')
	{
		return x - '0';
	}

	// Discard case info
	x &= ~('a' - 'A');

	if (x >= 'A' && x <= 'f')
	{
		return x - 'A' + 10;
	}

	return -1;
}

static const SCPI_Node_t * SCPI_ParseNode(const SCPI_Node_t * nodes, uint32_t node_count, const char ** pattern, char ** str, SCPI_Arg_t ** args)
{
	uint32_t match_depth = 0;
	char * head = *str;

	bool star_command = false;
	if (*head == '*')
	{
		head++;
		star_command = true;
	}

	for (uint32_t i = 0; i < node_count; i++)
	{
		const char * ptrn = nodes[i].pattern;

		if (*ptrn == '*')
		{
			if (!star_command) { continue; }
			ptrn++;
		}
		else if (star_command) { continue; }

		uint32_t depth = 0;
		// Blank node blocks automatically match to previous blocks.
		while (*ptrn == ':')
		{
			if (depth >= match_depth)
			{
				break;
			}
			depth += 1;
			ptrn++;
		}

		// No more matches possible.
		// We do not support walk back of the str.
		if (depth != match_depth) { break; }

		while (SCPI_MatchName(&ptrn, (const char **)&head))
		{
			match_depth += 1;

			if (*ptrn == '#')
			{
			    ptrn++;
			    if (*head >= '0' && *head <= '9')
			    {
			        (*args)->present = true;
    				(*args)->number = strtol(head, &head, 10);
			    }
			    (*args)++;

				// If there are trailing characters in the head, then later matches will fail.
				// This is good.
			}

			if (*head == ':')
			{
				head++;
				if (*ptrn == ':')
				{
					ptrn++;
					continue;
				}
			}
			break;
		}

		// At least one match needs to have occurred
		if (!(match_depth > depth)) { continue; }

		if (IS_NAME_END(*head) && IS_NAME_END(*ptrn))
		{
			*str = head;
			*pattern = ptrn;
			return &nodes[i];
		}
	}

	return NULL;
}

static bool SCPI_ParseLine(SCPI_t * scpi, char * str)
{
	SCPI_Arg_t args[SCPI_ARGS_MAX] = {0};
	SCPI_Arg_t * arg_head = args;

	const char * pattern;
	const SCPI_Node_t * node = SCPI_ParseNode(scpi->nodes, scpi->node_count, &pattern, &str, &arg_head);
	if (!node || !node->func)
	{
		return false;
	}

	bool can_query = true;
	bool can_run = true;
	if (*pattern == '?')
	{
		pattern++;
		can_run = false;
	}
	else if (*pattern == '!')
	{
		pattern++;
		can_query = false;
	}

	scpi->is_query = *str == '?';

	if (scpi->is_query)
	{
		if (can_query)
		{
			str++;
			if (*str == 0)
			{
				// This is a properly formatted query command.
				return node->func(scpi, args);
			}
		}
	}
	else
	{
		if (can_run)
		{
			if (SCPI_ParseArguments(arg_head, pattern, &str) && *str == 0)
			{
				// Argument parsing succeeded, and the line is terminated.
				return node->func(scpi, args);
			}
		}
	}
	return false;
}

static bool SCPI_ParseArgument(SCPI_Arg_t * arg, const char * fmt, char * token)
{
	bool optional = *fmt == '?';
	if (optional) { fmt++; }

	if (token == NULL)
	{
		if (optional)
		{
			arg->present = false;
			return true;
		}
		return false;
	}

	arg->present = true;

	uint32_t precision;

	switch (*fmt++)
	{
	case SCPI_ARG_BOOL:
		return SCPI_DecodeBool(token, &arg->boolean);
	case SCPI_ARG_NUMBER:
		precision = strtol(fmt, (char**)&fmt, 10);
		return SCPI_DecodeNumber(token, &arg->number, precision);
	case SCPI_ARG_UNIT:
	    precision = strtol(fmt, (char**)&fmt, 10);
	    return SCPI_DecodeUnit(token, &arg->number, precision, fmt);
	case SCPI_ARG_INT:
		return SCPI_DecodeInt(token, &arg->number);
	case SCPI_ARG_STRING:
		arg->string = token;
		return true;
	case SCPI_ARG_BYTES:
		arg->bytes.bfr = (uint8_t*)token;
		// We use our token as our dst buffer. We just rely on the fact that DecodeBytes produces less bytes than it reads.
		return SCPI_DecodeBytes(token, (uint8_t*)arg->bytes.bfr, &arg->bytes.size);
	default:
		break;
	}
	return false;
}

static bool SCPI_ParseArguments(SCPI_Arg_t * args, const char * pattern, char ** str)
{
	for (int i = 0; i < SCPI_ARGS_MAX; i++)
	{
		char * format = SCPI_GetToken((char**)&pattern, false);
		char * token = SCPI_GetToken(str, true);

		if (token == NULL && **str != 0)
		{
			// We must have failed to decode an argument.
			return false;
		}

		if (format == NULL)
		{
			if (token != NULL)
			{
				// More arguments than supported.
				return false;
			}
			// No more args. We are done.
			break;
		}

		if (!SCPI_ParseArgument(args + i, format, token))
		{
			return false;
		}
	}
	return true;
}

static char * SCPI_GetToken(char ** str, bool terminate)
{
	char * head = *str;

	// Skip whitespace
	while (IS_WHITESPACE(*head)) { head++; }

	if (*head == 0 || *head == ',')
	{
		return NULL;
	}

	char * token;
	char * end;

	if (*head == '"')
	{
		// Token in in quotes.
		token = ++head;
		while (!(*head == 0 || *head == '"')) { head++; }
		// Check we actually found quotes.
		if (*head != '"') { return NULL; }
		end = head++;
	}
	else
	{
		// Read out the token.
		token = head;
		while (!IS_NULL_OR_WHITESPACE(*head) && *head != ',') { head++; }
		end = head;
	}

	// Skip more whitespace
	while (IS_WHITESPACE(*head)) { head++; }

	if (*head == ',')
	{
		// Skip the comma
		head++;
	}
	else if (*head != 0)
	{
		// Uh oh. Another token without a separator.
		// This is a malformed arg.
		return NULL;
	}

	if (terminate) { *end = 0; }

	*str = head;
	return token;
}

static void SCPI_Error(SCPI_t * scpi)
{
	scpi->rx.size = 0;
	SCPI_Reply_Error(scpi);
}

static bool SCPI_MatchName(const char ** name, const char ** str)
{
	const char * name_head = *name;
	const char * str_head = *str;
	while (1)
	{
		if (*name_head == '#')
		{
			// This is complete for now. The outer steps should check for this.
			*name = name_head;
			*str = str_head;
			return true;
		}

		if (!IS_ALPHA(*name_head))
		{
			if (!IS_ALPHA(*str_head))
			{
				// Complete match
				*name = name_head;
				*str = str_head;
				return true;
			}

			// str is too long to match.
			return false;
		}
		else if (!IS_ALPHA(*str_head))
		{
			// str is too short. Perhaps it is the short form?
			if (*name_head & ASCII_BIT_LOWER)
			{
				// Advance till the end of the name.
				while (IS_ALPHA(*name_head)) { name_head++; }
				*name = name_head;
				*str = str_head;
				return true;
			}
			return false;
		}

		if ((*str_head ^ *name_head) & ~ASCII_BIT_LOWER)
		{
			return false;
		}

		name_head++;
		str_head++;
	}
}


/*
 * PRIVATE FUNCTIONS: ARGUMENT DECODERS
 */

static bool SCPI_DecodeInt(const char * token, int32_t * value)
{
	char * end;
	*value = strtol(token, &end, 0);
	return *end == 0;
}

static bool SCPI_DecodeBool(const char * token, bool * arg)
{
	int32_t ivalue;
	if (strcmp(token, "ON") == 0)
	{
		*arg = true;
		return true;
	}
	else if (strcmp(token, "OFF") == 0)
	{
		*arg = false;
		return true;
	}
	else if (SCPI_DecodeInt(token, &ivalue))
	{
		*arg = ivalue != 0;
		return true;
	}
	return false;
}

static int32_t SCPI_BaseShift(int32_t n, int32_t power)
{
	while (power > 0) {
		n *= 10;
		power--;
	}
	while (power < 0) {
		n /= 10;
		power++;
	}
	return n;
}

static bool SCPI_DecodeNumber(const char * token, int32_t * value, int32_t precision)
{
	// TODO: This doesnt handle overflow
	uint32_t low = 0;
	bool negative = *token == '-';

	char * end;
	int32_t high = strtol(token, &end, 10);

	if (*end == '.')
	{
		char * start = end + 1;
		if (*start >= '0' && *start <= '9')
		{
			low = strtol(start, &end, 10);
			int32_t digits = end - start;
			low = SCPI_BaseShift(low, precision - digits);
		}

		if (negative)
		{
			low = -low;
		}
	}

	if (*end == 0)
	{
		high = SCPI_BaseShift(high, precision);
		*value = high + low;
		return true;
	}

	return false;
}

static bool SCPI_DecodeUnit(char * token, int32_t * value, int32_t precision, const char * unit)
{
    uint32_t unit_len = 0;
    while ( IS_ALPHA(unit[unit_len])) { unit_len++; }

    uint32_t token_len = strlen(token);
    if ( token_len < unit_len + 1 || strncmp( token + token_len - unit_len, unit, unit_len) != 0)
    {
        // Unit has not been specified
        return false;
    }

    char si_prefix = token[token_len - unit_len - 1];
    if (si_prefix >= '0' && si_prefix <= '9')
    {
        // Number. So no e notation.
        token[token_len - unit_len] = 0;
    }
    else
    {
    	int32_t power = 0;
        switch (si_prefix)
        {
        case 'T':
        	power = 12;
            break;
        case 'G':
        	power = 9;
            break;
        case 'M':
        	power = 6;
            break;
        case 'k':
        	power = 3;
            break;
        case 'm':
        	power = -3;
            break;
        case 'u':
        	power = -6;
            break;
        case 'n':
        	power = -9;
            break;
        case 'p':
        	power = -12;
            break;
        default:
            // Unknown unit specifier
            return false;
        }
        precision += power;
        token[token_len - unit_len - 1] = 0;
    }
    return SCPI_DecodeNumber(token, value, precision);
}

static bool SCPI_DecodeBytes(const char * token, uint8_t * dst, uint32_t * size)
{
	uint32_t read = 0;

	while (*token != 0)
	{
		// We rely on the second SCPI_FromHex to gracefully return -1 if its passed a null byte
		int32_t high = SCPI_FromHex(*token++);
		int32_t low = SCPI_FromHex(*token++);
		if (high < 0 || low < 0)
		{
			return false;
		}
		*dst++ = (high << 4) | low;
		read++;
	}
	*size = read;
	return true;
}

/*
 * INTERRUPT ROUTINES
 */
