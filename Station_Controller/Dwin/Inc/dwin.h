/*
 * Dwin.h
 *
 *  Created on: Nov 19, 2020
 *      Author: play
 */

#ifndef INC_DWIN_H_
#define INC_DWIN_H_

#include "main.h"

/*迪文屏幕带CRC校验*/
#define USING_CRC 1

#define RX_BUF_SIZE 128
#define TX_BUF_SIZE 128

#define WRITE_CMD 0x82		 //写
#define READ_CMD 0x83		 //读
#define PAGE_CHANGE_CMD 0x84 //页面切换
#define TOUCH_CMD 0xD4		 //触摸动作

#define USER_NAME_ADDR 0x1000	  //用户名地址
#define USER_PASSWORD_ADDR 0x1001 //用户密码
#define LOGIN_SURE_ADDR 0x1002	  //登录确认地址
// #define HELP_ADDR 0x1003			//帮助按钮地址
#define CANCEL_ADDR 0x1003			//注销地址
#define ERROR_NOTE_ADDR 0x1028		//错误图标提示地址
#define DIGITAL_INPUT_ADDR 0x1004	//数字量输入地址
#define DIGITAL_OUTPUT_ADDR 0x1005	//数字量1输出地址
#define DIGITAL_OUTPUT1_ADDR 0x1005 //数字量1输出地址
// #define DIGITAL_OUTPUT2_ADDR 0x1006 //数字量2输出地址
// #define DIGITAL_OUTPUT3_ADDR 0x1007 //数字量3输出地址
// #define DIGITAL_OUTPUT4_ADDR 0x1008 //数字量4输出地址
// #define DIGITAL_OUTPUT5_ADDR 0x1009 //数字量5输出地址
// #define DIGITAL_OUTPUT6_ADDR 0x100A //数字量6输出地址
// #define DIGITAL_OUTPUT7_ADDR 0x100B //数字量7输出地址
// #define DIGITAL_OUTPUT8_ADDR 0x100C //数字量8输出地址

#define ANALOG_INPUT1_ADDR 0x100E //模拟量1输入地址
#define ANALOG_INPUT2_ADDR 0x1010 //模拟量2输入地址
#define ANALOG_INPUT3_ADDR 0x1012 //模拟量3输入地址
#define ANALOG_INPUT4_ADDR 0x1014 //模拟量4输入地址
#define ANALOG_INPUT5_ADDR 0x1016 //模拟量5输入地址
#define ANALOG_INPUT6_ADDR 0x1018 //模拟量6输入地址
#define ANALOG_INPUT7_ADDR 0x101A //模拟量7输入地址
#define ANALOG_INPUT8_ADDR 0x101C //模拟量8输入地址

