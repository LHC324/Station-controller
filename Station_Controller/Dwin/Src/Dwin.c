/*
 * Dwin.c
 *
 *  Created on: 2022年3月25日
 *      Author: LHC
 */

#include "dwin.h"
#include "tool.h"
#include "usart.h"
#include "shell_port.h"
#include "Flash.h"
#include "mdrtuslave.h"
#if defined(USING_FREERTOS)
#include "cmsis_os.h"
#endif

/*定义迪文屏幕对象*/
pDwinHandle Dwin_Object;

extern Save_Param Save_InitPara;
extern void Report_Backparam(pDwinHandle pd, Save_Param *sp);

static void Dwin_Send(pDwinHandle pd);
static void Dwin_Write(pDwinHandle pd, uint16_t start_addr, uint8_t *dat, uint16_t len);
static void Dwin_Read(pDwinHandle pd, uint16_t start_addr, uint8_t words);
static void Dwin_PageChange(pDwinHandle pd, uint16_t page);
static void Dwin_Poll(pDwinHandle pd);
// void Dwin_Poll(pDwinHandle pd);
static void Dwin_ErrorHandle(pDwinHandle pd, uint8_t error_code, uint8_t site);
static void Dwin_EventHandle(pDwinHandle pd, uint8_t *pSite);
static void Restore_Factory(pDwinHandle pd, uint8_t *pSite);
static void Password_Handle(pDwinHandle pd, uint8_t *pSite);

/*迪文响应线程*/
DwinMap Dwin_ObjMap[] = {
	// {.addr = PARAM_SETTING_ADDR, .upper = 4.0F, .lower = 0, .pHandle = &Save_Flash.Param.Ptank_max, .event = Dwin_EventHandle},
	{.addr = PTANK_MAX_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PTANK_MIN_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PVAOUT_MAX_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PVAOUT_MIN_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PGAS_MAX_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PGAS_MIN_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = LTANK_MAX_ADDR, .upper = 50.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = LTANK_MIN_ADDR, .upper = 50.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PTOLE_MAX_ADDR, .upper = 0.5F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PTOLE_MIN_ADDR, .upper = 0.5F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = LEVEL_MAX_ADDR, .upper = 2.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = LEVEL_MIN_ADDR, .upper = 2.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = SPSFS_MAX_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = SPSFS_MIN_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PSVA_LIMIT_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PBACK_MAX_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PBACK_MIN_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PEVA_LIMIT_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = SPSFE_MAX_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = SPSFE_MIN_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PTANK_LIMIT_ADDR, .upper = 2.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = LTANK_LIMIT_ADDR, .upper = 10.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PVAP_STOP_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = PSVAP_STOP_ADDR, .upper = 4.0F, .lower = 0, .event = Dwin_EventHandle},
	{.addr = RESTORE_ADDR, .upper = 0xFFFF, .lower = 0, .event = Restore_Factory},
	{.addr = USER_NAME_ADDR, .upper = 9999, .lower = 0, .event = Password_Handle},
	{.addr = USER_PASSWORD_ADDR, .upper = 9999, .lower = 0, .event = Password_Handle},
	{.addr = LOGIN_SURE_ADDR, .upper = 0xFFFF, .lower = 0, .event = Password_Handle},
	{.addr = CANCEL_ADDR, .upper = 0xFFFF, .lower = 0, .event = Password_Handle},
};

#define Dwin_EventSize (sizeof(Dwin_ObjMap) / sizeof(DwinMap))

static void Dwin_Write(pDwinHandle pd, uint16_t start_addr, uint8_t *dat, uint16_t len);

/**
 * @brief  创建迪文屏幕对象
 * @param  pd 需要初始化对象指针
 * @param  ps 初始化数据指针
 * @retval None
 */
