
#include "USB_CDCX.h"

#include "usb/USB_EP.h"
#include "usb/USB_CTL.h"
#include "Core.h"

/*
 * PRIVATE DEFINITIONS
 */

#define USB_CDC_COUNT								3

#ifdef USB_CDC_BFR_SIZE
#define CDC_BFR_SIZE 	USB_CDC_BFR_SIZE
#else
#define CDC_BFR_SIZE 512
#endif

#define CDC_BFR_WRAP(v) ((v) & (CDC_BFR_SIZE - 1))

#if (CDC_BFR_WRAP(CDC_BFR_SIZE) != 0)
#error "USB_CDCX_BFR_SIZE must be a power of two"
#endif


// Having adjustable endpoint and interface offsets is required for composite device support
#ifndef USB_CDC_ENDPOINT_BASE
#define USB_CDC_ENDPOINT_BASE						1
#endif

#ifndef USB_CDC_INTERFACE_BASE
#define USB_CDC_INTERFACE_BASE						0
#endif

#define IN_EP(n)									((USB_CDC_ENDPOINT_BASE + n) | 0x80)
#define OUT_EP(n)									(USB_CDC_ENDPOINT_BASE + n)

#define CDC_IN_EP(port)                             IN_EP((port*2) + 0)
#define CDC_OUT_EP(port)                            OUT_EP((port*2) + 0)
#define CDC_CMD_EP(port)                            IN_EP((port*2) + 1)

#define CDC_BINTERVAL                          		0x10
#define CDC_PACKET_SIZE								USB_PACKET_SIZE
#define CDC_CMD_PACKET_SIZE                         8

#define CDC_SEND_ENCAPSULATED_COMMAND               0x00
#define CDC_GET_ENCAPSULATED_RESPONSE               0x01
#define CDC_SET_COMM_FEATURE                        0x02
#define CDC_GET_COMM_FEATURE                        0x03
#define CDC_CLEAR_COMM_FEATURE                      0x04
#define CDC_SET_LINE_CODING                         0x20
#define CDC_GET_LINE_CODING                         0x21
#define CDC_SET_CONTROL_LINE_STATE                  0x22
#define CDC_SEND_BREAK                              0x23


/*
 * PRIVATE TYPES
 */

typedef struct {
	uint8_t buffer[CDC_BFR_SIZE];
	uint32_t head;
	uint32_t tail;
} CDCBuffer_t;

typedef struct {
	volatile bool txBusy;
	uint8_t lineCoding[7];
	CDCBuffer_t rx;
	uint8_t rx_packet[CDC_PACKET_SIZE];
} CDC_t;

typedef struct {
	uint8_t opcode;
	uint8_t size;
	uint8_t port;
	// Not sure why this needs to be aligned?
	uint32_t data[CDC_CMD_PACKET_SIZE/4];
} CDC_CMD_t;

/*
 * PRIVATE PROTOTYPES
 */

static void USB_CDC_Control(uint8_t port, uint8_t cmd, uint8_t* data, uint16_t length);
static void USB_CDC_CtlRxReady(void);

#if (USB_CDC_COUNT > 0)
static void USB_CDC_Receive0(uint32_t count);
static void USB_CDC_TransmitDone0(uint32_t count);
#endif
#if (USB_CDC_COUNT > 1)
static void USB_CDC_Receive1(uint32_t count);
static void USB_CDC_TransmitDone1(uint32_t count);
#endif
#if (USB_CDC_COUNT > 2)
static void USB_CDC_Receive2(uint32_t count);
static void USB_CDC_TransmitDone2(uint32_t count);
#endif


/*
 * PRIVATE VARIABLES
 */

static CDC_t gCDC[USB_CDC_COUNT];
static CDC_CMD_t gCMD;

/*
 * PUBLIC FUNCTIONS
 */

void USB_CDCX_Init(uint8_t config)
{
	// 115200bps, 1stop, no parity, 8bit
	const uint8_t lineCoding[] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };

	for (uint8_t port = 0; port < USB_CDC_COUNT; port++)
	{
		CDC_t * cdc = gCDC + port;
		cdc->rx.head = cdc->rx.tail = 0;
		cdc->txBusy = false;
		memcpy(cdc->lineCoding, lineCoding, sizeof(cdc->lineCoding));
	}

