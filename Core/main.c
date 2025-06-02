
#include "Core.h"

#include "GPIO.h"
#include "USB.h"
#include "USB_CDCX.h"
#include "Console.h"
#include "LED.h"
#include "UART.h"
#include "I2C.h"
#include "M24xx.h"

#include "SCPI.h"


#define DETECT_STRING_MAX		32

static struct {
	bool pwr_en;
	bool dtr;
	bool reset;
	bool wake;
	bool uart_modem_en;
	bool uart_aux_en;
} gIO;


bool CMD_RST(SCPI_t * scpi, SCPI_Arg_t * args)
{
	bzero(&gIO, sizeof(gIO));
	GPIO_Write(MODEM_PWR_EN, GPIO_PIN_RESET);
	GPIO_Write(MODEM_RESET, GPIO_PIN_RESET);
	GPIO_Write(MODEM_WAKE, GPIO_PIN_RESET);
	GPIO_Write(MODEM_DTR, GPIO_PIN_RESET);
	UART_Deinit(MODEM_UART);
	UART_Deinit(AUX_UART);
	return true;
}

bool CMD_IDN(SCPI_t * scpi, SCPI_Arg_t * args)
{
	SCPI_Reply_Printf(scpi, "TL-Embedded, Winglet-Carrier, 0, v1.1\r\n");
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

bool CMD_UARTX(SCPI_t * scpi, SCPI_Arg_t * args, UART_t * uart, bool * state)
{
	if (!args)
	{
		// Handle query
		SCPI_Reply_Bool(scpi, *state);
		return true;
	}

	bool enable = args[0].boolean;

	if (enable)
	{
		if (!args[1].present)
		{
			return false;
		}

		uint32_t baud = args[1].number;
		if (baud < 1200 || baud > 230400)
		{
			return false;
		}

		UART_Init(uart, baud, UART_Mode_Default);
		*state = true;
	}
	else
	{
		UART_Deinit(uart);
		*state = false;
	}
	return true;
}

bool CMD_UART_Modem(SCPI_t * scpi, SCPI_Arg_t * args)
{
	return CMD_UARTX(scpi, args, MODEM_UART, &gIO.uart_modem_en);
}

bool CMD_UART_Aux(SCPI_t * scpi, SCPI_Arg_t * args)
{
	return CMD_UARTX(scpi, args, AUX_UART, &gIO.uart_aux_en);
}

bool CMD_PROM_Detect(SCPI_t * scpi, SCPI_Arg_t * args)
{
	I2C_Init(DETECT_I2C, I2C_Mode_Fast);
	bool success = M24xx_Init();
	SCPI_Reply_Bool(scpi, success);
	I2C_Deinit(DETECT_I2C);
	return true;
}

bool CMD_PROM_Read(SCPI_t * scpi, SCPI_Arg_t * args)
{
	bool success = false;
	I2C_Init(DETECT_I2C, I2C_Mode_Fast);

	uint8_t size;
	if (M24xx_Init() && M24xx_Read(0, &size, sizeof(size)))
	{
		char detect_str[DETECT_STRING_MAX];
		if (size < DETECT_STRING_MAX && M24xx_Read(1, (uint8_t*)detect_str, size))
		{
			success = true;
			SCPI_Reply_Printf(scpi, "\"%.*s\"", size, detect_str);
		}
	}

	I2C_Deinit(DETECT_I2C);
	return success;
}

bool CMD_PROM_Write(SCPI_t * scpi, SCPI_Arg_t * args)
{
	const char * str = args[0].string;
	uint8_t len = strlen(str);
	if (len >= DETECT_STRING_MAX) { return false; }

	bool success = false;
	I2C_Init(DETECT_I2C, I2C_Mode_Fast);

	uint8_t bfr[DETECT_STRING_MAX + 1];
	bfr[0] = len;
	memcpy(bfr+1, str, len);

	success = M24xx_Init() && M24xx_Write(0, bfr, len+1);

	I2C_Deinit(DETECT_I2C);
	return success;
}

const SCPI_Node_t cNodes[] = {
	{ .pattern = "*RST!", .func = CMD_RST },
	{ .pattern = "*IDN?", .func = CMD_IDN },
	{ .pattern = "POWer b", .func = CMD_Power },
	{ .pattern = "IO:DTR b", .func = CMD_IO_DTR },
	{ .pattern = ":DCD?", .func = CMD_IO_DCD },
	{ .pattern = ":RESet b", .func = CMD_IO_Reset },
	{ .pattern = ":WAKE b", .func = CMD_IO_Wake },
	{ .pattern = "UART:MODem b,?n", .func = CMD_UART_Modem },
	{ .pattern = ":AUX b,?n", .func = CMD_UART_Aux },
	{ .pattern = "PROM?", .func = CMD_PROM_Detect },
	{ .pattern = ":READ?", .func = CMD_PROM_Read },
	{ .pattern = ":WRITE! s", .func = CMD_PROM_Write },
};

static SCPI_t scpi;


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

		LED_Write(LED_Color_Red);
		SCPI_Parse(&scpi, bfr, read);
		LED_Write(LED_Color_Green);

		read = USB_CDCX_Read(1, bfr, sizeof(bfr));
		if (gIO.uart_modem_en)
		{
			UART_Write(MODEM_UART, bfr, read);
			USB_CDCX_Write(1, bfr, UART_Read(MODEM_UART, bfr, sizeof(bfr)));
		}

		read = USB_CDCX_Read(2, bfr, sizeof(bfr));
		if (gIO.uart_aux_en)
		{
			UART_Write(AUX_UART, bfr, read);
			USB_CDCX_Write(2, bfr, UART_Read(AUX_UART, bfr, sizeof(bfr)));
		}

		CORE_Idle();
	}
}