static void Create_DwinObject(pDwinHandle *pd, pDwinHandle ps)
{
	if (!ps)
		return;
#if defined(USING_FREERTOS)
	(*pd) = (pDwinHandle)CUSTOM_MALLOC(sizeof(DwinHandle));
	if (!(*pd))
		CUSTOM_FREE(*pd);
	uint8_t *pTxbuf = (uint8_t *)CUSTOM_MALLOC(ps->Master.TxSize);
	if (!pTxbuf)
		CUSTOM_FREE(pTxbuf);
	uint8_t *pRxbuf = (uint8_t *)CUSTOM_MALLOC(ps->Slave.RxSize);
	if (!pRxbuf)
		CUSTOM_FREE(pRxbuf);
#else
	uint8_t pTxbuf[ps->Master.TxSize];
	uint8_t pRxbuf[ps->Slave.RxSize];
#endif
	memset(pTxbuf, 0x00, ps->Master.TxSize);
	memset(pRxbuf, 0x00, ps->Slave.RxSize);
#if defined(USING_DEBUG)
	shellPrint(Shell_Object, "Dwin[%d]_handler = 0x%p\r\n", ps->Id, *pd);
#endif

	(*pd)->Id = ps->Id;
	(*pd)->Dw_Transmit = Dwin_Send;
	(*pd)->Dw_Write = Dwin_Write;
	(*pd)->Dw_Read = Dwin_Read;
	(*pd)->Dw_Page = Dwin_PageChange;
	(*pd)->Dw_Poll = Dwin_Poll;
	(*pd)->Dw_Handle = Dwin_EventHandle;
	(*pd)->Dw_Error = Dwin_ErrorHandle;
	(*pd)->Master.pTbuf = pTxbuf;
	(*pd)->Master.TxCount = 0U;
	(*pd)->Master.TxSize = ps->Master.TxSize;
	(*pd)->Slave.pRbuf = pRxbuf;
	(*pd)->Slave.RxSize = ps->Slave.RxSize;
	(*pd)->Slave.RxCount = 0U;
	(*pd)->Slave.pMap = ps->Slave.pMap;
	(*pd)->Slave.Events_Size = ps->Slave.Events_Size;
	// (*pd)->Slave.pMap->pHandle = ps->Slave.pMap->pHandle;
	(*pd)->Slave.pHandle = ps->Slave.pHandle;
	(*pd)->huart = ps->huart;
}

/**
 * @brief  销毁迪文屏幕对象
 * @param  pd 需要初始化对象指针
 * @retval None
 */
void Free_DwinObject(pDwinHandle *pd)
{
	if (*pd)
	{
		CUSTOM_FREE((*pd)->Master.pTbuf);
		CUSTOM_FREE((*pd)->Slave.pRbuf);
		CUSTOM_FREE((*pd));
	}
}

/**
 * @brief  带CRC的发送数据帧
 * @param  _pBuf 数据缓冲区指针
 * @param  _ucLen 数据长度
 * @retval None
 */
void MX_DwinInit()
{
	extern Save_HandleTypeDef Save_Flash;
	DwinHandle Dwin;

	Dwin.Id = 0x00;
	Dwin.Master.TxSize = TX_BUF_SIZE;
	Dwin.Slave.RxSize = RX_BUF_SIZE;
	Dwin.Slave.pMap = Dwin_ObjMap;
	Dwin.Slave.Events_Size = Dwin_EventSize;
	Dwin.Slave.pHandle = &Save_Flash.Param;
	/*定义迪文屏幕使用目标串口*/
	Dwin.huart = &huart1;
/*使用屏幕接收处理时*/
#define TYPEDEF_STRUCT float
	Create_DwinObject(&Dwin_Object, &Dwin);
}

/**
 * @brief  带CRC的发送数据帧
 * @param  _pBuf 数据缓冲区指针
 * @param  _ucLen 数据长度
 * @retval None
 */