#if (USB_CDC_COUNT > 0)
	// Data endpoints 0
	USB_EP_Open(CDC_IN_EP(0), USB_EP_TYPE_BULK, CDC_PACKET_SIZE, USB_CDC_TransmitDone0);
	USB_EP_Open(CDC_OUT_EP(0), USB_EP_TYPE_BULK, CDC_PACKET_SIZE, USB_CDC_Receive0);
	USB_EP_Open(CDC_CMD_EP(0), USB_EP_TYPE_BULK, CDC_CMD_PACKET_SIZE, USB_CDC_TransmitDone0);
	USB_EP_Read(CDC_OUT_EP(0), gCDC[0].rx_packet, CDC_PACKET_SIZE);
#endif
#if (USB_CDC_COUNT > 1)
	// Data endpoints 1
	USB_EP_Open(CDC_IN_EP(1), USB_EP_TYPE_BULK, CDC_PACKET_SIZE, USB_CDC_TransmitDone1);
	USB_EP_Open(CDC_OUT_EP(1), USB_EP_TYPE_BULK, CDC_PACKET_SIZE, USB_CDC_Receive1);
	USB_EP_Open(CDC_CMD_EP(1), USB_EP_TYPE_BULK, CDC_CMD_PACKET_SIZE, USB_CDC_TransmitDone1);
	USB_EP_Read(CDC_OUT_EP(1), gCDC[1].rx_packet, CDC_PACKET_SIZE);
#endif
#if (USB_CDC_COUNT > 2)
	// Data endpoints 2
	USB_EP_Open(CDC_IN_EP(2), USB_EP_TYPE_BULK, CDC_PACKET_SIZE, USB_CDC_TransmitDone2);
	USB_EP_Open(CDC_OUT_EP(2), USB_EP_TYPE_BULK, CDC_PACKET_SIZE, USB_CDC_Receive2);
	USB_EP_Open(CDC_CMD_EP(2), USB_EP_TYPE_BULK, CDC_CMD_PACKET_SIZE, USB_CDC_TransmitDone2);
	USB_EP_Read(CDC_OUT_EP(2), gCDC[2].rx_packet, CDC_PACKET_SIZE);
#endif
}

void USB_CDCX_Deinit(void)
{
	for (uint8_t port = 0; port < USB_CDC_COUNT; port++)
	{
		CDC_t * cdc = gCDC + port;
		USB_EP_Close(CDC_IN_EP(port));
		USB_EP_Close(CDC_OUT_EP(port));
		USB_EP_Close(CDC_CMD_EP(port));
		cdc->rx.head = cdc->rx.tail = 0;
	}
}

void USB_CDCX_Write(uint8_t port, const uint8_t * data, uint32_t count)
{
	// This will block if the transmitter is not free, or multiple packets are sent out.
	uint32_t tide = CORE_GetTick();
	CDC_t * cdc = gCDC + port;
	while (count)
	{
		if (cdc->txBusy)
		{
			// Wait for transmit to be free. Abort if it does not come free.
			if (CORE_GetTick() - tide > 10)
			{
				break;
			}
			CORE_Idle();
		}
		else
		{
			// Transmit a packet
			// We send packets of length 63. This gets around an issue where windows can drop full sized serial packets.
			uint32_t packet_size = count > (CDC_PACKET_SIZE - 1) ? (CDC_PACKET_SIZE - 1) : count;
			cdc->txBusy = true;
			USB_EP_Write(CDC_IN_EP(port), data, packet_size);
			count -= packet_size;
			data += packet_size;
		}
	}
}

void USB_CDCX_WriteStr(uint8_t port, const char * str)
{
	USB_CDCX_Write(port, (const uint8_t *)str, strlen(str));
}

uint32_t USB_CDCX_ReadReady(uint8_t port)
{
	CDC_t * cdc = gCDC + port;
	// Assume these reads are atomic
	return CDC_BFR_WRAP(cdc->rx.head - cdc->rx.tail);
}

uint32_t USB_CDCX_Read(uint8_t port, uint8_t * data, uint32_t count)
{
	CDC_t * cdc = gCDC + port;
	uint32_t ready = USB_CDCX_ReadReady(port);

	if (count > ready)
	{
		count = ready;
	}
	if (count > 0)
	{
		uint32_t tail = cdc->rx.tail;
		uint32_t newtail = CDC_BFR_WRAP( tail + count );
		if (newtail > tail)
		{
			// We can read continuously from the buffer
			memcpy(data, cdc->rx.buffer + tail, count);
		}
		else
		{
			// We read to end of buffer, then read from the start
			uint32_t chunk = CDC_BFR_SIZE - tail;
			memcpy(data, cdc->rx.buffer + tail, chunk);
			memcpy(data + chunk, cdc->rx.buffer, count - chunk);
		}
		cdc->rx.tail = newtail;
	}
	return count;
}

