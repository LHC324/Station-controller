/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "shell_port.h"
#include "mdrtuslave.h"
#include "io_signal.h"
#include "dwin.h"
#include "adc.h"
#include "tool.h"
#include "Flash.h"
#include "Mcp4822.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

Save_HandleTypeDef Save_Flash;
Save_User *puser = &Save_Flash.User;
// GPIO_PinState g_Status = GPIO_PIN_SET;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId ShellHandle;
osThreadId InputHandle;
osThreadId OutputHandle;
osThreadId UpdateHandle;
osThreadId ScreenHandle;
osThreadId RS485Handle;
osThreadId ContrlHandle;
osThreadId ReportHandle;
osMessageQId UserQueueHandle;
osTimerId mdtimerHandle;
osMutexId shellMutexHandle;
osSemaphoreId Recive_Uart1Handle;
osSemaphoreId Recive_Uart2Handle;
osSemaphoreId Recive_Uart3Handle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/**
 * @brief	Report screen background parameters
 * @details
 * @param	pd:Dewin obj
 * @param sp:data pointer
 * @retval	NULL
 */
void Report_Backparam(pDwinHandle pd, Save_Param *sp)
{
  uint32_t actual_size = offsetof(Save_Param, User_Name); // 29*szieof(float)
#if defined(USING_FREERTOS)
  float *pdata = (float *)CUSTOM_MALLOC(actual_size);
  if (!pdata)
    goto __exit;
#else
  float pdata[sizeof(Save_Param) - 1U];
#endif

  memcpy(pdata, sp, actual_size);
  for (float *p = pdata; p < pdata + actual_size / sizeof(float); p++)
  {
#if defined(USING_DEBUG)
    shellPrint(Shell_Object, "sp = %p,p = %p, *p = %.3f\r\n", sp, p, *p);
#endif
    Endian_Swap((uint8_t *)p, 0U, sizeof(float));
  }
  pd->Dw_Write(pd, PARAM_SETTING_ADDR, (uint8_t *)pdata, actual_size);
__exit:
  CUSTOM_FREE(pdata);
}

/**
 * @brief	Determine how the 4G module works
 * @details
 * @param	handler:modbus master/slave handle
 * @retval	true：MODBUS;fasle:shell
 */
bool Check_Mode(ModbusRTUSlaveHandler handler)
{
  ReceiveBufferHandle pB = handler->receiveBuffer;

  return (!!((pB->count == 1U) && (pB->buf[0] == ENTER_CODE)));
}
/**
 * @brief   enter shell mode
 * @details
 * @param	None
 * @retval	None
 */
// static void Shell_Mode(void)
// {
//   __HAL_UART_DISABLE_IT(&huart3, UART_IT_IDLE);
//   HAL_NVIC_DisableIRQ(USART3_IRQn);
//   if (HAL_UART_DMAStop(&huart3) != HAL_OK)
//   {
//     return;
//   }
//   /*Pending upgrade task*/
//   osThreadSuspend(UpdateHandle);
//   /*Resume shell task*/
//   osThreadResume(ShellHandle);
//   /*stop send timer*/
//   osTimerStop(mdtimerHandle);
// }

/**
 * @brief   exit shell mode
 * @details
 * @param	None
 * @retval	None
 */
bool Exit_Shell(void)
{
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
  if (HAL_UART_Receive_DMA(&huart3, mdRTU_Recive_Buf(Slave1_Object), MODBUS_PDU_SIZE_MAX) != HAL_OK)
  {
    return false;
  }
  /*Restore upgrade task*/
  osThreadResume(UpdateHandle);
  /*Suspend shell task*/
  osThreadSuspend(ShellHandle);
  /*Turn on send timer*/
  osTimerStart(mdtimerHandle, REPORT_TIMES);
  return true;
}
#if defined(USING_DEBUG)
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), exit_shell, Exit_Shell, exit);
#endif

/**
 * @brief	Background parameters are written back to modbus registers
 * @details
 * @param	ps Points to the address of the first background parameter
 * @retval	None
 */
void Param_WriteBack(Save_HandleTypeDef *ps)
{
  ps->Param.System_Flag = (*(__IO uint32_t *)UPDATE_SAVE_ADDRESS);
  ps->Param.System_Version = SYSTEM_VERSION();
  /*Parameters are written to the mdbus hold register*/
  mdSTATUS ret = mdRTU_WriteHoldRegs(Slave1_Object, PARAM_MD_ADDR, GET_PARAM_SITE(Save_Param, Adc, uint16_t),
                                     (mdU16 *)&ps->Param);
  if (ret == mdFALSE)
  {
#if defined(USING_DEBUG)
    shellPrint(Shell_Object, "Parameter write to hold register failed!\r\n");
#endif
  }
}
/* USER CODE END FunctionPrototypes */