static void Dwin_Send(pDwinHandle pd)
{
#if (USING_CRC == 1U)
	uint16_t crc16 = Get_Crc16(&pd->Master.pTbuf[3U], pd->Master.TxCount - 3U, 0xffff);

	memcpy(&pd->Master.pTbuf[pd->Master.TxCount], (uint8_t *)&crc16, sizeof(crc16));
	pd->Master.TxCount += sizeof(crc16);
#endif

#if defined(USING_DMA)
#if defined(USING_FREERTOS)
	// uint8_t *pbuf = (uint8_t *)CUSTOM_MALLOC(pd->Master.TxCount);
	// if (!pbuf)
	// 	CUSTOM_FREE(pbuf);
#endif
	// memset(pbuf, 0x00, pd->Master.TxCount);
	// memcpy(pbuf, pd->Master.pTbuf, pd->Master.TxCount);
	HAL_UART_Transmit_DMA(pd->huart, pd->Master.pTbuf, pd->Master.TxCount);
	/*https://blog.csdn.net/mickey35/article/details/80186124*/
	/*https://blog.csdn.net/qq_40452910/article/details/80022619*/
	while (__HAL_UART_GET_FLAG(pd->huart, UART_FLAG_TC) == RESET)
	{
		// osDelay(1);
	}
#else
	HAL_UART_Transmit(pd->huart, pd->Master.pTbuf, pd->Master.TxCount, 0xffff);
#endif
#if defined(USING_FREERTOS)
	// CUSTOM_FREE(pbuf);
#endif
}

/**
 * @brief  写数据变量到指定地址并显示
 * @param  pd 迪文屏幕对象句柄
 * @param  start_addr 开始地址
 * @param  dat 指向数据的指针
 * @param  length 数据长度
 * @retval None
 */
static void Dwin_Write(pDwinHandle pd, uint16_t start_addr, uint8_t *pdat, uint16_t len)
{
#if (USING_CRC)
	uint8_t buf[] = {
		0x5A, 0xA5, len + 3U + 2U, WRITE_CMD, start_addr >> 8U,
		start_addr};
#else
	uint8_t buf[] = {
		0x5A, 0xA5, len + 3U, WRITE_CMD, start_addr >> 8U,
		start_addr};
#endif
	pd->Master.TxCount = 0U;
	memcpy(pd->Master.pTbuf, buf, sizeof(buf));
	pd->Master.TxCount += sizeof(buf);
	memcpy(&pd->Master.pTbuf[pd->Master.TxCount], pdat, len);
	pd->Master.TxCount += len;
#if defined(USING_DEBUG)
	// shellPrint(Shell_Object, "pd = %p, pd->Master.TxCount = %d.\r\n", pd, pd->Master.TxCount);
	// shellPrint(Shell_Object, "pd->Master.pTbuf = %s.\r\n", pd->Master.pTbuf);
#endif

	pd->Dw_Transmit(pd);
}

/**
 * @brief  读出指定地址指定长度数据
 * @param  pd 迪文屏幕对象句柄
 * @param  start_addr 开始地址
 * @param  dat 指向数据的指针
 * @param  length 数据长度
 * @retval None
 */
static void Dwin_Read(pDwinHandle pd, uint16_t start_addr, uint8_t words)
{
#if (USING_CRC)
	uint8_t buf[] = {
		0x5A, 0xA5, 0x04 + 2U, READ_CMD, start_addr >> 8U,
		words};
#else
	uint8_t buf[] = {
		0x5A, 0xA5, 0x04, READ_CMD, start_addr >> 8U,
		words};
#endif
	pd->Master.TxCount = 0U;
	memcpy(pd->Master.pTbuf, buf, sizeof(buf));
	pd->Master.TxCount += sizeof(buf);

	// Dwin_Send(pd);
	pd->Dw_Transmit(pd);
}

// void Dwin_Read(uint16_t start_addr, uint16_t words)
// {
// 	g_Dwin.TxCount = 0;
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = 0x5A;
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = 0xA5;
// #if (USING_CRC)
// 	/*Add two bytes CRC*/
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = 0x04 + 2U;
// #else
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = 0x04;
// #endif
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = READ_CMD;
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = start_addr >> 8;
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = start_addr;
// 	g_Dwin.TxBuf[g_Dwin.TxCount++] = words;

// #if (USING_CRC)
// 	Dwin_SendWithCRC(g_Dwin.TxBuf, g_Dwin.TxCount);
// #else
// 	Dwin_Send(g_Dwin.TxBuf, g_Dwin.TxCount);
// #endif
// }

