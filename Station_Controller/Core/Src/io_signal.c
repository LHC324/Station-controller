#include "io_signal.h"
#include "main.h"
#include "mdrtuslave.h"
#include "adc.h"
#include "Mcp4822.h"
#include "shell_port.h"
#include "tool.h"
#if defined(USING_FREERTOS)
#include "cmsis_os.h"
#endif

#define Get_GPIO_Port(__Port, __P1, __P2) \
    ((__Port) ? (__P1) : (__P2))

#define Get_GPIO_Pin(__Pin, __Offset, __P1, __P2) \
    ((__Pin) < (__Offset) ? ((__P1) << (__Pin)) : (__P2) << ((__Pin) - (__Offset)))

#define Get_Digital_Port(GPIO_Port) \
    (GPIO_Port ? GPIOD : GPIOC)

#define Get_Digital_Pin(GPIO_Pin) \
    (GPIO_Pin < 5U ? (DI_0_Pin << GPIO_Pin) : DI_5_Pin << (GPIO_Pin - 5U))

#define Get_ADC_Channel(__Ch, __Offset, __MaxCh) \
    ((__Ch) < (__Offset) ? ((__MaxCh)-1U - (__Ch)) : ((__Ch) - (__Offset)))

/**
 * @brief	外部数字量输入处理
 * @details	STM32F103C8T6共在io口扩展了8路数字输入
 * @param	None
 * @retval	None
 */
void Read_Digital_Io(void)
{
    GPIO_TypeDef *pGPIOx;
    uint16_t GPIO_Pinx;
    mdBit bit = mdLow;
    mdU32 addr;
    mdSTATUS ret = mdFALSE;

    for (uint16_t i = 0; i < EXTERN_DIGITALIN_MAX; i++)
    {
        pGPIOx = Get_GPIO_Port(!!(i < 5U), GPIOD, GPIOC);
        GPIO_Pinx = Get_GPIO_Pin(i, 5U, DI_0_Pin, DI_5_Pin);
        /*读取外部数字引脚状态:翻转光耦的输入信号*/
        bit = (mdBit)!HAL_GPIO_ReadPin(pGPIOx, GPIO_Pinx);
        /*计算出出当前写入地址*/
        addr = INPUT_DIGITAL_START_ADDR + i;
        /*写入输入线圈*/
        ret = mdRTU_WriteInputCoil(Slave1_Object, addr, bit);
        /*写入失败*/
        if (ret == mdFALSE)
        {
#if defined(USING_DEBUG)
            shellPrint(Shell_Object, "IN_D[%d] = 0x%d\r\n", i, bit);
#endif
            break;
        }
    }
}

/**
 * @brief	数字量输出
 * @details	STM32F103VET6共在io口扩展了7路数字输出
 * @param	None
 * @retval	None
 */
void Write_Digital_IO(void)
{
    GPIO_TypeDef *pGPIOx;
    uint16_t GPIO_Pinx;
    GPIO_PinState bit = GPIO_PIN_RESET;
    mdU32 addr = OUT_DIGITAL_START_ADDR;
    mdSTATUS ret = mdFALSE;

    for (uint16_t i = 0; i < EXTERN_DIGITALOUT_MAX; i++)
    {
        pGPIOx = Get_GPIO_Port(!!(i < 4U), GPIOB, GPIOD);
        GPIO_Pinx = Get_GPIO_Pin(i, 4U, Q_0_Pin, Q_4_Pin);
        /*此处添加规则*/
        /*读出线圈*/
        ret = mdRTU_ReadCoil(Slave1_Object, addr + i, bit);
        /*写入失败*/
        if (ret == mdFALSE)
        {
#if defined(USING_DEBUG)
            shellPrint(Shell_Object, "OUT_D[%d] = 0x%d\r\n", i, bit);
#endif
            break;
        }
        HAL_GPIO_WritePin(pGPIOx, GPIO_Pinx, bit);
    }
}

/**
 * @brief	外部模拟量输入处理
 * @details	STM32F103VET6共在io口扩展了8路模拟输入
 * @param	None
 * @retval	None
 */
