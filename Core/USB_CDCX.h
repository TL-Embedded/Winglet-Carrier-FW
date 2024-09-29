#ifndef USB_CDCX_H
#define USB_CDCX_H

#include "STM32X.h"
#include "usb/USB_Defs.h"

/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

// Callbacks for USB_CTL.
// These should be referenced in USB_Class.h
void USB_CDCX_Init(uint8_t config);
void USB_CDCX_Deinit(void);
void USB_CDCX_Setup(uint8_t port, USB_SetupRequest_t * req);

// Interface to user
uint32_t USB_CDCX_ReadReady(uint8_t port);
uint32_t USB_CDCX_Read(uint8_t port, uint8_t * data, uint32_t count);
void USB_CDCX_Write(uint8_t port, const uint8_t * data, uint32_t count);
void USB_CDCX_WriteStr(uint8_t port, const char * str);

/*
 * EXTERN DECLARATIONS
 */

#endif //USB_CUSTOM_H