/**
 * @brief  迪文屏幕指定页面切换
 * @param  pd 迪文屏幕对象句柄
 * @param  page 目标页面
 * @retval None
 */
static void Dwin_PageChange(pDwinHandle pd, uint16_t page)
{
#if (USING_CRC)
	uint8_t buf[] = {
		0x5A, 0xA5, 0x07 + 2U, WRITE_CMD, 0x00, 0x84, 0x5A, 0x01,
		page >> 8U, page};
#else
	uint8_t buf[] = {
		0x5A, 0xA5, 0x07, WRITE_CMD, 0x00, 0x84, 0x5A, 0x01,
		page >> 8U, page};
#endif
	pd->Master.TxCount = 0U;
	memcpy(pd->Master.pTbuf, buf, sizeof(buf));
	pd->Master.TxCount += sizeof(buf);

	pd->Dw_Transmit(pd);
}

/*83指令返回数据以一个字为基础*/
#define DW_WORD 1U
#define DW_DWORD 2U

/*获取迪文屏幕地址*/
// #define Get_Addr(__ptr, __s1, __s2, __size) \
// 	((__size) > 1U ? (((__ptr)->Slave.pRbuf[__s1] << 8U) | ((__ptr)->Slave.pRbuf[__s2])) : ((__ptr)->Slave.pRbuf[__s1]))
/*获取迪文屏幕数据*/
#define Get_Data(__ptr, __s, __size) \
	((__size) < 2U ? (((__ptr)->Slave.pRbuf[__s] << 8U) | ((__ptr)->Slave.pRbuf[__s + 1U])) : (((__ptr)->Slave.pRbuf[__s] << 24U) | ((__ptr)->Slave.pRbuf[__s + 1U] << 16U) | ((__ptr)->Slave.pRbuf[__s + 2U] << 8U) | ((__ptr)->Slave.pRbuf[__s + 3U])))

/**
 * @brief  迪文屏幕接收数据解析
 * @param  pd 迪文屏幕对象句柄
 * @retval None
 */
static void Dwin_Poll(pDwinHandle pd)
// void Dwin_Poll(pDwinHandle pd)
{ /*检查帧头是否符合要求*/
	if ((pd->Slave.pRbuf[0] == 0x5A) && (pd->Slave.pRbuf[1] == 0xA5))
	{
		uint16_t addr = Get_Data(pd, 4U, DW_WORD);
#if defined(USING_DEBUG)
		// shellPrint(Shell_Object, "addr = 0x%x\r\n", addr);
#endif
		for (uint8_t i = 0; i < pd->Slave.Events_Size; i++)
		{
			if (pd->Slave.pMap[i].addr == addr)
			{
				if (pd->Slave.pMap[i].event)
					pd->Slave.pMap[i].event(pd, &i);
				break;
			}
		}
	}
#if defined(USING_DEBUG)
	for (uint16_t i = 0; i < pd->Slave.RxCount; i++)
	// shellPrint(Shell_Object, "pRbuf[%d] = 0x%x\r\n", i, pd->Slave.pRbuf[i]);
#endif
		memset(pd->Slave.pRbuf, 0x00, pd->Slave.RxCount);
	pd->Slave.RxCount = 0U;
}

/**
 * @brief  迪文屏幕接收数据处理
 * @param  pd 迪文屏幕对象句柄
 * @param  pSite 记录当前Map中位置
 * @retval None
 */
