#ifndef USB_COMPOSITE_H
#define USB_COMPOSITE_H

#include "STM32X.h"
#include "usb/USB_Defs.h"

/*
 * PUBLIC DEFINITIONS
 */

#define USB_COMPOSITE_INTERFACES			6
#define USB_COMPOSITE_ENDPOINTS				7

#define USB_COMPOSITE_CONFIG_DESC_SIZE		207
#define USB_COMPOSITE_CONFIG_DESC			cUSB_Composite_ConfigDescriptor

#define USB_COMPOSITE_CLASSID				0xEF
#define USB_COMPOSITE_SUBCLASSID			0x02
#define USB_COMPOSITE_PROTOCOLID			0x01

#define USB_CDC_INTERFACE_BASE				0
#define USB_CDC_ENDPOINT_BASE				1

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

// Callbacks for USB_CTL.
// These should be referenced in USB_Class.h
void USB_Composite_Init(uint8_t config);
void USB_Composite_Deinit(void);
void USB_Composite_Setup(USB_SetupRequest_t * req);

/*
 * EXTERN DECLARATIONS
 */

extern const uint8_t cUSB_Composite_ConfigDescriptor[USB_COMPOSITE_CONFIG_DESC_SIZE];


#endif // USB_COMPOSITE_H