void Shell_Task(void const *argument);
void Input_Task(void const *argument);
void Out_Task(void const *argument);
void Update_Task(void const *argument);
void Screen_Task(void const *argument);
void RS485_Task(void const *argument);
void Contrl_Task(void const *argument);
void Report_Task(void const *argument);
void MdTimer(void const *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize);

/* GetTimerTaskMemory prototype (linked to static allocation support) */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize);

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  /* Run time stack overflow checking is performed if
  configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
  called if a stack overflow is detected. */
  shellPrint(Shell_Object, "%s is stack overflow!\r\n", pcTaskName);
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
__weak void vApplicationMallocFailedHook(void)
{
  /* vApplicationMallocFailedHook() will only be called if
  configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
  function that will get called if a call to pvPortMalloc() fails.
  pvPortMalloc() is called internally by the kernel whenever a task, queue,
  timer or semaphore is created. It is also called by various parts of the
  demo application. If heap_1.c or heap_2.c are used, then the size of the
  heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
  FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
  to query the size of free heap space that remains (although it does not
  provide information on how the remaining heap might be fragmented). */
  shellPrint(Shell_Object, "memory allocation failed!\r\n");
}
/* USER CODE END 5 */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
  /* place for user code */
}
/* USER CODE END GET_TIMER_TASK_MEMORY */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of shellMutex */
  osMutexDef(shellMutex);
  shellMutexHandle = osMutexCreate(osMutex(shellMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of Recive_Uart1 */
  osSemaphoreDef(Recive_Uart1);
  Recive_Uart1Handle = osSemaphoreCreate(osSemaphore(Recive_Uart1), 1);

  /* definition and creation of Recive_Uart2 */
  osSemaphoreDef(Recive_Uart2);
  Recive_Uart2Handle = osSemaphoreCreate(osSemaphore(Recive_Uart2), 1);

  /* definition and creation of Recive_Uart3 */
  osSemaphoreDef(Recive_Uart3);
  Recive_Uart3Handle = osSemaphoreCreate(osSemaphore(Recive_Uart3), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* definition and creation of mdtimer */
  osTimerDef(mdtimer, MdTimer);
  mdtimerHandle = osTimerCreate(osTimer(mdtimer), osTimerPeriodic, (void *)Slave1_Object);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of UserQueue */
  osMessageQDef(UserQueue, 4, Save_User);
  UserQueueHandle = osMessageCreate(osMessageQ(UserQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of Shell */
  osThreadDef(Shell, Shell_Task, osPriorityLow, 0, 1024);
  ShellHandle = osThreadCreate(osThread(Shell), (void *)&shell);

  /* definition and creation of Input */
  osThreadDef(Input, Input_Task, osPriorityAboveNormal, 0, 256);
  InputHandle = osThreadCreate(osThread(Input), NULL);

  /* definition and creation of Output */
  osThreadDef(Output, Out_Task, osPriorityNormal, 0, 256);
  OutputHandle = osThreadCreate(osThread(Output), NULL);

  /* definition and creation of Update */
  osThreadDef(Update, Update_Task, osPriorityHigh, 0, 256);
  UpdateHandle = osThreadCreate(osThread(Update), (void *)Slave1_Object);

  /* definition and creation of Screen */
  osThreadDef(Screen, Screen_Task, osPriorityNormal, 0, 256);
  ScreenHandle = osThreadCreate(osThread(Screen), NULL);

  /* definition and creation of RS485 */
  osThreadDef(RS485, RS485_Task, osPriorityLow, 0, 128);
  RS485Handle = osThreadCreate(osThread(RS485), NULL);

  /* definition and creation of Contrl */
  osThreadDef(Contrl, Contrl_Task, osPriorityHigh, 0, 1024);
  ContrlHandle = osThreadCreate(osThread(Contrl), NULL);

  /* definition and creation of Report */
  osThreadDef(Report, Report_Task, osPriorityBelowNormal, 0, 256);
  ReportHandle = osThreadCreate(osThread(Report), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  // osTimerStart(reportHandle, REPORT_TIMES);
  osTimerStart(mdtimerHandle, MD_TIMES);
  // osThreadSuspend(ShellHandle);
  /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN Header_Shell_Task */
/**
 * @brief  Function implementing the Shell thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_Shell_Task */
void Shell_Task(void const *argument)
{
  /* USER CODE BEGIN Shell_Task */
  /* Infinite loop */
  for (;;)
  {
    shellTask((void *)argument);
  }
  /* USER CODE END Shell_Task */
}

/* USER CODE BEGIN Header_Input_Task */
/**
 * @brief Function implementing the Input thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Input_Task */
void Input_Task(void const *argument)
{
  /* USER CODE BEGIN Input_Task */
  /* Infinite loop */
  for (;;)
  {
    Read_Io_Handle();
#if defined(USING_DEBUG)
    osDelay(1000);
#else
    osDelay(100);
#endif
  }
  /* USER CODE END Input_Task */
}

/* USER CODE BEGIN Header_Out_Task */
/**
 * @brief Function implementing the Output thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Out_Task */
void Out_Task(void const *argument)
{
  /* USER CODE BEGIN Out_Task */
  /* Infinite loop */
  for (;;)
  {
    // Output_Current(&dac_object, 4.0F);
    Write_Io_Handle();
    osDelay(100);
  }
  /* USER CODE END Out_Task */
}

/* USER CODE BEGIN Header_Update_Task */
/**
 * @brief Function implementing the Update thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Update_Task */
void Update_Task(void const *argument)
{
  /* USER CODE BEGIN Update_Task */
  uint32_t update_flag = 0;
  /* Infinite loop */
  for (;;)
  {
    /*https://www.cnblogs.com/w-smile/p/11333950.html*/
    if (osOK == osSemaphoreWait(Recive_Uart3Handle, osWaitForever))
    {
      // Check_Mode((struct ModbusRTUSlave *)argument) ? (__set_FAULTMASK(1), NVIC_SystemReset()) : mdRTU_Handler(Slave1_Object);
      if (Check_Mode((struct ModbusRTUSlave *)argument))
      {
#if defined(USING_DEBUG)
        shellPrint(Shell_Object, "About to enter upgrade mode .......\r\n");
#endif
        /*Clear mcp4822 output*/
        extern Dac_Obj dac_object_group[EXTERN_ANALOGOUT_MAX];
        for (uint8_t ch = 0; ch < EXTERN_ANALOGOUT_MAX; ch++)
        {
          Output_Current(&dac_object_group[ch], 0);
        }
        /*Switch to the upgrade page*/
        if (Dwin_Object)
        {
#define Update_Page 0x0F
          Dwin_Object->Dw_Page(Dwin_Object, Update_Page);
        }

        update_flag = (*(__IO uint32_t *)UPDATE_SAVE_ADDRESS);

        if (((update_flag & 0xFFFF0000) >> 16U) == UPDATE_APP1)
        {
          update_flag = (((uint32_t)UPDATE_APP2 << 16U) | UPDATE_CMD);
        }
        else
        {
          update_flag = (((uint32_t)UPDATE_APP1 << 16U) | UPDATE_CMD);
        }
        // update_flag = ((*(__IO uint32_t *)UPDATE_SAVE_ADDRESS) & 0xFFFF0000) | UPDATE_CMD;
        taskENTER_CRITICAL();
        FLASH_Write(UPDATE_SAVE_ADDRESS, (uint16_t *)&update_flag, sizeof(update_flag));
        taskEXIT_CRITICAL();
        NVIC_SystemReset();
      }
      else
      {
        mdRTU_Handler(Slave1_Object);
      }
      // mdRTU_Handler(Slave1_Object);
    }
  }
  /* USER CODE END Update_Task */
}

/* USER CODE BEGIN Header_Screen_Task */
/**
 * @brief Function implementing the Screen thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Screen_Task */
void Screen_Task(void const *argument)
{
  /* USER CODE BEGIN Screen_Task */
  /* Infinite loop */
  for (;;)
  {
    /*https://www.cnblogs.com/w-smile/p/11333950.html*/
    if (osOK == osSemaphoreWait(Recive_Uart1Handle, osWaitForever))
    {
      /*Screen data distribution detected, close reporting*/
      // osTimerStop(reportHandle);
      Dwin_Handler(Dwin_Object);
      // osTimerStart(reportHandle, REPORT_TIMES);
#if defined(USING_DEBUG)
      // shellPrint(Shell_Object, "Dwin data!\r\n");
      // shellPrint(Shell_Object, "rx_count = %d\r\n", Dwin_Object->Slave.RxCount);
#endif
    }
  }
  /* USER CODE END Screen_Task */
}

/* USER CODE BEGIN Header_RS485_Task */
/**
 * @brief Function implementing the RS485 thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_RS485_Task */
void RS485_Task(void const *argument)
{
  /* USER CODE BEGIN RS485_Task */
  /* Infinite loop */
  for (;;)
  {
    /*https://www.cnblogs.com/w-smile/p/11333950.html*/
    if (osOK == osSemaphoreWait(Recive_Uart2Handle, osWaitForever))
    {

#if defined(USING_DEBUG)
// shellPrint(Shell_Object, "RS485 data!\r\n");
#else
      mdRTU_Handler(Slave2_Object);
#endif
    }
  }
  /* USER CODE END RS485_Task */
}

/* USER CODE BEGIN Header_Contrl_Task */
/**
 * @brief Function implementing the Contrl thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Contrl_Task */
void Contrl_Task(void const *argument)
{
/* USER CODE BEGIN Contrl_Task */
#define PI 3.2415926F
#define OFFSET 4U
#define BX_SIZE 4U
#if defined(USING_LYNCH)
#define VX_SIZE 6U
#else
#define VX_SIZE 5U
#endif
#define ACTION_TIMES 1000U
#define ACTION_DELAY_TIMES (ACTION_TIMES * 5U)
#define STIMES (10U * 1000U / ACTION_TIMES)
#define CURRENT_UPPER 16.0F
#define CURRENT_LOWER 4.0F
#define Get_Target(__current, __upper, __lower) \
  (((__current)-CURRENT_LOWER) / CURRENT_UPPER * ((__upper) - (__lower)) + (__lower))
#define Get_Tank_Level(__R, __L, __H) \
  (PI * ((__R) * (__R) * (__L) + (__H) * (__H) * (__H) / 96.0F))
#define Set_SoftTimer_Flag(__obj, __val) \
  ((__obj)->flag = (__val))
#define Sure_Overtimes(__obj, __times)                                     \
  ((__obj)->flag ? (++(__obj)->counts >= (__times) ? (__obj)->counts = 0U, \
                    (__obj)->flag = false, true : false)                   \
                 : false)
#define Clear_Counter(__obj) ((__obj)->counts = 0U)
#define Open_Vx(__x) ((__x) <= VX_SIZE ? wbit[__x - 1U] = true : false)
#define Close_Vx(__x) ((__x) <= VX_SIZE ? wbit[__x - 1U] = false : false)
  mdBit sbit = mdLow, mode = mdLow;
  // mdU32 addr;
  mdSTATUS ret = mdFALSE;
#if defined(USING_LYNCH)
  static mdBit wbit[] = {false, false, false, false, false, false};
#else
  static mdBit wbit[] = {false, false, false, false, false};
#endif
  static Soft_Timer_HandleTypeDef timer[] = {
      {.counts = 0, .flag = false},
      {.counts = 0, .flag = false},
  };
  Save_HandleTypeDef *ps = &Save_Flash;
  Save_User usinfo;
  static bool mutex_flag = false;
  uint32_t next_times = 0;

  /* Infinite loop */
  for (;;)
  {
    uint32_t error_code = 0;
    // goto __no_action;

    next_times = ACTION_TIMES;
    // memset((void *)&pas->User, 0x00, BX_SIZE * 2U);
    memset((void *)&ps->User, 0x00, BX_SIZE * 2U);
    // ret = mdRTU_ReadInputRegisters(Slave1_Object, INPUT_ANALOG_START_ADDR, BX_SIZE * 2U, (mdU16 *)&pas->User);
    ret = mdRTU_ReadInputRegisters(Slave1_Object, INPUT_ANALOG_START_ADDR, BX_SIZE * 2U, (mdU16 *)&ps->User);
    if (ret == mdFALSE)
    {
#if defined(USING_DEBUG)
      shellPrint(Shell_Object, "Failed to read input register!\r\n");
#endif
      goto __no_action;
    }
    taskENTER_CRITICAL();
    for (float *p = &ps->User.Ptank, *pu = &ps->Param.Ptank_max, *pinfo = (float *)&usinfo;
         p < &ps->User.Ptank + BX_SIZE; p++, pu += 2U, pinfo++)
    {
#define ERROR_BASE_CODE 0x02
      uint8_t site = p - &ps->User.Ptank;
#if defined(USING_DEBUG)
      // shellPrint(Shell_Object, "R_Current[0x%X] = %.3f\r\n", p, *p);
#endif
      // Endian_Swap((uint8_t *)p, 0U, sizeof(float));
      /*User sensor access error check*/
#define ERROR_CHECK
      {
        if (!error_code && (site != 0x02))
          error_code = *p <= 0.5F
                           ? (3U * site + ERROR_BASE_CODE)
                           : (*p < (CURRENT_LOWER - 0.5F) /*Disconnection detection offset 0.5mA*/
                                  ? (3U * site + ERROR_BASE_CODE + 1U)
                                  : (*p > (CURRENT_LOWER + CURRENT_UPPER + 1.0F)
                                         ? (3U * site + ERROR_BASE_CODE + 2U)
                                         : 0));
      }
      /*Convert analog signal into physical quantity*/
      *p = (0x03 & site) ? Get_Tank_Level(ps->Param.Ptoler_upper, Get_Target(*p, *pu, *(pu + 1U)), ps->Param.Ptoler_lower)
                         : Get_Target(*p, *pu, *(pu + 1U)); // Calculate the current liquid level volume of vertical storage tank
      *p = *p <= 0.0F ? 0 : *p;
      *pinfo = *p;
#if defined(USING_DEBUG)
      // shellPrint(Shell_Object, "max = %.3f,min = %.3f, C_Value[0x%p] = %.3fMpa/M3\r\n", *pu, *(pu + 1U), p, *p);
#endif
    }
#if !defined(USING_B3)
    /*Shield vaporizer outlet pressure failure*/
    if ((error_code == 0x08) || (error_code == 0x09))
      error_code = 0;
#endif
    ps->Param.Error_Code = error_code;
    taskEXIT_CRITICAL();
    xQueueSend(UserQueueHandle, &usinfo, 10);

#if defined(USING_DEBUG)
    // shellPrint(Shell_Object, "ps->User.Ptank = %.3f\r\n", ps->User.Ptank);
#endif

    // Open_Vx(1U), Open_Vx(2U), Open_Vx(3U), Open_Vx(4U);
    // goto __no_action;

    /*Read start signal*/
    ret = mdRTU_ReadInputCoil(Slave1_Object, INPUT_DIGITAL_START_ADDR, sbit);
    if (ret == mdFALSE)
    {
#if defined(USING_DEBUG)
      shellPrint(Shell_Object, "IN_D[%d] = 0x%d\r\n", INPUT_DIGITAL_START_ADDR, sbit);
#endif
      goto __no_action;
    }
    /*Manual mode management highest authority*/
    ret = mdRTU_ReadCoil(Slave1_Object, M_MODE_ADDR, mode);
    if (ret == mdFALSE)
    {
#if defined(USING_DEBUG)
      shellPrint(Shell_Object, "Mode[%d] = 0x%d\r\n", INPUT_DIGITAL_START_ADDR, mode);
#endif
      goto __no_action;
    }
    if (mode == mdHigh)
    {
#if defined(USING_DEBUG)
      shellPrint(Shell_Object, "Note: Manual mode startup, automatic management failure!\r\n");
#endif
      /*Clear error codes*/
      ps->Param.Error_Code = 0;
      goto __exit;
    }
    /*Safe operation guarantee*/
#define SAFETY
    { /*V1在开始�?�停机模式只有打�?????，没有关闭！！！*/
      if ((ps->User.Ptank <= ps->Param.Ptank_limit) ||
          (ps->User.Ltank <= ps->Param.Ltank_limit) || (ps->Param.Error_Code))
      {
#if defined(USING_LYNCH)
        /*close V1、V2、V3, open V6*/
        Close_Vx(1U), Close_Vx(2U), Close_Vx(3U), Close_Vx(4U), Close_Vx(5U), Open_Vx(6U);
#else
        /*close V1、V2、V3*/
        Close_Vx(1U), Close_Vx(2U), Close_Vx(3U), Close_Vx(4U), Close_Vx(5U);
#endif
#if defined(USING_DEBUG_APPLICATION)
        shellPrint(Shell_Object, "SAF: close V1 V2 V3; open V6\r\n");
#endif
        goto __no_action;
      }
    }
#if defined(USING_DEBUG_APPLICATION)
    shellPrint(Shell_Object, "sbit = 0x%d\r\n", sbit);
#endif
    /*Start mode*/
    if (sbit)
    {
#define D1
      {
        /*close V4*/
        Close_Vx(4U);
#if defined(USING_DEBUG_APPLICATION)
        shellPrint(Shell_Object, "D1: close V4\r\n");
#endif
      }
#define A1
      {
#if defined(USING_DEBUG_APPLICATION)
        // shellPrint(Shell_Object, "ps->User.Ptank = %.3f, ps->Param.PSspf_start = %.3f\r\n", ps->User.Ptank, ps->Param.PSspf_start);
#endif
        /*Pressure relief*/
        if (ps->User.Ptank >= ps->Param.PSspf_start)
        { /*open counter*/
          // Set_SoftTimer_Flag(&timer[0U], true);

#if defined(USING_LYNCH)
          /*close V1、V6;open V2、V3*/
          Close_Vx(1U), Open_Vx(2U), Open_Vx(3U), Close_Vx(6U);
#else
          /*close V1、open V2、V3*/
          Close_Vx(1U), Open_Vx(2U), Open_Vx(3U);
#endif
          /*set flag*/
          mutex_flag = true;
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "A1: close V1 V6; open V2 V3\r\n");
#endif
          goto __no_action;
        }
        else
        {
          if (ps->User.Ptank <= ps->Param.PSspf_stop)
          {
            mutex_flag = false;
            /*close V2 、close V4*/
            Close_Vx(2U), Close_Vx(4U);
#if defined(USING_DEBUG_APPLICATION)
            shellPrint(Shell_Object, "A1: close V2 V4\r\n");
#endif
          }
          else
          {
            if (mutex_flag)
            {
              goto __no_action;
            }
            else
            {
              /*openV3*/
              Open_Vx(3U);
#if defined(USING_DEBUG_APPLICATION)
              shellPrint(Shell_Object, "A1: open V3\r\n");
#endif
            }
          }
        }
      }
#define B1C1
      {
        if (ps->User.Pvap_outlet >= ps->Param.PSvap_outlet_Start)
        {
          Set_SoftTimer_Flag(&timer[1U], true);
          if (Sure_Overtimes(&timer[1U], STIMES))
          {
            // osDelay(10000);
            /*open V3*/
            Open_Vx(3U);
#if defined(USING_DEBUG_APPLICATION)
            shellPrint(Shell_Object, "B1C1: open V3\r\n");
#endif
          }
          /*open V1 、close V2*/
          Open_Vx(1U), Close_Vx(2U);
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "B1C1: open V1 close V2\r\n");
#endif
        }
        else if (ps->User.Pvap_outlet <= ps->Param.PSvap_outlet_stop)
        {
          /*Clear counter*/
          Clear_Counter(&timer[1U]);
          /*open counter*/
          // Set_SoftTimer_Flag(&timer[1U], true);
          /*close V3*/
          Close_Vx(3U), Close_Vx(1U), Open_Vx(2U);
          next_times = ACTION_DELAY_TIMES; // 解决气相阀打开时间太短导致的震荡
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "B1C1: close V3 close V1 open V2\r\n");
#endif
        }
      }
    }
    /*stop mode*/
    else
    {
      /*Check whether the pressure relief operation is on*/
      if (mutex_flag)
      {
        mutex_flag = false;
      }
#define A2
      {
        if (((ps->User.Pvap_outlet - ps->User.Ptank) >= ps->Param.Pback_difference) &&
            (ps->User.Ptank < ps->Param.Ptank_difference))
        {
          /*open V2*/
          Open_Vx(2U);
          next_times = ACTION_DELAY_TIMES; // 解决气相阀打开时间太短导致的震荡
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "A2: open V2\r\n");
#endif
        }
        else
        {
          /*Close V2*/
          Close_Vx(2U);
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "A2: Close V2\r\n");
#endif
        }
      }
#define B2
      {
        if (ps->User.Pvap_outlet >= ps->Param.PPvap_outlet_Start)
        {
          /*open V3、open V5、close V2*/
          Open_Vx(3U), Open_Vx(5U), Close_Vx(2U);
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "B2: open V3 open V5 close V2\r\n");
#endif
        }
        else if (ps->User.Pvap_outlet <= ps->Param.PPvap_outlet_stop)
        {
          /*close V3*/
          Close_Vx(3U), Close_Vx(5U);
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "B2: close V3 close V5\r\n");
#endif
        }
      }
#define C2
      {
        if (ps->User.Ptank >= ps->Param.PPspf_start)
        {
          /*open V4*/
          Open_Vx(4U);
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "C2: open V4\r\n");
#endif
        }
        else if (ps->User.Ptank <= ps->Param.PPspf_stop)
        {
          /*close V4*/
          Close_Vx(4U);
#if defined(USING_DEBUG_APPLICATION)
          shellPrint(Shell_Object, "C2: close V4\r\n");
#endif
        }
      }
#define D2
      {
        /*close V1*/
        Close_Vx(1U);
#if defined(USING_DEBUG_APPLICATION)
        shellPrint(Shell_Object, "D2: close V1\r\n");
#endif
      }
    }
  __no_action:
#if defined(USING_DEBUG_APPLICATION)
    for (uint16_t i = 0; i < VX_SIZE; i++)
    {
      // ret = mdRTU_WriteCoil(Slave1_Object, i, wbit[i]);
      shellPrint(Shell_Object, "wbit[%d] = 0x%x\r\n", i, wbit[i]);
    }
#endif
    ret = mdRTU_WriteCoils(Slave1_Object, OUT_DIGITAL_START_ADDR, VX_SIZE, wbit);
    if (ret == mdFALSE)
    {
#if defined(USING_DEBUG)
      shellPrint(Shell_Object, "write failed!\r\n");
#endif
    }
  __exit:
    osDelay(next_times);
  }
  /* USER CODE END Contrl_Task */
}

/* USER CODE BEGIN Header_Report_Task */
/**
 * @brief Function implementing the Report thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Report_Task */
void Report_Task(void const *argument)
{
  /* USER CODE BEGIN Report_Task */
  mdU16 buf[] = {0, 0};
  uint8_t bit = 0;
  static bool first_flag = false;
  Save_HandleTypeDef *ps = &Save_Flash;
  Save_User urinfo;
  uint16_t size = sizeof(buf) + sizeof(urinfo) + (sizeof(float) * ADC_DMA_CHANNEL);
#if defined(USING_FREERTOS)
  uint8_t data[sizeof(buf) + sizeof(urinfo) + (sizeof(float) * ADC_DMA_CHANNEL)], *pdata = data;
  // uint8_t *pdata = (uint8_t *)CUSTOM_MALLOC(size);
  // if (!pdata)
  //   goto __exit;
#else
  float pdata[ADC_DMA_CHANNEL];
#endif
  /* Infinite loop */
  for (;;)
  {
    memset(pdata, 0x00, size);
    memset(buf, 0x00, sizeof(buf));
    for (uint16_t i = 0; i < EXTERN_DIGITALIN_MAX; i++)
    {
      mdRTU_ReadInputCoil(Slave1_Object, INPUT_DIGITAL_START_ADDR + i, bit);
      buf[0] |= bit << (i + 8U);
      mdRTU_ReadCoil(Slave1_Object, OUT_DIGITAL_START_ADDR + i, bit);
      buf[1] |= bit << (i + 8U);
    }
    /*Read 4G module status*/
    buf[0] |= Read_LTE_State();
    memcpy(pdata, buf, sizeof(buf));
#if defined(USING_DEBUG)
    // shellPrint(Shell_Object, "rbit = 0x%02x.\r\n", rbit);
#endif
    BaseType_t ret = xQueueReceive(UserQueueHandle, (void *)&urinfo, 10);
    if (ret == pdPASS) // 使用有效的用户数据
    {
      for (float *puser = &urinfo.Ptank; puser < &urinfo.Ptank + BX_SIZE; puser++)
      {
#if defined(USING_DEBUG)
        // shellPrint(Shell_Object, "Value[%d] = %.3fMpa/M3\r\n", i, temp_data[i]);
#endif
        Endian_Swap((uint8_t *)puser, 0U, sizeof(float));
      }
    }
    memcpy(&pdata[sizeof(buf)], &urinfo, sizeof(urinfo));

    /*Analog input*/
    mdRTU_ReadInputRegisters(Slave1_Object, INPUT_ANALOG_START_ADDR, ADC_DMA_CHANNEL * 2U,
                             (mdU16 *)&pdata[sizeof(buf) + sizeof(urinfo)]);
    for (uint8_t i = 0; i < ADC_DMA_CHANNEL; i++)
    {
      Endian_Swap((uint8_t *)&pdata[sizeof(buf) + sizeof(urinfo) + i * sizeof(float)], 0U, sizeof(float));
    }
    Dwin_Object->Dw_Write(Dwin_Object, DIGITAL_INPUT_ADDR, (uint8_t *)pdata, size);
    osDelay(5);

    /*Analog output*/
    mdRTU_ReadHoldRegisters(Slave1_Object, OUT_ANALOG_START_ADDR, EXTERN_ANALOGOUT_MAX * 2U, (mdU16 *)pdata);
    for (uint8_t i = 0; i < EXTERN_ANALOGOUT_MAX; i++)
    {
      Endian_Swap((uint8_t *)&pdata[i * sizeof(float)], 0U, sizeof(float));
    }
    Dwin_Object->Dw_Write(Dwin_Object, ANALOG_OUTPUT_ADDR, (uint8_t *)pdata, EXTERN_ANALOGOUT_MAX * sizeof(float));
    osDelay(5);
    /*Report error code*/
    if (ps->Param.Error_Code)
    {
#define ERROR_PAGE 0x0E
#define MAIN_PAGE 0x03
      buf[0] = (uint16_t)ps->Param.Error_Code;
      buf[0] = (buf[0] >> 8U) | (buf[0] << 8U);
      buf[1] = 0x0100;
      Dwin_Object->Dw_Write(Dwin_Object, ERROR_CODE_ADDR, (uint8_t *)buf, sizeof(buf));
      osDelay(5);
      Dwin_Object->Dw_Page(Dwin_Object, ERROR_PAGE);
      first_flag = false;
    }
    else
    {
      if (!first_flag)
      {
        first_flag = true;
        Dwin_Object->Dw_Page(Dwin_Object, MAIN_PAGE);
      }
    }
    /*Parameter write back to holding register*/
    // extern void Param_WriteBack(Save_HandleTypeDef * ps);
    Param_WriteBack(ps);
#if defined(USING_FREERTOS)
    // __exit:
    //   CUSTOM_FREE(pdata);
#endif
    osDelay(REPORT_TIMES);
  }
  /* USER CODE END Report_Task */
}