#define ANALOG_OUTPUT1_ADDR 0x101E //模拟量1输出地址
#define ANALOG_OUTPUT2_ADDR 0x1020 //模拟量2输出地址
#define ANALOG_OUTPUT3_ADDR 0x1022 //模拟量3输出地址
#define ANALOG_OUTPUT4_ADDR 0x1024 //模拟量4输出地址
/*参数存储区*/
#define PRESSURE_OUT_ADDR 0x1006  //压力输出地址
#define ANALOG_INPUT_ADDR 0x100E  //模拟量1输入地址
#define ANALOG_OUTPUT_ADDR 0x1020 //模拟量1输出地址
#define PARAM_SETTING_ADDR 0x1030 //迪文屏幕后台参数设定地址
#define PTANK_MAX_ADDR 0x1030	  //储槽压力上限地址
#define PTANK_MIN_ADDR 0x1032	  //储槽压力下限地址
#define PVAOUT_MAX_ADDR 0x1034	  //汽化器出口压力上限地址
#define PVAOUT_MIN_ADDR 0x1036	  //汽化器出口压力下限地址
#define PGAS_MAX_ADDR 0x1038	  //气站出口压力上限地址
#define PGAS_MIN_ADDR 0x103A	  //气站出口压力下限地址
#define LTANK_MAX_ADDR 0x103C	  //储槽液位上限地址
#define LTANK_MIN_ADDR 0x103E	  //储槽液位下限地址
#define PTOLE_MAX_ADDR 0x1040	  //压力容差上限地址
#define PTOLE_MIN_ADDR 0x1042	  //压力容差下限地址
#define LEVEL_MAX_ADDR 0x1044	  //液位容差上限地址
#define LEVEL_MIN_ADDR 0x1046	  //液位容差下限地址
#define SPSFS_MAX_ADDR 0x1048	  //启动模式时储槽泄压启动值上限地址
#define SPSFS_MIN_ADDR 0x104A	  //启动模式时储槽泄压启动值下限地址
#define PSVA_LIMIT_ADDR 0x104C	  //启动模式时汽化器出口压力泄压启动地址
#define PBACK_MAX_ADDR 0x104E	  // B2-B1回压差上限地址
#define PBACK_MIN_ADDR 0x1050	  //储槽回压差下限地址
#define PEVA_LIMIT_ADDR 0x1052	  //停机模式时汽化器出口压力泄压启动地址
#define SPSFE_MAX_ADDR 0x1054	  //停机模式时储槽泄压启动值上限地址
#define SPSFE_MIN_ADDR 0x1056	  //停机模式时储槽泄压启动值下限地址
#define PTANK_LIMIT_ADDR 0x1058	  //安全策略储槽压力极限
#define LTANK_LIMIT_ADDR 0x105A	  //安全策略储槽液位极限
#define PVAP_STOP_ADDR 0x105C	  //停机模式汽化器出口压力停止值
#define PSVAP_STOP_ADDR 0x105E	  //启动模式汽化器出口压力停止值
#define RESTORE_ADDR 0x1060		  //恢复出厂设置地址
#define ERROR_CODE_ADDR 0x1062	  //错误代码地址
#define ERROR_ANMATION 0x1063	  //错误动画地址
#define RSURE_CODE 0x00F1		  //恢复出厂设置确认键值
#define RCANCEL_CODE 0x00F0		  //注销键值

typedef struct Dwin_HandleTypeDef *pDwinHandle;
typedef struct Dwin_HandleTypeDef DwinHandle;
typedef void (*pfunc)(pDwinHandle, uint8_t *);
typedef struct
{
	uint32_t addr;
	float upper;
	float lower;
	/*预留外部数据结构接口*/
	// void *pHandle;
	pfunc event;
} DwinMap;

struct Dwin_HandleTypeDef
{
	uint8_t Id;
	void (*Dw_Transmit)(pDwinHandle);
	void (*Dw_Write)(pDwinHandle, uint16_t, uint8_t *, uint16_t);
	void (*Dw_Read)(pDwinHandle, uint16_t, uint8_t);
	void (*Dw_Page)(pDwinHandle, uint16_t);
	void (*Dw_Poll)(pDwinHandle);
	void (*Dw_Handle)(pDwinHandle, uint8_t *);
	void (*Dw_Error)(pDwinHandle, uint8_t, uint8_t);
	struct
	{
		uint8_t *pTbuf;
		uint16_t TxSize;
		uint16_t TxCount;
	} Master;
	struct
	{
		uint8_t *pRbuf;
		uint16_t RxSize;
		uint16_t RxCount;
		/*预留外部数据结构接口*/
		void *pHandle;
		DwinMap *pMap;
		uint16_t Events_Size;
	} Slave;
	UART_HandleTypeDef *huart;
} __attribute__((aligned(4)));

extern pDwinHandle Dwin_Object;
#define Dwin_Handler(obj) (obj->Dw_Poll(obj))
#define Dwin_Recive_Buf(obj) (obj->Slave.pRbuf)
#define Dwin_Recive_Len(obj) (obj->Slave.RxCount)
#define Dwin_Rx_Size(obj) (obj->Slave.RxSize)
extern void MX_DwinInit(void);

#endif /* INC_DWIN_H_ */
