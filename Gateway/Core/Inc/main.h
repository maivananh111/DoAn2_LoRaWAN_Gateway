/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LORA1_IO0_Pin GPIO_PIN_3
#define LORA1_IO0_GPIO_Port GPIOE
#define LORA1_IO0_EXTI_IRQn EXTI3_IRQn
#define LORA1_IO1_Pin GPIO_PIN_4
#define LORA1_IO1_GPIO_Port GPIOE
#define LORA1_IO1_EXTI_IRQn EXTI4_IRQn
#define LORA1_RST_Pin GPIO_PIN_5
#define LORA1_RST_GPIO_Port GPIOE
#define LORA1_CS_Pin GPIO_PIN_6
#define LORA1_CS_GPIO_Port GPIOE
#define LED_WIFI_Pin GPIO_PIN_13
#define LED_WIFI_GPIO_Port GPIOC
#define ETH_LINK_INT_Pin GPIO_PIN_0
#define ETH_LINK_INT_GPIO_Port GPIOC
#define ETH_LINK_INT_EXTI_IRQn EXTI0_IRQn
#define UART8_RX_Pin GPIO_PIN_0
#define UART8_RX_GPIO_Port GPIOE
#define UART8_TX_Pin GPIO_PIN_1
#define UART8_TX_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