void Read_Analog_Io(void)
{
#define CP 0.005378F
#define CQ 0.375224F
    mdSTATUS ret = mdFALSE;
    uint16_t tch = 0;
#if defined(KALMAN)
    KFP hkfp = {
        .Last_Covariance = LASTP,
        .Kg = 0,
        .Now_Covariance = 0,
        .Output = 0,
        .Q = COVAR_Q,
        .R = COVAR_R,
    };
#else
    SideParm side = {
        .First_Flag = true,
        .Head = &side.SideBuff[0],
        .SideBuff = {0},
        .Sum = 0,
    };
#endif
#if defined(USING_FREERTOS)
    float *pdata = (float *)CUSTOM_MALLOC(ADC_DMA_CHANNEL * sizeof(float));
    if (!pdata)
        CUSTOM_FREE(pdata);
#if defined(KALMAN)
    KFP *pkfp = (KFP *)CUSTOM_MALLOC(ADC_DMA_CHANNEL * sizeof(KFP));
    if (!pkfp)
        CUSTOM_FREE(pkfp);
#else
    SideParm *pside = (SideParm *)CUSTOM_MALLOC(ADC_DMA_CHANNEL * sizeof(SideParm));
    if (!pside)
        CUSTOM_FREE(pside);
#endif
#else
    float pdata[ADC_DMA_CHANNEL];
    KFP pkfp[ADC_DMA_CHANNEL];
#endif
    memset(pdata, 0x00, ADC_DMA_CHANNEL * sizeof(float));
    for (uint16_t ch = 0; ch < ADC_DMA_CHANNEL; ch++)
    {
#if defined(KALMAN)
        memcpy(&pkfp[ch], &hkfp, sizeof(hkfp));
#else
        memcpy(&pside[ch], &pside, sizeof(pside));
#endif
    }

#if defined(USING_DEBUG)
    // // uint16_t sum;
    // for (uint16_t ch = 0; ch < ADC_DMA_CHANNEL; ch++)
    // {
    //     // tch = (ch < 4U) ? (7U - ch) : (ch - 4U);
    //     tch = Get_ADC_Channel(ch, 4U, ADC_DMA_CHANNEL);
    //     // sum = 0;
    //     // for (uint16_t i = 0; i < 10; i++)
    //     // {
    //     //     sum += Get_AdcValue(tch);
    //     // }
    //     // shellPrint(Shell_Object, "R_AD[%d] = %d\r\n", ch, sum / 10U);

    //     pdata = CP * Get_AdcValue(tch) + CQ;
    //     pdata = (pdata <= CQ) ? 0 : pdata;
    //     shellPrint(Shell_Object, "R_C[%d] = %.3f\r\n", ch, pdata);
    // }
    // return;
#endif
    for (uint16_t ch = 0; ch < ADC_DMA_CHANNEL; ch++)
    { /*获取DAC值*/
        tch = Get_ADC_Channel(ch, 4U, ADC_DMA_CHANNEL);
        pdata[ch] = CP * Get_AdcValue(tch) + CQ;
        pdata[ch] = (pdata[ch] <= CQ) ? 0 : pdata[ch];
        /*滤波处理*/
#if defined(KALMAN)
        pdata[ch] = kalmanFilter(&pkfp[ch], pdata[ch]);
#else
        pdata[ch] = sidefilter(&pside[ch], pdata[ch]);
#endif
        /*大小端转换*/
        Endian_Swap((uint8_t *)&pdata[ch], 0U, sizeof(float));

#if defined(USING_DEBUG)
        // shellPrint(Shell_Object, "R_AD[%d] = %.3f\r\n", ch, pdata[ch]);
#endif
    }
    /*写入输入寄存器*/
    ret = mdRTU_WriteInputRegisters(Slave1_Object, INPUT_ANALOG_START_ADDR, ADC_DMA_CHANNEL * 2U, (mdU16 *)pdata);
    /*写入失败*/
    if (ret == mdFALSE)
    {
#if defined(USING_DEBUG)
        shellPrint(Shell_Object, "R_AD = 0x%.3f\r\n", *pdata);
#endif
    }
#if defined(USING_FREERTOS)
    CUSTOM_FREE(pdata);
#if defined(KALMAN)
    CUSTOM_FREE(pkfp);
#else
    CUSTOM_FREE(pside);
#endif
#endif
}

/**
 * @brief	模拟量输出
 * @details	STM32F103VET6共在io口扩展了6路数字输出
 * @param	None
 * @retval	None
 */
void Write_Analog_IO(void)
{
    mdSTATUS ret = mdFALSE;
    Dac_Obj dac_object[EXTERN_ANALOGOUT_MAX] = {
        {.Channel = DAC_OUT1, .Mcpxx_Id = Input_A},
        {.Channel = DAC_OUT2, .Mcpxx_Id = Input_B},
        {.Channel = DAC_OUT3, .Mcpxx_Id = Input_Other},
        {.Channel = DAC_OUT4, .Mcpxx_Id = Input_Other},
    };
#if defined(USING_FREERTOS)
    float *pdata = (float *)CUSTOM_MALLOC(EXTERN_ANALOGOUT_MAX * sizeof(float));
    if (!pdata)
        CUSTOM_FREE(pdata);
#else
    float pdata[EXTERN_ANALOGOUT_MAX];
#endif
    memset(pdata, 0x00, EXTERN_ANALOGOUT_MAX * sizeof(float));
    /*读出保持寄存器*/
    ret = mdRTU_ReadHoldRegisters(Slave1_Object, OUT_ANALOG_START_ADDR, EXTERN_ANALOGOUT_MAX * 2U, (mdU16 *)pdata);
    /*读取失败*/
    if (ret == mdFALSE)
    {
#if defined(USING_DEBUG)
        shellPrint(Shell_Object, "W_AD = %.3f\r\n", *pdata);
#endif
    }
    for (uint16_t ch = 0; ch < EXTERN_ANALOGOUT_MAX; ch++)
    {
        /*大小端转换*/
        Endian_Swap((uint8_t *)&pdata[ch], 0U, sizeof(float));
#if defined(USING_DEBUG)
        // shellPrint(Shell_Object, "W_AD[%d] = %.3f\r\n", ch, pdata[ch]);
#endif
        Output_Current(&dac_object[ch], pdata[ch]);
    }
#if defined(USING_FREERTOS)
    CUSTOM_FREE(pdata);
#endif
}
