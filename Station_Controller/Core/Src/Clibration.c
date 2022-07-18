#include "clibration.h"
// #include "charger.h"
#include "usart.h"
// #include "adc.h"
#include "Mcp4822.h"
#include "shell.h"
#include "shell_port.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "io_signal.h"
#include "tool.h"

extern osTimerId reportHandle;
extern osTimerId mdtimerHandle;

// #define RECEIVE_TARET_UART huart1

ADC_Calibration_HandleTypeDef Adc = {
	.P = 0,
	.Q = 0,
	.X = {0},
	.Y = {1.0F, 4.0F, 8.0F, 12.0F, 16.0F, 20.0F},
};
DAC_Calibration_HandleTypeDef Dac = {0};

static const char *clibrationText[] =
	{
		[CLIBRATION_OK] = "Success: Calibration successful !\r\n",
		[PARSING_SUCCEEDED] = "Success: Parsing succeeded !\r\n",
		[USER_CANCEL] = "Note: User cancel !\r\n",
		[DATA_TOO_LONG] = "Warning: data is too long !\r\n",
		[INPUT_ILLEGAL] = "Warning: Illegal input !\r\n",
		// [RE_ENTER] = "\r\nNote: Please re-enter !\r\n"
};

/**
 * @brief	对终端输入的浮点数进行解析
 * @details
 * @param	None
 * @retval	None
 */
Clibration_Error Input_FloatPaser(Shell *shell, float *pFloat)
{
	char recive_data = '\0';
	uint8_t arry[BYTE_SIZE] = {0};
	uint16_t len = 0;
	uint16_t site = 0;

	while (1)
	{
		*pFloat = 0;
		if (shell->read(&recive_data, 0x01))
		{
			switch (recive_data)
			{
			case ESC_CODE:
			{
				// shellWriteString(shell, "User Cancel !\r\n");
				return USER_CANCEL;
			}
			case BACKSPACE_CODE:
			{
				if (len)
				{
					len--;
				}
				shellDeleteCommandLine(shell, 0x01);
				break;
			}
			case ENTER_CODE:
			{ /*首次就输入了小数点*/
				if (!site)
				{
					return INPUT_ILLEGAL;
				}

				// SHELL_LOCK(shell);
				// shellPrint(shell, "len:%d,data:%S\r\n", len, arry);
				// SHELL_UNLOCK(shell);

				for (uint16_t i = 0; i < len; i++)
				{
					if (i < site)
					{
						*pFloat += ((float)arry[i]) * powf(10.0F, site - (i + 1U));
					}
					else
					{
						*pFloat += ((float)arry[i]) * powf(10.0F, -1.0F * (float)(i - site + 1U));
					}
					// Usart1_Printf("pFloat:%f\r\n", *pFloat);
					// break;
				}
				shellPrint(shell, "\r\npFloat:%f\r\n", *pFloat);
				return PARSING_SUCCEEDED;
			}
			case SPOT_CODE: /*跳过小数点*/
			{				/*记录小数点位置*/
				site = len;
				// SHELL_LOCK(shell);
				// shellPrint(shell, "site:%d\r\n", site);
				// SHELL_UNLOCK(shell);
				break;
			}
			default:
			{
				if ((recive_data >= '0') && (recive_data <= '9'))
				{
					arry[len] = recive_data - '0';
					// SHELL_LOCK(shell);
					// shellPrint(shell, "recive data[%d]:%d\r\n", len, arry[len]);
					// SHELL_UNLOCK(shell);
					if (++len > BYTE_SIZE)
					{
						return DATA_TOO_LONG;
					}
				}
				else
				{
					return INPUT_ILLEGAL;
				}
				// Usart1_Printf("recive data[%d]:%d\r\n", recive_data);
				break;
			}
			}

			if (recive_data != BACKSPACE_CODE)
			{
				/*写回到控制台*/
				shell->write(&recive_data, 0x01);
			}
		}
	}
}

/**
 * @brief	ADC与采集电压间自动校准
 * @details	通过lettershell自动映射
 * @param	None
 * @retval	None
 */
