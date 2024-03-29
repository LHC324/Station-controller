/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "shell_port.h"
#include "mdrtuslave.h"
#include "dwin.h"
#include "Flash.h"
#include "tool.h"
#include "lte.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
Save_Param Save_InitPara = {
    // .flag = SAVE_SURE_CODE,
    .crc16 = 0x9D61, // 0xE31A
    .Ptank_max = 4.0F,
    .Ptank_min = 0.0F,
    .Pvap_outlet_max = 4.0F,
    .Pvap_outlet_min = 0.0F,
    .Pgas_soutlet_max = 4.0F,
    .Pgas_soutlet_min = 0.0F,
    .Ltank_max = 50.0F,
    .Ltank_min = 0.0F,
    .Ptoler_upper = 1.2F,
    .Ptoler_lower = 0.66F,
    .Ltoler_upper = 2.0F,
    .Ltoler_lower = 0.5F,
    .PSspf_start = 2.0F,
    .PSspf_stop = 1.8F,
    // .PSvap_outlet_limit = 1.1F,
    .PSvap_outlet_Start = 1.2F,
    .Pback_difference = 0.2F,
    .Ptank_difference = 2.0F,
    // .PPvap_outlet_limit = 2.2F,
    .PPvap_outlet_Start = 2.2F,
    .PPspf_start = 2.3F,
    .PPspf_stop = 2.1F,
    .Ptank_limit = 1.2F,
    .Ltank_limit = 2.0F,
    // .Pvap_outlet_stop = 1.8F,
    .PPvap_outlet_stop = 1.8F,
    .PSvap_outlet_stop = 1.1F,
    .User_Name = 0x07E6,
    .User_Code = 0x0522,
    .System_Version = SYSTEM_VERSION(),
    .System_Flag = 0xFFFFFFFF,
    .Adc = {
        .Cp = 0.009052399F,
        .Cq = 0.588793299F,
    },
    .Dac = {.Dac = {
                {202.3956F, 4.585381F},
                {203.08832F, -1.29154F},
                {190.0288F, -19.1239F},
                {189.398281F, -16.8367F},
            }},
    .Error_Code = 0x0000,
    // .Update = 0xFFFFFFFF,
};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
extern void Report_Backparam(pDwinHandle pd, Save_Param *sp);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */
  Save_HandleTypeDef *ps = &Save_Flash;
  // Save_Param *sp = &Save_Flash.Param;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  MX_DAC_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)Adc_buffer, ADC_DMA_SIZE);
  MX_ShellInit(Shell_Object);
  MX_ModbusInit();
  MX_DwinInit();
  FLASH_Init();
  FLASH_Read(PARAM_SAVE_ADDRESS, &ps->Param, sizeof(Save_Param));
  /*Must be 4 byte aligned!!!*/
  uint16_t crc16 = Get_Crc16((uint8_t *)&ps->Param, sizeof(Save_Param) - sizeof(ps->Param.crc16), 0xFFFF);
#if defined(USING_DEBUG)
  // shellPrint(Shell_Object, "ps->Param.flag = 0x%x\r\n", ps->Param.flag);
  shellPrint(Shell_Object, "ps->Param.crc16 = 0x%x, crc16 = 0x%x.\r\n", ps->Param.crc16, crc16);
#endif
  // if (Save_Flash.Param.flag != SAVE_SURE_CODE)
  if (crc16 != ps->Param.crc16)
  {
    Save_InitPara.crc16 = Get_Crc16((uint8_t *)&Save_InitPara, sizeof(Save_Param) - sizeof(Save_InitPara.crc16), 0xFFFF);
    memcpy(&ps->Param, &Save_InitPara, sizeof(Save_Param));
    FLASH_Write(PARAM_SAVE_ADDRESS, (uint16_t *)&Save_InitPara, sizeof(Save_Param));
#if defined(USING_DEBUG)
    shellPrint(Shell_Object, "Save_InitPara.crc16 = 0x%x,First initialization of flash parameters!\r\n", Save_InitPara.crc16);
#endif
    /*Initial 4G module*/
    MX_AtInit();
    if (At_Object)
    {
      At_Object->AT_SetDefault(At_Object);
      At_Object->Free_AtObject(&At_Object);
    }
  }
  if (Slave1_Object)
  {
    /*Idle interrupt*/
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    /*DMA buffer must point to an entity address!!!*/
    HAL_UART_Receive_DMA(&huart3, mdRTU_Recive_Buf(Slave1_Object), MODBUS_PDU_SIZE_MAX);
    /*Parameters are written to the mdbus hold register*/
    // Param_WriteBack(ps);
  }
#if !defined(USING_SHELL)
  if (Slave2_Object)
  {
    /*Idle interrupt*/
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    /*DMA buffer must point to an entity address!!!*/
    HAL_UART_Receive_DMA(&huart2, mdRTU_Recive_Buf(Slave2_Object), MODBUS_PDU_SIZE_MAX);
  }
#endif
  if (Dwin_Object)
  {
    /*Idle interrupt*/
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    /*DMA buffer must point to an entity address!!!*/
    HAL_UART_Receive_DMA(&huart1, Dwin_Recive_Buf(Dwin_Object), Dwin_Rx_Size(Dwin_Object));
  }
  /*Turn off the global interrupt in bootloader, and turn it on here*/
  __set_FAULTMASK(0);
  /*Solve the problem that the background data cannot be received due to the unstable power supply when the Devon screen is turned on*/
  HAL_Delay(2000);
  Report_Backparam(Dwin_Object, &ps->Param);
  /*Turn on the DAC*/
  HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac, DAC_CHANNEL_2);
  /* USER CODE END 2 */

  /* Call init function for freertos objects (in freertos.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
