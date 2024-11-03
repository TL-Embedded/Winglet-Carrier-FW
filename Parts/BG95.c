#include "BG95.h"

#include "Core.h"
#include "GPIO.h"
#include "AT.h"

// Would be nice to be independant of this.
#include "Console.h"

#include <stdio.h>
#include <stdarg.h>

/*
 * PRIVATE DEFINITIONS
 */

#define BG95_HTTP_TIMEOUT_S		20

#define TASK_PENDING(_task)		(gBG95.tasks & _task)
#define TASK_SET(_task)			(gBG95.tasks |= _task)
#define TASK_CLEAR(_task)		(gBG95.tasks &= ~_task)

/*
 * PRIVATE TYPES
 */

typedef enum {
	// Base state. UART off. Hopefully device in PSM
	BG95_Step_Standby,

	// Metastates used as helpers.
	BG95_Step_Error, // In an error state. Something has gone wrong.
	BG95_Step_Delay, // This step delays for a while before triggering another step

	// Startup/wakeup state machines.
	BG95_Step_Wakeup,
	BG95_Step_Reset,
	BG95_Step_WaitForReady,
	BG95_Step_ATE0,
	BG95_Step_AT,
	BG95_Step_Idle,

	// Configuration state machine
	BG95_Step_Configure,
	BG95_Step_SetPSM = BG95_Step_Configure,
	BG95_Step_SetQCFG_PSM,
	BG95_Step_SetQCFG_NWScan,
	BG95_Step_SetQCFG_Mode,
	BG95_Step_SetCOPS,
	BG95_Step_GetCREG,

	// Information state machine
	BG95_Step_Info,
	BG95_Step_GetRSSI = BG95_Step_Info,

	// Request state machines
	BG95_Step_HTTP,
	BG95_Step_SetURL = BG95_Step_HTTP,
	BG95_Step_SetURL_Content,
	BG95_Step_SetHTTPGET,
	BG95_Step_SetHTTPGET_Wait,
	BG95_Step_HTTPRead,
	BG95_Step_HTTPRead_Content,
	BG95_Step_SendCallback,
} BG95_Step_t;

typedef enum {
	BG95_Method_None,
	BG95_Method_Get,
	BG95_Method_Post,
} BG95_Method_t;

typedef enum {
	BG95_Task_Configure = 1 << 0,
	BG95_Task_Info		= 1 << 1,
	BG95_Task_HTTP		= 1 << 2,
} BG95_Task_t;


/*
 * PRIVATE PROTOTYPES
 */

// Step execution
static void BG95_EnterStep(BG95_Step_t step);
static BG95_Step_t BG95_RunStep(BG95_Step_t step);

// Utility
static void BG95_Log(const char * fmt, ...);

/*
 * PRIVATE VARIABLES
 */

static struct {
	BG95_Step_t step;
	BG95_Task_t tasks;

	struct {
		BG95_Step_t next_step;
		uint32_t delay;
	} delay;

	struct {
		BG95_Method_t method;
		BG95_RequestCallback_t callback;
		const char * url;
		uint32_t url_len;
		uint8_t * bfr;
		uint32_t bfr_size;
		uint32_t response_size;
		uint32_t status;
	} http;

} gBG95;

/*
 * PUBLIC FUNCTIONS
 */

void BG95_Init(void)
{
	GPIO_EnableOutput(BG95_RST_PIN, GPIO_PIN_RESET);
	// The state change below should fix any other variables.
	gBG95.tasks = 0;
	gBG95.step = BG95_Step_Standby;
	BG95_EnterStep(BG95_Step_Reset);
}

void BG95_Deinit(void)
{
	BG95_EnterStep(BG95_Step_Standby);
	GPIO_Deinit(BG95_RST_PIN);
}

void BG95_Update(void)
{
	BG95_Step_t step = BG95_RunStep(gBG95.step);
	if (step != gBG95.step)
	{
		BG95_EnterStep(step);
	}
}

void BG95_Reset(void)
{
	BG95_EnterStep(BG95_Step_Reset);
}

bool BG95_IsBusy(void)
{
	return gBG95.step != BG95_Step_Standby;
}

bool BG95_HttpGet(const char * url, uint8_t * bfr, uint32_t bfr_size, BG95_RequestCallback_t cb)
{
	BG95_Wakeup();

	if (BG95_HttpPending())
	{
		// Already a pending request. This request cannot be executed.
		return false;
	}

	TASK_SET(BG95_Task_HTTP);

	gBG95.http.method = BG95_Method_Get;
	gBG95.http.url = url;
	gBG95.http.bfr = bfr;
	gBG95.http.bfr_size = bfr_size;
	gBG95.http.callback = cb;
	gBG95.http.url_len = strlen(url);
	return true;
}

