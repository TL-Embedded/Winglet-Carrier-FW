
#include "Core.h"

#include "GPIO.h"
#include "USB.h"
#include "USB_CDCX.h"
#include "Console.h"
#include "LED.h"

#include "SCPI.h"


static struct {
	bool pwr_en;
	bool dtr;
	bool reset;
	bool wake;
} gIO;


bool CMD_RST(SCPI_t * scpi, SCPI_Arg_t * args)
{
	bzero(&gIO, sizeof(gIO));
	GPIO_Write(MODEM_PWR_EN, GPIO_PIN_RESET);
	GPIO_Write(MODEM_RESET, GPIO_PIN_RESET);
	GPIO_Write(MODEM_WAKE, GPIO_PIN_RESET);
	GPIO_Write(MODEM_DTR, GPIO_PIN_RESET);
	return true;
}

bool CMD_IDN(SCPI_t * scpi, SCPI_Arg_t * args)
{
	SCPI_Reply_Printf(scpi, "TL-Embedded, Winglet-Carrier, 0, v0.0\r\n");
	return true;
}

bool CMD_PinState(SCPI_t * scpi, SCPI_Arg_t * args, GPIO_Pin_t pin, bool * state)
{
	if (args)
	{
		*state = args[0].boolean;
		GPIO_Write(pin, *state);
	}
	else
	{
		SCPI_Reply_Bool(scpi, *state);
	}
	return true;
}

bool CMD_Power(SCPI_t * scpi, SCPI_Arg_t * args)
{
	return CMD_PinState(scpi, args, MODEM_PWR_EN, &gIO.pwr_en);
}

bool CMD_IO_DTR(SCPI_t * scpi, SCPI_Arg_t * args)
{
	return CMD_PinState(scpi, args, MODEM_DTR, &gIO.dtr);
}

bool CMD_IO_DCD(SCPI_t * scpi, SCPI_Arg_t * args)
{
	SCPI_Reply_Bool(scpi, GPIO_Read(MODEM_DCD));
	return true;
}

bool CMD_IO_Reset(SCPI_t * scpi, SCPI_Arg_t * args)
{
	return CMD_PinState(scpi, args, MODEM_RESET, &gIO.reset);
}

bool CMD_IO_Wake(SCPI_t * scpi, SCPI_Arg_t * args)
{
	return CMD_PinState(scpi, args, MODEM_WAKE, &gIO.wake);
}

const SCPI_Node_t cNodes[] = {
	{ .pattern = "*RST!", .func = CMD_RST },
	{ .pattern = "*IDN?", .func = CMD_IDN },
	{ .pattern = "POWer b", .func = CMD_Power },
	{ .pattern = "IO:DTR b", .func = CMD_IO_DTR },
	{ .pattern = ":DCD?", .func = CMD_IO_DCD },
	{ .pattern = ":RESet b", .func = CMD_IO_Reset },
	{ .pattern = ":WAKE b", .func = CMD_IO_Wake },
	{ .pattern = "UART:EN b", .func = NULL },
	{ .pattern = ":SEND! s", .func = NULL },
	{ .pattern = ":RECEive!", .func = NULL },
};

SCPI_t scpi;


int main(void)
{
	CORE_Init();
	USB_Init();
	Console_Init();
	LED_Init();

	GPIO_EnableOutput(MODEM_PWR_EN, GPIO_PIN_RESET);
	GPIO_EnableOutput(MODEM_RESET, GPIO_PIN_RESET);
	GPIO_EnableOutput(MODEM_WAKE, GPIO_PIN_RESET);
	GPIO_EnableOutput(MODEM_DTR, GPIO_PIN_RESET);
	GPIO_EnableInput(MODEM_DCD, GPIO_Pull_None);

	SCPI_Init(&scpi, cNodes, LENGTH(cNodes), Console_Write);

	while(1)
	{
		uint8_t bfr[64];
		uint32_t read = Console_Read(bfr, sizeof(bfr));
		SCPI_Parse(&scpi, bfr, read);

		read = USB_CDCX_Read(1, bfr, sizeof(bfr));
		USB_CDCX_Write(2, bfr, read);

		read = USB_CDCX_Read(2, bfr, sizeof(bfr));
		USB_CDCX_Write(1, bfr, read);

		CORE_Idle();
	}
}

