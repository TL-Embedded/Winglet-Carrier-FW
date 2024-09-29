#ifndef SCPI_H
#define SCPI_H

#include "STM32X.h"

/*
 * PUBLIC DEFINITIONS
 */

#ifndef SCPI_BUFFER_SIZE
#define SCPI_BUFFER_SIZE	128
#endif
#ifndef SCPI_ARGS_MAX
#define SCPI_ARGS_MAX		4
#endif

#define SCPI_ARG_BOOL		'b'
#define SCPI_ARG_INT		'i'
#define SCPI_ARG_NUMBER		'n'
#define SCPI_ARG_STRING		's'

/*
 * PUBLIC TYPES
 */

typedef struct SCPI_s SCPI_t;

typedef struct {
	bool present;
	union {
		int32_t number;
		bool boolean;
		const char * string;
	};
} SCPI_Arg_t;

typedef struct {
	const char * pattern;
	bool (*func)(SCPI_t *, SCPI_Arg_t * args);
} SCPI_Node_t;

typedef struct SCPI_s{
	const SCPI_Node_t * nodes;
	uint32_t node_count;
	void (*write)(const uint8_t * data, uint32_t size);
	struct {
		char bfr[SCPI_BUFFER_SIZE];
		uint32_t size;
	} rx;
} SCPI_t;

/*
 * PUBLIC FUNCTIONS
 */

void SCPI_Init(SCPI_t * scpi, const SCPI_Node_t * nodes, uint32_t node_count, void (*write)(const uint8_t*, uint32_t));
void SCPI_Parse(SCPI_t * scpi, const uint8_t * data, uint32_t size);

// Command handlers: Output
void SCPI_Reply_Printf(SCPI_t * scpi, const char * fmt, ...);
void SCPI_Reply_Error(SCPI_t * scpi);
void SCPI_Reply_Number(SCPI_t * scpi, int32_t value, uint32_t precision);
void SCPI_Reply_Bool(SCPI_t * scpi, bool value);
void SCPI_Reply_Int(SCPI_t * scpi, int32_t value);

/*
 * UTIL DEFINITIONS
 */

#define SCPI_NODE_FUNC(_name, _run, _query)	{\
		.name = _name,\
		.run = _run,\
		.query = _query,\
	}

#define SCPI_NODE_MENU(_name, ...)	{\
		.name = _name,\
		.nodes = (const SCPI_Node_t*[]){__VA_ARGS__},\
		.node_count = (sizeof((const void*[]){__VA_ARGS__})/sizeof(const void*)),\
	}

#endif // SCPI_H
