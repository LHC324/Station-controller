/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
// #define USING_DEBUG 1
#define USING_SHELL
// #define USING_DEBUG_APPLICATION
#define USING_RTOS
#define USING_DMA
// #define USING_B3
// #define USING_LYNCH /*For Lynch project*/
/*Custom memory management*/
#define CUSTOM_MALLOC pvPortMalloc
#define CUSTOM_FREE vPortFree
#define CURRENT_HARDWARE_VERSION 101
#define CURRENT_SOFT_VERSION 143
#define SYSTEM_VERSION() ((uint32_t)((CURRENT_HARDWARE_VERSION << 16U) | CURRENT_SOFT_VERSION))
#define PARAM_MD_ADDR 0x0008
#define MDUSER_NAME_ADDR 0x0038
#define SOFT_VERSION_ADDR 0x003A
#define SAVE_SIZE 26U
#define SAVE_SURE_CODE 0x5AA58734
#define UPDATE_FLAG_FLASH_PAGE 254
#define UPDATE_SAVE_ADDRESS (FLASH_BASE + UPDATE_FLAG_FLASH_PAGE * FLASH_PAGE_SIZE)
#define USER_SAVE_FLASH_PAGE 255U
#define PARAM_SAVE_ADDRESS (FLASH_BASE + USER_SAVE_FLASH_PAGE * FLASH_PAGE_SIZE)
#define UPDATE_CMD 0x1234
#define UPDATE_APP1 0x5AA5
#define UPDATE_APP2 0xA55A
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define REPORT_TIMES 1100U
#define MD_TIMES 1000U
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define ESC_CODE 0x1B
#define BACKSPACE_CODE 0x08
#define ENTER_CODE 0x0D
#define SPOT_CODE 0x2E
#define COMMA_CODE 0x2C

  typedef struct
  {
    float Cp, Cq;
  } Adc_Param_HandleTypeDef;
  typedef struct
  {
    float Dac[4U][2U];
  } Dac_Param_HandleTypeDef;

  typedef struct
  {
    float Ptank;
    float Pvap_outlet;
    float Pgas_soutlet;
    float Ltank;
  } Save_User;

  typedef struct
  {
    float Ptank_max;
    float Ptank_min;
    float Pvap_outlet_max;
    float Pvap_outlet_min;
    float Pgas_soutlet_max;
    float Pgas_soutlet_min;
    float Ltank_max;
    float Ltank_min;
    float Ptoler_upper;
    float Ptoler_lower;
    float Ltoler_upper;
    float Ltoler_lower;
    float PSspf_start;
    float PSspf_stop;
    // float PSvap_outlet_limit;
    float PSvap_outlet_Start;
    float Pback_difference;
    float Ptank_difference;
    // float PPvap_outlet_limit;
    float PPvap_outlet_Start;
    float PPspf_start;
    float PPspf_stop;
    float Ptank_limit;
    float Ltank_limit;
    // float Pvap_outlet_stop;
    float PPvap_outlet_stop;
    float PSvap_outlet_stop;
    // uint32_t flag;
    // uint32_t crc16;
    uint16_t User_Name;
    uint16_t User_Code;
    // uint32_t Update;
    uint32_t Error_Code;
    uint32_t System_Version;
    uint32_t System_Flag;
    Adc_Param_HandleTypeDef Adc;
    Dac_Param_HandleTypeDef Dac;
    uint32_t crc16;
  } Save_Param;
  typedef struct
  {
    Save_User User;
    Save_Param Param;
  } Save_HandleTypeDef __attribute__((aligned(4)));

  typedef struct
  {
    uint16_t counts;
    bool flag;
  } Soft_Timer_HandleTypeDef;

  extern Save_HandleTypeDef Save_Flash;
  /* USER CODE END EM */

  /* Exported functions prototypes ---------------------------------------------*/
  void Error_Handler(void);

  /* USER CODE BEGIN EFP */
  extern void Param_WriteBack(Save_HandleTypeDef *ps);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RESET_4G_Pin GPIO_PIN_12
#define RESET_4G_GPIO_Port GPIOE
#define LINK_4G_Pin GPIO_PIN_13
#define LINK_4G_GPIO_Port GPIOE
#define RELOAD_4G_Pin GPIO_PIN_14
#define RELOAD_4G_GPIO_Port GPIOE
#define NET_4G_Pin GPIO_PIN_15
#define NET_4G_GPIO_Port GPIOE
#define TX_4G_Pin GPIO_PIN_10
#define TX_4G_GPIO_Port GPIOB
#define RX_4G_Pin GPIO_PIN_11
#define RX_4G_GPIO_Port GPIOB
#define Q_0_Pin GPIO_PIN_12
#define Q_0_GPIO_Port GPIOB
#define Q_1_Pin GPIO_PIN_13
#define Q_1_GPIO_Port GPIOB
#define Q_2_Pin GPIO_PIN_14
#define Q_2_GPIO_Port GPIOB
#define Q_3_Pin GPIO_PIN_15
#define Q_3_GPIO_Port GPIOB
#define Q_4_Pin GPIO_PIN_8
#define Q_4_GPIO_Port GPIOD
#define Q_5_Pin GPIO_PIN_9
#define Q_5_GPIO_Port GPIOD
#define Q_6_Pin GPIO_PIN_10
#define Q_6_GPIO_Port GPIOD
#define DI_0_Pin GPIO_PIN_11
#define DI_0_GPIO_Port GPIOD
#define DI_1_Pin GPIO_PIN_12
#define DI_1_GPIO_Port GPIOD
#define DI_2_Pin GPIO_PIN_13
#define DI_2_GPIO_Port GPIOD
#define DI_3_Pin GPIO_PIN_14
#define DI_3_GPIO_Port GPIOD
#define DI_4_Pin GPIO_PIN_15
#define DI_4_GPIO_Port GPIOD
#define DI_5_Pin GPIO_PIN_6
#define DI_5_GPIO_Port GPIOC
#define DI_6_Pin GPIO_PIN_7
#define DI_6_GPIO_Port GPIOC
#define DI_7_Pin GPIO_PIN_8
#define DI_7_GPIO_Port GPIOC
#define TX_DWEN_Pin GPIO_PIN_9
#define TX_DWEN_GPIO_Port GPIOA
#define RX_DWEN_Pin GPIO_PIN_10
#define RX_DWEN_GPIO_Port GPIOA
#define SPI1_CS_Pin GPIO_PIN_15
#define SPI1_CS_GPIO_Port GPIOA
#define WDI_Pin GPIO_PIN_2
#define WDI_GPIO_Port GPIOD
#define RS4850TX_Pin GPIO_PIN_5
#define RS4850TX_GPIO_Port GPIOD
#define RS4850RX_Pin GPIO_PIN_6
#define RS4850RX_GPIO_Port GPIOD
/* USER CODE BEGIN Private defines */
#define GET_PARAM_SITE(TYPE, MEMBER, SIZE) (offsetof(TYPE, MEMBER) / sizeof(SIZE))
#define PARAM_END_ADDR (PARAM_MD_ADDR + GET_PARAM_SITE(Save_Param, User_Name, uint16_t))
  /* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