bool Adc_Clibration(void)
{
	Shell *pShell = &shell;
	float p_sum = 0, q_sum = 0;
	Clibration_Error error;
	char re_data = '\0';
	Save_HandleTypeDef *ps = &Save_Flash;
	extern Dac_Obj dac_object_group[EXTERN_ANALOGOUT_MAX];
#define SAMPLING_NUM 8U

	osTimerStop(reportHandle);
	osTimerStop(mdtimerHandle);
	osThreadSuspendAll();

	for (uint8_t i = 0; i < ADC_POINTS; i++)
	{
		re_data = '\0';
		while (re_data != ENTER_CODE)
		{
			pShell->read(&re_data, 0x01);
			HAL_Delay(1000);
			// osDelay(1000);
			/*存储ADC值:默认获取通道0*/
			// Adc.X[i] = Get_AdcValue(0x07);
			// shellPrint(pShell, "\r\n@Current ADC[%d] value is:%d\r\n", i, Get_AdcValue(0x07));
			shellPrint(pShell, "\r\n@Please confirm the target value,ADC[%d] value is:%d.\r\n", i, Get_AdcValue(0x07));
		}
		/*求取8次平均*/
		uint32_t data = 0;
		for (uint8_t i = 0; i < SAMPLING_NUM; i++)
		{
			data += Get_AdcValue(0x07);
		}
		/*存储ADC值:默认获取通道0*/
		Adc.X[i] = data >> 3U;
		shellPrint(pShell, "\r\n@note:ADC[%d]_AVG = %d,Please enter the target current value.\r\n", i, Adc.X[i]);
	start:
		error = Input_FloatPaser(pShell, &Adc.Y[i]);
		shellWriteString(pShell, clibrationText[error]);
		if ((error == DATA_TOO_LONG) || (error == INPUT_ILLEGAL))
		{
			goto start;
		} /*用户取消，校准失败*/
		else if (error == USER_CANCEL)
			goto __exit;
		if (i)
		{
			Adc.P[i - 1U] = (Adc.Y[i] - Adc.Y[i - 1U]) / (float)(Adc.X[i] - Adc.X[i - 1U]);
			p_sum += Adc.P[i - 1U];
			Adc.Q[i - 1U] = Adc.Y[i - 1U] - Adc.P[i - 1U] * (float)Adc.X[i - 1U];
			q_sum += Adc.Q[i - 1U];
			shellPrint(pShell, "P[%d]:%f, Q[%d]:%f\r\n", i - 1U, Adc.P[i - 1U], i - 1U, Adc.Q[i - 1U]);
		}
	}
	if (ps)
	{
		/*对ADC_POINTS求取平均值*/
		ps->Param.Adc.Cp = p_sum / (ADC_POINTS - 1U);
		ps->Param.Adc.Cq = q_sum / (ADC_POINTS - 1U);
		shellPrint(pShell, "SP:%f,SQ:%f\r\n", ps->Param.Adc.Cp, ps->Param.Adc.Cq);
#if defined(USING_FREERTOS)
		taskENTER_CRITICAL();
#endif
		/*计算crc校验码*/
		ps->Param.crc16 = Get_Crc16((uint8_t *)&ps->Param, sizeof(Save_Param) - sizeof(ps->Param.crc16), 0xFFFF);
		/*参数保存到Flash*/
		FLASH_Write(PARAM_SAVE_ADDRESS, (uint16_t *)&ps->Param, sizeof(Save_Param));
#if defined(USING_FREERTOS)
		taskEXIT_CRITICAL();
#endif
		shellPrint(pShell, "@ADC parameters stored successfully!\r\n");
	}
__exit:
	/*校准完成后恢复任务和定时器*/
	osTimerStart(reportHandle, 1000);
	osTimerStart(mdtimerHandle, 1000);
	osThreadResumeAll();

	return true;
}
// #if defined(USING_DEBUG)
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), adc_clibration, Adc_Clibration, clibration);
// #endif

/**
 * @brief	DAC与输出电压间自动校准
 * @details	通过lettershell自动映射
 * @param	None
 * @retval	None
 */