bool BG95_HttpPending(void)
{
	return TASK_PENDING(BG95_Task_HTTP);
}

void BG95_Wakeup(void)
{
	if (gBG95.step == BG95_Step_Standby)
	{
		GPIO_EnableInput(BG95_STAT_PIN, GPIO_Pull_Down);
		bool rx_high = GPIO_Read(BG95_STAT_PIN);
		GPIO_Deinit(BG95_STAT_PIN);

		// Wake the modem up.
		BG95_EnterStep(rx_high ? BG95_Step_AT : BG95_Step_Wakeup);
	}
}

/*
 * PRIVATE FUNCTIONS
 */

static void BG95_EnterStep(BG95_Step_t step)
{
	if (step != gBG95.step)
	{
		// During the step change, we may want the UART on/off.
		if (gBG95.step == BG95_Step_Standby)
		{
			TASK_SET(BG95_Task_Info);
			AT_Init();
		}
		else if (step == BG95_Step_Standby)
		{
			AT_Deinit();
		}

		if (step == BG95_Step_Reset)
		{
			TASK_SET(BG95_Task_Configure);
		}

		AT_StartCommand();

		// Might as well notify everyone of an error.
		if (step == BG95_Step_Error)
		{
			BG95_Log("Error %u", gBG95.step);
		}

		gBG95.step = step;
	}
}

static BG95_Step_t BG95_DelayStep(BG95_Step_t next_step, uint32_t delay)
{
	gBG95.delay.next_step = next_step;
	gBG95.delay.delay = delay;
	return BG95_Step_Delay;
}