static void Dwin_EventHandle(pDwinHandle pd, uint8_t *pSite)
{
#define INPUT_PRECISION 10.0F
	TYPEDEF_STRUCT data = (TYPEDEF_STRUCT)Get_Data(pd, 7U, pd->Slave.pRbuf[6U]) / INPUT_PRECISION;
	// TYPEDEF_STRUCT *pdata = (TYPEDEF_STRUCT *)pd->Slave.pMap[*pSite].pHandle;
	TYPEDEF_STRUCT *pdata = (TYPEDEF_STRUCT *)pd->Slave.pHandle;
	Save_HandleTypeDef *ps = &Save_Flash;

#if defined(USING_DEBUG)
	shellPrint(Shell_Object, "data = %.3f, *psite = %d.\r\n", data, *pSite);
#endif
	if ((data >= pd->Slave.pMap[*pSite].lower) && (data <= pd->Slave.pMap[*pSite].upper))
	{
		if ((pdata) && (*pSite < pd->Slave.Events_Size))
			pdata[*pSite] = data;
#if defined(USING_FREERTOS)
		taskENTER_CRITICAL();
#endif
		/*计算crc校验码*/
		ps->Param.crc16 = Get_Crc16((uint8_t *)&ps->Param, sizeof(Save_Param) - sizeof(ps->Param.crc16), 0xFFFF);
		/*参数保存到Flash*/
		FLASH_Write(PARAM_SAVE_ADDRESS, (uint16_t *)&Save_Flash.Param, sizeof(Save_Param));
#if defined(USING_FREERTOS)
		taskEXIT_CRITICAL();
#endif
		Endian_Swap((uint8_t *)&data, 0U, sizeof(TYPEDEF_STRUCT));
		/*数据写回保持寄存器区*/
		mdSTATUS ret = mdRTU_WriteHoldRegs(Slave1_Object, PARAM_MD_ADDR + (*pSite) * 2U, 2U, (mdU16 *)&data);
		if (ret == mdFALSE)
		{
#if defined(USING_DEBUG)
			shellPrint(Shell_Object, "Holding register addr[0x%x], Write: %.3f failed!\r\n", PARAM_MD_ADDR + (*pSite) * 2U, data);
#endif
		}
		/*确认数据回传到屏幕*/
		pd->Dw_Write(pd, pd->Slave.pMap[*pSite].addr, (uint8_t *)&data, sizeof(TYPEDEF_STRUCT));
#if defined(USING_DEBUG)
		shellPrint(Shell_Object, "pdata[%d] = %.3f,Ptank_max = %.3f.\r\n", *pSite, pdata[*pSite], Save_Flash.Param.Ptank_max);
#endif
	}
	else
	{
#define BELOW_LOWER_LIMIT 1U
#define ABOVE_UPPER_LIMIT 2U

		uint8_t error = data < pd->Slave.pMap[*pSite].lower ? BELOW_LOWER_LIMIT : ABOVE_UPPER_LIMIT;
		/*屏幕传回参数越界处理：维持原值不变、或者切换报错页面*/
		pd->Dw_Error(pd, error, *pSite);
	}
}

/**
 * @brief  迪文屏幕错误处理
 * @param  pd 迪文屏幕对象句柄
 * @param  error_code 错误代码
 * @param  site 当前对象出错位置
 * @retval None
 */
static void Dwin_ErrorHandle(pDwinHandle pd, uint8_t error_code, uint8_t site)
{
	TYPEDEF_STRUCT tdata = (error_code == BELOW_LOWER_LIMIT) ? pd->Slave.pMap[site].lower : pd->Slave.pMap[site].upper;
#if defined(USING_DEBUG)
	if (error_code == BELOW_LOWER_LIMIT)
	{
		shellPrint(Shell_Object, "Error: Below lower limit %.3f.\r\n", tdata);
	}
	else
	{
		shellPrint(Shell_Object, "Error: Above upper limit %.3f.\r\n", tdata);
	}
#endif
	Endian_Swap((uint8_t *)&tdata, 0U, sizeof(TYPEDEF_STRUCT));
	/*设置错误时将显示上下限*/
	pd->Dw_Write(pd, pd->Slave.pMap[site].addr, (uint8_t *)&tdata, sizeof(TYPEDEF_STRUCT));
}

/**
 * @brief  系统参数恢复出厂设置
 * @param  pd 迪文屏幕对象句柄
 * @param  pSite 记录当前Map中位置
 * @retval None
 */