void USB_CDCX_Setup(uint8_t port, USB_SetupRequest_t * req)
{
	if (req->wLength)
	{
		if (req->bmRequest & 0x80U)
		{
			USB_CDC_Control(port, req->bRequest, (uint8_t *)gCMD.data, req->wLength);
			USB_CTL_Send((uint8_t *)gCMD.data, req->wLength);
		}
		else
		{
			gCMD.port = port;
			gCMD.opcode = req->bRequest;
			gCMD.size = req->wLength;
			USB_CTL_Receive((uint8_t *)gCMD.data, req->wLength, USB_CDC_CtlRxReady);
		}
	}
	else
	{
		USB_CDC_Control(port, req->bRequest, (uint8_t *)req, 0U);
	}
}

/*
 * PRIVATE FUNCTIONS
 */

static void USB_CDC_CtlRxReady(void)
{
	if (gCMD.opcode != 0xFF)
	{
		USB_CDC_Control(gCMD.port, gCMD.opcode, (uint8_t *)gCMD.data,  gCMD.size);
		gCMD.opcode = 0xFF;
	}
}

static void USB_CDC_Control(uint8_t port, uint8_t cmd, uint8_t* data, uint16_t length)
{
	CDC_t * cdc = gCDC + port;
	switch(cmd)
	{
	case CDC_SET_LINE_CODING:
		memcpy(cdc->lineCoding, data, sizeof(cdc->lineCoding));
		break;
	case CDC_GET_LINE_CODING:
		memcpy(data, cdc->lineCoding, sizeof(cdc->lineCoding));
		break;
	case CDC_SEND_ENCAPSULATED_COMMAND:
	case CDC_GET_ENCAPSULATED_RESPONSE:
	case CDC_SET_COMM_FEATURE:
	case CDC_GET_COMM_FEATURE:
	case CDC_CLEAR_COMM_FEATURE:
	case CDC_SET_CONTROL_LINE_STATE:
	case CDC_SEND_BREAK:
	default:
		break;
	}
}

static void USB_CDC_Receive(uint8_t port, uint32_t count)
{
	CDC_t * cdc = gCDC + port;
	// Minus 1 because head == tail represents the empty condition.
	uint32_t space = CDC_BFR_WRAP(cdc->rx.tail - cdc->rx.head - 1);

	if (count > space)
	{
		// Discard any data that we cannot insert into the buffer.
		count = space;
	}
	if (count > 0)
	{
		uint32_t head = cdc->rx.head;
		uint32_t newhead = CDC_BFR_WRAP( head + count );
		if (newhead > head)
		{
			// We can write continuously into the buffer
			memcpy(cdc->rx.buffer + head, cdc->rx_packet, count);
		}
		else
		{
			// We write to end of buffer, then write from the start
			uint32_t chunk = CDC_BFR_SIZE - head;
			memcpy(cdc->rx.buffer + head, cdc->rx_packet, chunk);
			memcpy(cdc->rx.buffer, cdc->rx_packet + chunk, count - chunk);
		}
		cdc->rx.head = newhead;
	}

	USB_EP_Read(CDC_OUT_EP(port), cdc->rx_packet, CDC_PACKET_SIZE);
}

static void USB_CDC_TransmitDone(uint8_t port, uint32_t count)
{
	CDC_t * cdc = gCDC + port;
	if (count > 0 && (count % CDC_PACKET_SIZE) == 0)
	{
		// Write a ZLP to complete the tx.
		USB_EP_WriteZLP(CDC_IN_EP(port));
	}
	else
	{
		cdc->txBusy = false;
	}
}

#if (USB_CDC_COUNT > 0)
static void USB_CDC_Receive0(uint32_t count) 		{ USB_CDC_Receive(0, count); }
static void USB_CDC_TransmitDone0(uint32_t count) 	{ USB_CDC_TransmitDone(0, count); }
#endif
#if (USB_CDC_COUNT > 1)
static void USB_CDC_Receive1(uint32_t count) 		{ USB_CDC_Receive(1, count); }
static void USB_CDC_TransmitDone1(uint32_t count)	{ USB_CDC_TransmitDone(1, count); }
#endif
#if (USB_CDC_COUNT > 2)
static void USB_CDC_Receive2(uint32_t count) 		{ USB_CDC_Receive(2, count); }
static void USB_CDC_TransmitDone2(uint32_t count)	{ USB_CDC_TransmitDone(2, count); }
#endif