/* MdTimer function */
void MdTimer(void const *argument)
{
  /* USER CODE BEGIN MdTimer */
  ModbusRTUSlaveHandler pMd = (ModbusRTUSlaveHandler)argument;
  // g_Status ^= GPIO_PIN_SET;
  /*Hardware watchdog feed dog*/
  // HAL_GPIO_WritePin(WDI_GPIO_Port, WDI_Pin, g_Status);
  HAL_GPIO_TogglePin(WDI_GPIO_Port, WDI_Pin);

  if (!pMd)
  {
    return;
  }
  /*考虑加互斥锁：在update�??????????46指令*/
  // mdRTU_46H(pMd->slaveId, 0x00, 0x00, 0x00, NULL);
  //  pMd->mdRTU_46H(pMd, 0x00, 0x00, 0x00, NULL);

#if defined(USING_DEBUG)
/****************Digital input and output test********************/
// uint8_t bit = 0;
// /*Test digital input and output*/
// for (uint16_t i = 0; i < EXTERN_DIGITALIN_MAX; i++)
// {
//   mdRTU_ReadInputCoil(Slave1_Object, i, bit);
//   if (i < EXTERN_DIGITALOUT_MAX)
//   {
//     mdRTU_WriteCoil(Slave1_Object, i, bit);
//   }
#if defined(USING_DEBUG)
  //   shellPrint(Shell_Object, "IN_D[%d] = 0x%x\r\n", i, bit);
#endif
  // }
  /****************Analog input test*********************/
  // mdSTATUS ret = mdFALSE;
  // float pdata[ADC_DMA_CHANNEL];
  // memset(pdata, 0x00, sizeof(pdata));
  // /*读输入寄存器*/
  // ret = mdRTU_ReadInputRegisters(Slave1_Object, INPUT_ANALOG_START_ADDR, ADC_DMA_CHANNEL * 2U, (mdU16 *)&pdata);
  // // ret = Slave1_Object->registerPool->mdReadInputRegisters(Slave1_Object->registerPool, INPUT_ANALOG_START_ADDR, ADC_DMA_CHANNEL * 2U, (mdU16 *)&pdata);
  // for (uint16_t ch = 0; ch < ADC_DMA_CHANNEL; ch++)
  // {
  //   Endian_Swap((uint8_t *)&pdata[ch], 0U, sizeof(float));
#if defined(USING_DEBUG)
  // shellPrint(Shell_Object, "R_AD[%d] = %.3f\r\n", ch, pdata[ch]);
#endif
  // }

  /****************Analog output test*********************/
  // float pdata[EXTERN_ANALOGOUT_MAX] = {20.0F, 20.0F, 20.0F, 20.0F};
  // for (uint16_t ch = 0; ch < EXTERN_ANALOGOUT_MAX; ch++)
  //   Endian_Swap((uint8_t *)&pdata[ch], 0U, sizeof(float));
  // mdRTU_WriteHoldRegs(Slave1_Object, OUT_ANALOG_START_ADDR, EXTERN_ANALOGOUT_MAX * 2U, (mdU16 *)&pdata);
#endif
  /* USER CODE END MdTimer */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
