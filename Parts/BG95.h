#ifndef BG95_H
#define BG95_H

#include "STM32X.h"

/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

typedef void (*BG95_RequestCallback_t)(uint32_t code, uint32_t rx_size);

/*
 * PUBLIC FUNCTIONS
 */

void BG95_Init(void);
void BG95_Deinit(void);

void BG95_Update(void);
bool BG95_IsBusy(void);

void BG95_Reset(void);
void BG95_Wakeup(void);

// Starts a HTTP Request.
// url is expected to remain valid until the callback has been called.
// cb will be called once the request has returned, with the response code and the total received buffer length
// Returns true if the request was successfully enqueued. False if the modem was not ready for it.
bool BG95_HttpGet(const char * url, uint8_t * bfr, uint32_t bfr_size, BG95_RequestCallback_t cb);
bool BG95_HttpPending(void);

/*
 * EXTERN DECLARATIONS
 */

#endif // MODEM_H





