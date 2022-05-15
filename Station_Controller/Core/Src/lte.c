#include "lte.h"
#include "usart.h"
#include "shell_port.h"
#if defined(USING_FREERTOS)
#include "cmsis_os.h"
#endif

/*定义AT模块对象*/
pAtHandle At_Object;

/*局部函数申明*/
static void Free_AtObject(pAtHandle *pa);
static void At_SetPin(pAtHandle pa, Gpiox_info *pgpio, GPIO_PinState PinState);
static void At_SetDefault(pAtHandle pa);
static bool At_ExeAppointCmd(pAtHandle pa, AT_Command *pat);

/*4G模块AT指令列表*/
AT_Command Lte[] = {

	{"+++", "a", Get_Ms(2)},
	{"a", "+ok", Get_Ms(2)},
	// {"AT+E=OFF\r\n", "OK", Get_Ms(0.2F)},

	{"AT+HEARTDT=7777772E796E7061782E636F6D\r\n", "OK", Get_Ms(0.2F)},

	{"AT+WKMOD=NET\r\n", "OK", Get_Ms(0.2F)},
	{"AT+SOCKAEN=ON\r\n", "OK", Get_Ms(0.2F)},
	{"AT+SOCKASL=LONG\r\n", "OK", Get_Ms(0.2F)},
	{"AT+SOCKA=TCP,clouddata.usr.cn,15000\r\n", "OK", Get_Ms(0.2F)},

	{"AT+REGEN=ON\r\n", "OK", Get_Ms(0.2F)},
	{"AT+REGTP=CLOUD\r\n", "OK", Get_Ms(0.2F)},
	{CLOUD_ID, "OK", Get_Ms(0.2F)},
	{"AT+UART=" UART_PARAM "\r\n", "OK", Get_Ms(0.2F)},
	{"AT+UARTFL=" PACKAGE_SIZE "\r\n", "OK", Get_Ms(0.2F)},
	{"AT+Z\r\n", "OK", Get_Ms(0.2F)},
};
#define AT_LIST_SIZE (sizeof(Lte) / sizeof(AT_Command))

/**
 * @brief	创建AT模块对象
 * @details
 * @param	pa 需要初始化对象指针
 * @param   ps 初始化数据指针
 * @retval	None
 */
static void Creat_AtObject(pAtHandle *pa, pAtHandle ps)
{
	if (!ps)
		return;
#if defined(USING_FREERTOS)
	(*pa) = (pAtHandle)CUSTOM_MALLOC(sizeof(AtHandle));
	if (!(*pa))
		CUSTOM_FREE(*pa);
#else

#endif

#if defined(USING_DEBUG)
	shellPrint(Shell_Object, "At[%d]_handler = 0x%p\r\n", ps->Id, *pa);
#endif
	(*pa)->Id = ps->Id;
	(*pa)->Table = ps->Table;
	(*pa)->Gpio = ps->Gpio;
	(*pa)->Free_AtObject = Free_AtObject;
	(*pa)->AT_SetPin = At_SetPin;
	(*pa)->AT_SetDefault = At_SetDefault;
	(*pa)->AT_ExeAppointCmd = At_ExeAppointCmd;
}

/**
 * @brief	销毁AT模块对象
 * @details
 * @param	pa 对象指针
 * @retval	None
 */
static void Free_AtObject(pAtHandle *pa)
{
	if (*pa)
	{
		CUSTOM_FREE((*pa));
	}
}

/**
 * @brief	销毁AT模块对象
 * @details
 * @param	None
 * @retval	None
 */
void MX_AtInit(void)
{
	At_Gpio gpio = {
		.Reload = {.pGPIOx = RESET_4G_GPIO_Port, .Gpio_Pin = RESET_4G_Pin},
		.Reset = {.pGPIOx = RELOAD_4G_GPIO_Port, .Gpio_Pin = RELOAD_4G_Pin},
	};
	AT_Table table = {
		.pList = Lte,
		.Comm_Num = AT_LIST_SIZE,
	};
	extern UART_HandleTypeDef huart3;
	AtHandle at = {
		.Id = 0x00,
		.Gpio = gpio,
		.Table = table,
		.huart = &huart3,
	};
	Creat_AtObject(&At_Object, &at);
}

/**
 * @brief	AT模块的reset、reload
 * @details
 * @param	None
 * @retval	None
 */
static void At_SetPin(pAtHandle pa, Gpiox_info *pgpio, GPIO_PinState PinState)
{
	if (pgpio && (pgpio->pGPIOx))
		HAL_GPIO_WritePin(pgpio->pGPIOx, pgpio->Gpio_Pin, PinState);
}

/**
 * @brief	AT模块初始化默认参数
 * @details
 * @param	None
 * @retval	None
 */
static void At_SetDefault(pAtHandle pa)
{
	if (pa && (pa->Table.pList) && (pa->huart))
	{
		for (AT_Command *pat = pa->Table.pList; pat < pa->Table.pList + pa->Table.Comm_Num; pat++)
		{
			if (!pa->AT_ExeAppointCmd(pa, pat))
			{
				break;
			}
		}
	}
}

/**
 * @brief	AT模块执行指定指令
 * @details
 * @param	None
 * @retval	None
 */
static bool At_ExeAppointCmd(pAtHandle pa, AT_Command *pat)
{
#define RETRY_COUNTS 3U
#define AT_CMD_ERROR "ERR"
	uint8_t counts = 0;
	bool result = true;
	if (pa && pat)
	{
		uint8_t rx_size = strlen(pat->pRecv);
		uint8_t *prdata = (uint8_t *)CUSTOM_MALLOC(rx_size);
		if (prdata && rx_size)
		{
			while (result)
			{
				HAL_UART_Transmit(pa->huart, (uint8_t *)pat->pSend, strlen(pat->pSend), pat->WaitTimes);
				if (HAL_UART_Receive(pa->huart, prdata, rx_size, pa->Table.pList->WaitTimes) == HAL_OK)
				{
					if (strstr((const char *)prdata, (const char *)pat->pSend) == NULL)
					{
						counts++;
#if defined(USING_DEBUG)
						shellPrint(Shell_Object, "Response instruction:%s and %s mismatch.\r\n",
								   prdata, pat->pSend);
#endif
					}
					else
					{
#if defined(USING_DEBUG)
						shellPrint(Shell_Object, "Command sent successfully.\r\n");
#endif
						break;
					}
				}
				else
				{
					counts++;
#if defined(USING_DEBUG)
					shellPrint(Shell_Object, "At module does not respond!\r\n");
#endif
				}
				if (counts >= RETRY_COUNTS)
				{
					result = false;
#if defined(USING_DEBUG)
					shellPrint(Shell_Object, "Retransmission exceeds the maximum number of times!\r\n");
#endif
				}
			}
		}
		CUSTOM_FREE(prdata);
	}
	return result;
}