static void Restore_Factory(pDwinHandle pd, uint8_t *pSite)
{
	uint16_t rcode = Get_Data(pd, 7U, pd->Slave.pRbuf[6U]);
	Save_HandleTypeDef *ps = &Save_Flash;
	if (rcode == RSURE_CODE)
	{
		memcpy(&ps->Param, &Save_InitPara, sizeof(Save_Param));
#if defined(USING_FREERTOS)
		taskENTER_CRITICAL();
#endif
		FLASH_Write(PARAM_SAVE_ADDRESS, (uint16_t *)&Save_InitPara, sizeof(Save_Param));
#if defined(USING_FREERTOS)
		taskEXIT_CRITICAL();
#endif
		extern void Param_WriteBack(Save_HandleTypeDef * ps);
		/*更新屏幕同时，更新远程参数*/
		Param_WriteBack(ps);
		Report_Backparam(Dwin_Object, &ps->Param);
#if defined(USING_DEBUG)
		shellPrint(Shell_Object, "success: Factory settings restored successfully!\r\n");
#endif
	}
}

/**
 * @brief  密码处理
 * @param  pd 迪文屏幕对象句柄
 * @param  pSite 记录当前Map中位置
 * @retval None
 */
static void Password_Handle(pDwinHandle pd, uint8_t *pSite)
{
#define USER_NAMES 0x07E6
#define USER_PASSWORD 0x0522
#define PAGE_NUMBER 0x08
	uint16_t data = Get_Data(pd, 7U, pd->Slave.pRbuf[6U]);
	uint16_t addr = pd->Slave.pMap[*pSite].addr;
	static uint16_t user_name = 0x0000, user_code = 0x0000, error = 0x0000;
	Save_HandleTypeDef *ps = &Save_Flash;
	uint16_t default_name = ps->Param.User_Name, defalut_code = ps->Param.User_Code;

	if ((data >= pd->Slave.pMap[*pSite].lower) && (data <= pd->Slave.pMap[*pSite].upper))
	{
		addr == USER_NAME_ADDR ? user_name = data : (addr == USER_PASSWORD_ADDR ? user_code = data : 0U);
		if ((addr == LOGIN_SURE_ADDR) && (data == RSURE_CODE))
		{ /*密码用户名正确*/
			if ((user_name == default_name) && (user_code == defalut_code))
			{ /*清除错误信息*/
				error = 0x0000;
				pd->Dw_Page(pd, PAGE_NUMBER);
#if defined(USING_DEBUG)
				shellPrint(Shell_Object, "success: The password is correct!\r\n");
#endif
			}
			else
			{
				/*用户名、密码错误*/
				if ((user_name != default_name) && (user_code != defalut_code))
				{
					error = 0x0300;
#if defined(USING_DEBUG)
					shellPrint(Shell_Object, "error: Wrong user name and password!\r\n");
#endif
				}
				/*用户名错误*/
				else if (user_name != default_name)
				{
					error = 0x0100;
#if defined(USING_DEBUG)
					shellPrint(Shell_Object, "error: User name error!\r\n");
#endif
				}
				/*密码错误*/
				else
				{
					error = 0x0200;

#if defined(USING_DEBUG)
					shellPrint(Shell_Object, "error: User password error!\r\n");
#endif
				}
			}
		}
		if ((addr == CANCEL_ADDR) && (data == RCANCEL_CODE))
		{
			error = 0x0000;
			user_name = user_code = 0x0000;
			uint32_t temp_value = 0x0000;
			pd->Dw_Write(pd, USER_NAME_ADDR, (uint8_t *)&temp_value, sizeof(temp_value));
			// pd->Dw_Write(pd, ERROR_NOTE_ADDR, (uint8_t *)&error, sizeof(error));
#if defined(USING_DEBUG)
			shellPrint(Shell_Object, "success: Clear Error Icon!\r\n");
#endif
		}
		pd->Dw_Write(pd, ERROR_NOTE_ADDR, (uint8_t *)&error, sizeof(error));
	}

#if defined(USING_DEBUG)
	shellPrint(Shell_Object, "data = %d,user_name = %d, user_code = %d\r\n", data, user_name, user_code);
#endif
}