bool Dac_Clibration(void)
{
	float k_sum = 0, g_sum = 0;
	Shell *pShell = &shell;
	Clibration_Error error;
	Save_HandleTypeDef *ps = &Save_Flash;
	extern Dac_Obj dac_object_group[EXTERN_ANALOGOUT_MAX];

	/*校准前挂起所有无关任务,只保留运行指示灯*/
	osTimerStop(reportHandle);
	osTimerStop(mdtimerHandle);
	osThreadSuspendAll();

	for (uint8_t ch = 0; ch < EXTERN_ANALOGOUT_MAX; ch++)
	{
		k_sum = g_sum = 0;
		for (uint8_t k = 0; k < DAC_POINTS; k++)
		{
			if (k)
				/*记录输入给DA的值*/
				Dac.X[ch][k] = 512U * k - 1U;
			shellPrint(pShell, "Dac.x[%d][%d]%d\r\n", ch, k, Dac.X[ch][k]);
			Coutput_Current(&dac_object_group[ch], Dac.X[ch][k]);
		start:
			error = Input_FloatPaser(pShell, &Dac.Y[ch][k]);
			shellWriteString(pShell, clibrationText[error]);
			if ((error == DATA_TOO_LONG) || (error == INPUT_ILLEGAL))
			{
				goto start;
			} /*用户取消，校准失败*/
			else if (error == USER_CANCEL)
				goto __exit;

			if (k)
			{
				Dac.K[ch][k - 1U] = (float)(Dac.X[ch][k] - Dac.X[ch][k - 1U]) / (Dac.Y[ch][k] - Dac.Y[ch][k - 1U]);
				k_sum += Dac.K[ch][k - 1U];
				Dac.G[ch][k - 1U] = (float)Dac.X[ch][k - 1U] - Dac.K[ch][k - 1U] * Dac.Y[ch][k - 1U];
				g_sum += Dac.G[ch][k - 1U];
				shellPrint(pShell, "k[%d][%d]:%f, G[%d][%d]:%f\r\n", ch, k - 1U, Dac.K[ch][k - 1U], ch, k - 1U,
						   Dac.G[ch][k - 1U]);
			}
		}
		ps->Param.Dac.Dac[ch][0] = k_sum / (float)(DAC_POINTS - 1U);
		ps->Param.Dac.Dac[ch][1] = g_sum / (float)(DAC_POINTS - 1U);
		shellPrint(pShell, "SK[%d][0]:%f,SG[%d][1]:%f\r\n", ch, ps->Param.Dac.Dac[ch][0], ch, ps->Param.Dac.Dac[ch][1]);
		/*清除DA输出值*/
		Coutput_Current(&dac_object_group[ch], 0);
	}
#if defined(USING_FREERTOS)
	taskENTER_CRITICAL();
#endif
	/*计算crc校验码*/
	ps->Param.crc16 = Get_Crc16((uint8_t *)&ps->Param, sizeof(Save_Param) - sizeof(ps->Param.crc16), 0xFFFF);
	/*参数保存到Flash*/
	FLASH_Write(PARAM_SAVE_ADDRESS, (uint16_t *)&ps->Param, sizeof(Save_Param));
#if defined(USING_FREERTOS)
	taskEXIT_CRITICAL();
#endif
	shellPrint(pShell, "@DAC parameters stored successfully!\r\n");
__exit:
	/*校准完成后恢复任务和定时器*/
	osTimerStart(reportHandle, 1000);
	osTimerStart(mdtimerHandle, 1000);
	osThreadResumeAll();

	return true;
}

// #if defined(USING_DEBUG)
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), dac_clibration, Dac_Clibration, clibration);
// #endif

/**
 * @brief	查看ADC校准系数
 * @details	通过lettershell自动映射
 * @param	None
 * @retval	None
 */
void See_ADC_Param(void)
{
	Save_HandleTypeDef *ps = &Save_Flash;
	shellPrint(&shell, "SP:%f\t\tSQ:%f\r\n", ps->Param.Adc.Cp, ps->Param.Adc.Cq);
}
// #if defined(USING_DEBUG)
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), see_adc, See_ADC_Param, see);
// #endif

/**
 * @brief	查看DAC校准系数
 * @details	通过lettershell自动映射
 * @param	None
 * @retval	None
 */
void See_DAC_Param(void)
{
	Save_HandleTypeDef *ps = &Save_Flash;
	shellPrint(&shell, "\tSK\t\tSG\r\n");
	for (uint16_t i = 0; i < EXTERN_ANALOGOUT_MAX; i++)
	{
		shellPrint(&shell, "CH[%d]:%f\t\t%f\r\n", i, ps->Param.Dac.Dac[i][0], ps->Param.Dac.Dac[i][1]);
	}
}
// #if defined(USING_DEBUG)
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), see_dac, See_DAC_Param, see);
// #endif

/**
 * @brief	检查DAC、ADC校准后精度是否满足
 * @details	通过lettershell自动映射
 * @param	None
 * @retval	None
 */
void Check_ADC(uint8_t ch, uint8_t id)
{
	//	Shell *pShell = &shell;
	/*恢复充电调节任务*/
	// osThreadResume(ChargingHandle);
	// osThreadResumeAll();
}
// #if defined(USING_DEBUG)
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), check_adc, Check_ADC, check);
// #endif