static BG95_Step_t BG95_RunStep(BG95_Step_t step)
{
	AT_Status_t r;

	switch (step)
	{
	default:
	case BG95_Step_Standby:
		return step;

	case BG95_Step_Error:

		if (TASK_PENDING(BG95_Task_HTTP))
		{
			// Abort a pending HTTP transaction.
			TASK_CLEAR(BG95_Task_HTTP);
			if (gBG95.http.callback) { gBG95.http.callback(0, 0); }
		}
		return BG95_Step_Standby;

	case BG95_Step_Delay:
		AT_SetTimeout(gBG95.delay.delay);
		if (AT_GetTimeout())
		{
			return gBG95.delay.next_step;
		}
		return step;

	case BG95_Step_Wakeup:
	case BG95_Step_Reset:
		AT_SetTimeout(step == BG95_Step_Reset ? 2500 : 100);
		if (!AT_GetTimeout())
		{
			// Hold reset for 3 seconds to boot modem.
			GPIO_Set(BG95_RST_PIN);
			return step;
		}
		else
		{
			GPIO_Reset(BG95_RST_PIN);
			return BG95_Step_WaitForReady;
		}

	case BG95_Step_WaitForReady:
		AT_SetTimeout(12000);
		r = AT_ExpectMatch("APP RDY");
		if (r == AT_Unexpected) { return step; }
		break;

	case BG95_Step_ATE0:
		AT_Command("E0");
		r = AT_ExpectOk();
		if (r == AT_Unexpected) { return step; }
		break;

	case BG95_Step_AT:
		AT_Command(""); // This generates an AT.
		r = AT_ExpectOk();
		break;

	case BG95_Step_Idle:
		if (TASK_PENDING(BG95_Task_Configure))
		{
			return BG95_Step_Configure;
		}
		if (TASK_PENDING(BG95_Task_HTTP))
		{
			return BG95_Step_HTTP;
		}
		if (TASK_PENDING(BG95_Task_Info))
		{
			// Try to do the HTTP first.
			// We dont get a sane RSSI value wihout some comms.
			return BG95_Step_Info;
		}
		// Nothing else to do.
		return BG95_Step_Standby;

	case BG95_Step_SetPSM:
		// T3412, Periodic TAU: 24hr
		// T3324, Active time: 2s
		AT_Command("+QPSMS=1,,,\"00111000\",\"00000001\"");
		r = AT_ExpectOk();
		break;

	case BG95_Step_SetQCFG_PSM:
		// Enter PSM mode as soon as connection released.
		AT_Command("+QCFG=\"psm/enter\",1");
		r = AT_ExpectOk();
		break;

	case BG95_Step_SetQCFG_NWScan:
		// Scan for NBIOT then eMTC
		AT_Command("+QCFG=\"nwscanseq\",0302");
		r = AT_ExpectOk();
		break;

	case BG95_Step_SetQCFG_Mode:
		// Scan for NBIOT then eMTC
		AT_Command("+QCFG=\"iotopmode\",2,1");
		r = AT_ExpectOk();
		break;

	case BG95_Step_SetCOPS:
		AT_Command("+COPS=0");
		AT_SetTimeout(3000);
		r = AT_ExpectOk();
		if (r == AT_Ok)
		{
			TASK_CLEAR(BG95_Task_Configure);
			return BG95_DelayStep(++step, 3000);
		}
		break;

	case BG95_Step_GetCREG:
		AT_Command("+CEREG?");
		int status;
		r = AT_ExpectResponsef(1, "+CEREG: 0,%d", &status);
		if (r == AT_Ok)
		{
			switch (status)
			{
			case 1: // Registered
			case 5: // Roaming
				BG95_Log("status: registered");
				return BG95_Step_Idle;
			case 2: // Searching
				BG95_Log("status: searching");
				// Check again in 3 seconds.
				// WARNING: no timeout on the loop this forms.
				return BG95_DelayStep(step, 3000);
			case 0: // Not searching
				BG95_Log("status: search stopped");
				return BG95_Step_Error;
			default:
			case 3:  // Resistration denied
				BG95_Log("status: registration denied");
				return BG95_Step_Error;
			}
		}
		break;

	case BG95_Step_GetRSSI:
		AT_Command("+QCSQ");
		int rssi = 0;
		char mode[16];
		r = AT_ExpectResponsef(1, "+QCSQ: \"%[^\"]\",%d", mode, &rssi);
		if (r == AT_Ok)
		{
			TASK_CLEAR(BG95_Task_Info);
			BG95_Log("%s: %d dBm", mode, rssi);
			return BG95_Step_Idle;
		}
		break;

	case BG95_Step_SetURL:
		AT_Commandf("+QHTTPURL=%u", (int)gBG95.http.url_len);
		r = AT_ExpectMatch("CONNECT");
		break;

	case BG95_Step_SetURL_Content:
		AT_CommandRaw((uint8_t*)gBG95.http.url, gBG95.http.url_len);
		r = AT_ExpectOk();
		break;

	case BG95_Step_SetHTTPGET:
		AT_Commandf("+QHTTPGET=%u", BG95_HTTP_TIMEOUT_S);
		r = AT_ExpectOk();
		break;

	case BG95_Step_SetHTTPGET_Wait:
		AT_SetTimeout((BG95_HTTP_TIMEOUT_S + 1) * 1000);
		int response_code = 0, response_status = 0, response_size = 0;
		r = AT_ExpectMatchf(1, "+QHTTPGET: %u,%u,%u",
			&response_code, &response_status, &response_size
		);
		if (r == AT_Ok)
		{
			gBG95.http.status = response_status; // These parameters may not be recieved.
			gBG95.http.response_size = response_size;

			if (response_code != 0)
			{
				// This could be an HTTP failure? Not necessarily a connection issue.
				BG95_Log("get failure");
				return BG95_Step_SendCallback;
			}

			BG95_Log("get %u (%u bytes)", gBG95.http.status, gBG95.http.response_size);
			if (gBG95.http.bfr_size > 0 && gBG95.http.response_size > 0)
			{
				if (gBG95.http.bfr_size < gBG95.http.response_size)
				{
					// We cant read this.
					BG95_Log("get buffer too small: buffer %d, recieved: %d", gBG95.http.bfr_size, gBG95.http.response_size);
					gBG95.http.response_size = 0;
					return BG95_Step_SendCallback;
				}

				// Lets read the request.
				return BG95_Step_HTTPRead;
			}

			// User doesnt want the request. Its fine.
			return BG95_Step_SendCallback;
		}
		break;

	case BG95_Step_HTTPRead:
		AT_Command("+QHTTPREAD");
		r = AT_ExpectMatch("CONNECT");
		break;

	case BG95_Step_HTTPRead_Content:
		// WARN: If more content is returned than can be parsed, this will gum up later requests....
		AT_SetTimeout(5000);
		r = AT_ExpectRaw(gBG95.http.bfr, gBG95.http.response_size);
		break;

	case BG95_Step_SendCallback:
		if (gBG95.http.callback)
		{
			gBG95.http.callback(gBG95.http.status, gBG95.http.response_size);
		}
		TASK_CLEAR(BG95_Task_HTTP);
		return BG95_Step_Idle;
	}

	// Default cases top simplify unhandled cases above.
	switch (r)
	{
	case AT_Pending:	return step;
	case AT_Ok:			return ++step;
	default:			return BG95_Step_Error;
	}
}


static void BG95_Log(const char * fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	char bfr[64];
	vsnprintf(bfr, sizeof(bfr), fmt, va);
	va_end(va);

	Console_Print("BG95: %s", bfr);
}


