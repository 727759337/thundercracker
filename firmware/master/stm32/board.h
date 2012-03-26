/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef BOARD_H
#define BOARD_H

// available boards to choose from
#define BOARD_STM32F10C         1
#define BOARD_TC_MASTER_REV1    2
#define BOARD_TC_MASTER_REV2    3

// default board
#ifndef BOARD
#define BOARD   BOARD_TC_MASTER_REV1
#endif

#include "hardware.h"

#if BOARD == BOARD_TC_MASTER_REV2

// C L O C K
#define RCC_CFGR_PLLXTPRE   1

// U S B
#define USB_DM_GPIO         GPIOPin(&GPIOA, 11)
#define USB_DP_GPIO         GPIOPin(&GPIOA, 12)
#define USB_VBUS_GPIO       GPIOPin(&GPIOA, 9)

// R A D I O
#define RF_SPI              SPI3
#define RF_CE_GPIO          GPIOPin(&GPIOC, 7)
#define RF_IRQ_GPIO         GPIOPin(&GPIOC, 8)
#define RF_SPI_CSN_GPIO     GPIOPin(&GPIOC, 9)
#define RF_SPI_SCK_GPIO     GPIOPin(&GPIOC, 10)
#define RF_SPI_MISO_GPIO    GPIOPin(&GPIOC, 11)
#define RF_SPI_MOSI_GPIO    GPIOPin(&GPIOC, 12)

// F L A S H
#define FLASH_SPI           SPI1
#define FLASH_CS_GPIO       GPIOPin(&GPIOA, 15)
#define FLASH_WP_GPIO       GPIOPin(&GPIOB, 6)
#define FLASH_SCK_GPIO      GPIOPin(&GPIOB, 3)
#define FLASH_MISO_GPIO     GPIOPin(&GPIOB, 4)
#define FLASH_MOSI_GPIO     GPIOPin(&GPIOB, 5)
#define FLASH_REG_EN_GPIO   GPIOPin(&GPIOC, 4)

// N E I G H B O R
#define NBR_OUT1_GPIO       GPIOPin(&GPIOB, 8)
#define NBR_OUT2_GPIO       GPIOPin(&GPIOB, 9)

// U A R T
#define UART_DBG            USART3
#define UART_RX_GPIO        GPIOPin(&GPIOB, 11)
#define UART_TX_GPIO        GPIOPin(&GPIOB, 10)

// L E D
#define LED_GREEN_GPIO      GPIOPin(&GPIOB, 0)
#define LED_RED_GPIO        GPIOPin(&GPIOB, 1)

// P O W E R
#define VCC20_ENABLE_GPIO   GPIOPin(&GPIOC, 0)
#define VCC33_ENABLE_GPIO   GPIOPin(&GPIOC, 1)

// A U D I O
#define AUDIO_DAC_GPIO      GPIOPin(&GPIOA, 4)
#define AUDIO_DAC_CHAN      1

#define AUDIO_PWMA_GPIO     GPIOPin(&GPIOA, 7)
#define AUDIO_PWMB_GPIO     GPIOPin(&GPIOA, 8)
#define AUDIO_PWM_CHAN      1

// M I S C
#define BTN_HOME_GPIO       GPIOPin(&GPIOD, 2)
#define BTN_HOME_EXTI_VEC   EXTI2

#elif BOARD == BOARD_TC_MASTER_REV1

// C L O C K
#define RCC_CFGR_PLLXTPRE   0

// U S B
#define USB_DM_GPIO         GPIOPin(&GPIOA, 11)
#define USB_DP_GPIO         GPIOPin(&GPIOA, 12)
#define USB_VBUS_GPIO       GPIOPin(&GPIOA, 9)

// R A D I O
#define RF_SPI              SPI3
#define RF_CE_GPIO          GPIOPin(&GPIOC, 7)
#define RF_IRQ_GPIO         GPIOPin(&GPIOC, 8)
#define RF_SPI_CSN_GPIO     GPIOPin(&GPIOC, 9)
#define RF_SPI_SCK_GPIO     GPIOPin(&GPIOC, 10)
#define RF_SPI_MISO_GPIO    GPIOPin(&GPIOC, 11)
#define RF_SPI_MOSI_GPIO    GPIOPin(&GPIOC, 12)

// F L A S H
#define FLASH_SPI           SPI1
#define FLASH_CS_GPIO       GPIOPin(&GPIOA, 15)
#define FLASH_WP_GPIO       GPIOPin(&GPIOB, 2)
#define FLASH_SCK_GPIO      GPIOPin(&GPIOB, 3)
#define FLASH_MISO_GPIO     GPIOPin(&GPIOB, 4)
#define FLASH_MOSI_GPIO     GPIOPin(&GPIOB, 5)

// N E I G H B O R
#define NBR_OUT1_GPIO       GPIOPin(&GPIOB, 8)
#define NBR_OUT2_GPIO       GPIOPin(&GPIOB, 9)

// U A R T
#define UART_DBG            USART3
#define UART_RX_GPIO        GPIOPin(&GPIOB, 11)
#define UART_TX_GPIO        GPIOPin(&GPIOB, 10)

// L E D
#define LED_GREEN_GPIO      GPIOPin(&GPIOB, 0)
#define LED_RED_GPIO        GPIOPin(&GPIOB, 1)

// P O W E R
#define VCC20_ENABLE_GPIO   GPIOPin(&GPIOC, 13)
#define VCC33_ENABLE_GPIO   GPIOPin(&GPIOC, 14)

// A U D I O
#define AUDIO_DAC_GPIO      GPIOPin(&GPIOA, 4)
#define AUDIO_DAC_CHAN      1

#define AUDIO_PWMA_GPIO     GPIOPin(&GPIOA, 7)
#define AUDIO_PWMB_GPIO     GPIOPin(&GPIOA, 8)
#define AUDIO_PWM_CHAN      1

// M I S C
#define BTN_HOME_GPIO       GPIOPin(&GPIOC, 0)
#define BTN_HOME_EXTI_VEC   EXTI0

#elif BOARD == BOARD_STM32F10C

// R A D I O
#define RF_CE_GPIO          GPIOPin(&GPIOB, 10)
#define RF_IRQ_GPIO         GPIOPin(&GPIOB, 11)
#define RF_SPI              SPI2
#define RF_SPI_CSN_GPIO     GPIOPin(&GPIOB, 12)
#define RF_SPI_SCK_GPIO     GPIOPin(&GPIOB, 13)
#define RF_SPI_MISO_GPIO    GPIOPin(&GPIOB, 14)
#define RF_SPI_MOSI_GPIO    GPIOPin(&GPIOB, 15)

#else
#error BOARD not configured
#endif

#endif // BOARD_H
