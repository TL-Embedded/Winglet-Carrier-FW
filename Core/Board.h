#ifndef BOARD_H
#define BOARD_H

//#define STM32L0
#define STM32F0
//#define STM32G0

// Core config
//#define CORE_USE_TICK_IRQ

// CLK config
#define CLK_USE_HSE
#define CLK_HSE_FREQ		16000000
//#define CLK_USE_LSE
//#define CLK_LSE_BYPASS
//#define CLK_LSE_FREQ		32768
//#define CLK_SYSCLK_FREQ	32000000

// RTC config
//#define RTC_USE_IRQS

// US config
//#define US_TIM			TIM_22
//#define US_RES			1

// ADC config
//#define ADC_VREF	        3300

// GPIO config
//#define GPIO_USE_IRQS
//#define GPIO_IRQ0_ENABLE

// TIM config
//#define TIM_USE_IRQS
//#define TIM2_ENABLE

// UART config
#define UART1_PINS			(PA9 | PA10)
#define UART1_AF		    GPIO_AF1_USART1

#define UART2_PINS			(PA2 | PA3)
#define UART2_AF		    GPIO_AF1_USART2

#define UART3_PINS			(PB10 | PB11)
#define UART3_AF		    GPIO_AF4_USART3

#define UART_BFR_SIZE     	128

// SPI config
//#define SPI1_PINS		    (PB3 | PB4 | PB5)
//#define SPI1_AF			GPIO_AF0_SPI1

// I2C config
#define I2C1_PINS			(PB6 | PB7)
#define I2C1_AF				GPIO_AF1_I2C1
//#define I2C_USE_FASTMODEPLUS
//#define I2C_USE_LONG_TRANSFER

// CAN config
//#define CAN_PINS			(PB8 | PB9)
//#define CAN_AF			GPIO_AF4_CAN
//#define CAN_DUAL_FIFO

// USB config
#define USB_ENABLE
#define USB_CLASS_CDC
#define USB_CDC_BFR_SIZE		256

// TSC config
//#define TSC_ENABLE
//#define TSC_AF					GPIO_AF3_TSC

#define MODEM_UART			UART_2
#define MODEM_WAKE			PA4
#define MODEM_RESET			PA5
#define MODEM_DTR			PA6
#define MODEM_DCD			PA7
#define MODEM_PWR_EN		PB0

#define AUX_UART			UART_3

#define LED_R_PIN			PB5
#define LED_G_PIN			PB4
#define LED_B_PIN			PB3
#define LED_PORT_COMMON

#define DETECT_I2C			I2C_1
#define M24XX_I2C			DETECT_I2C
#define M24XX_SERIES		1

//#define CONSOLE_UART		UART_1
#define CONSOLE_USB
#define CONSOLE_TX_BFR		64
#define CONSOLE_RX_BFR		64


#endif /* BOARD_H */
