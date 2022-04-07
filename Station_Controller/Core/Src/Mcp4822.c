/*
 * MCP48xx.c
 *
 *  Created on: Jan 11, 2021
 *      Author: LHC
 */
#include "Mcp4822.h"
#include "spi.h"
#include "dac.h"
#include "usart.h"

/*定义DAC输出对象*/
// Dac_Obj dac_object;

float gDac_OutPrarm[4U][2U] = {
	{200.61237F, -1.50459F},
	{199.14890F, 10.45532F},
	{204.49438F, -19.42697F},
	{202.72280F, -20.27230F},
};

/**
 * @brief	对MCP48xx写数据
 * @details
 * @param	data:写入的数据
 * @param   p_ch :当前充电通道指针
 * @retval	None
 */
void Mcp48xx_Write(Dac_Obj *p_ch, uint16_t data)
{
	void *pHandle = NULL;

	pHandle = p_ch->Channel < DAC_OUT3 ? (void *)&hspi1 : (void *)&hdac;

	if (pHandle)
	{
		switch (p_ch->Channel)
		{
		case DAC_OUT1:
		case DAC_OUT2:
		{

			data &= 0x0FFF;
			/*选择mcp4822通道*/
			data |= (p_ch->Mcpxx_Id ? INPUT_B : INPUT_A);
			/*软件拉低CS信号*/
			HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
			/*调用硬件spi发送函数*/
			HAL_SPI_Transmit(pHandle, (uint8_t *)&data, sizeof(data), SPI_TIMEOUT);
			/*软件拉高CS信号*/
			HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
		}
		break;
		case DAC_OUT3:
		case DAC_OUT4:
		{
			uint32_t channel = (p_ch->Channel > DAC_OUT3) ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
			HAL_DAC_SetValue((DAC_HandleTypeDef *)pHandle, channel, DAC_ALIGN_12B_R, (data & 0x0FFF));
		}
		break;
		default:
			break;
		}
	}
#if defined(USING_DEBUG)
	// shellPrint(&shell,"AD[%d] = 0x%d\r\n", 0, Get_AdcValue(ADC_CHANNEL_1));
	// Usart1_Printf("cs1 = %d, pGPIOx = %d\r\n", CS1_GPIO_Port, pGPIOx);
#endif
}

/**
 * @brief	对目标通道输出电流
 * @details
 * @param   p_ch :目标通道
 * @param	data:写入的数据
 * @retval	None
 */
void Output_Current(Dac_Obj *p_ch, float data)
{
	/*浮点数电流值转换为uint16_t*/
	uint16_t value = (uint16_t)(gDac_OutPrarm[p_ch->Channel][0] * data + gDac_OutPrarm[p_ch->Channel][1]);

	value = data ? ((data >= 20.0F) ? 0xFFFF : value) : 0U;
	Mcp48xx_Write(p_ch, value);
}
