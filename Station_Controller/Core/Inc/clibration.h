#ifndef __CLIBRATION_H
#define __CLIBRATION_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "adc.h"
#include "Flash.h"

/*ADC校准参数保存地址*/
#define ADC_CLIBRATION_SAVE_ADDR (STM32FLASH_BASE + 64U * FLASH_PAGE_SIZE)
/*DAC校准参数保存地址*/
#define DAC_CLIBRATION_SAVE_ADDR (STM32FLASH_BASE + 65U * FLASH_PAGE_SIZE)
/*校准完成后写入值*/
#define SURE_CODE 0x38382424
/*ADC校准时采集点数*/
#define ADC_POINTS 6U
#define DAC_POINTS 9U
#define DAC_NUMS 12U
#define PERMIT_ERROR 0.01F
#define ERROR_BASE 5.0F
#define ERROR_RATIO1 5.0F
#define ERROR_RATIO2 7.0F

/*允许输入的电压字节数*/
#define BYTE_SIZE 5U

	typedef enum
	{
		CLIBRATION_OK = 0,
		PARSING_SUCCEEDED,
		USER_CANCEL,
		DATA_TOO_LONG,
		INPUT_ILLEGAL,
		// RE_ENTER
	} Clibration_Error;

	typedef struct
	{
		float Y[ADC_POINTS];
		uint32_t X[ADC_POINTS];
		float P[ADC_POINTS], Q[ADC_POINTS];
		// float SP, SQ;
	} ADC_Calibration_HandleTypeDef __attribute__((aligned(4)));

	typedef struct
	{
		float Y[4][DAC_POINTS];
		uint32_t X[4][DAC_POINTS];
		float K[4][DAC_POINTS];
		float G[4][DAC_POINTS];
		// float SKx[CHANNEL_MAX];
		// float SGx[CHANNEL_MAX];
		// uint32_t Finish_Flag;
	} DAC_Calibration_HandleTypeDef __attribute__((aligned(4)));

	extern ADC_Calibration_HandleTypeDef Adc;
	extern DAC_Calibration_HandleTypeDef Dac;
	extern bool Dac_Clibration(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
