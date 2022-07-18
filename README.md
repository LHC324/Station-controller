# Station controller

- [Station controller](#station-controller)
  - [更新说明](#更新说明)
  - [简介](#简介)
  - [功能](#功能)
  - [使用方式](#使用方式)
  - [关键点](#关键点)
  - [测试项目](#测试项目)
  - [测试说明](#测试说明)
  - [函数说明](#函数说明)
  - [存在问题](#存在问题)

## 更新说明

| Date       | Author | Note          | Version number |
| ---------- | ------ | ------------- | -------------- |
| 2022.04.07 | LHC    | First edition | V1.0.0         |
| 2022.05.06 | LHC    | repair        | V1.1.0         |

- V1.0.1功能调整说明如下：
  - 增加了使用``4G``模块进行远程OTA功能。
  - 修改了flash区参数存储结构，调整了存储标志``Flag``为``CRC16``。
  - 修复了``FLASH_Write``函数按字节传入参数，写入按半字造成的参数区数据``size``成倍增加问题。
  - 修复了迪文屏幕参数界面``变量地址``与``变量名``不匹配问题。
  - 细分了迪文屏参数``汽化器出口压力阈值``为``汽化器出口压力启动值``和``汽化器出口压力停止值``。
  - 修复了用户程序流程中因**static**变量``mutex_flag``变量在停机模式下未赋初值为``flase``造成的由停机转启动后，储槽压力无法``>2.0Mpa``造成的无法启动问题。
  - 在增加了``阈值设定界面``中参数自动同步到云平台功能。（包含用户名、密码自动修改）。
  - 在云平台增加了``手动模式``，启用此模式后，用户原来控制逻辑将会失效。(仅调试模式和具有相应权限时可用)
  - 调整了``4G``模块数据打包长度：**1024**Byte->**2048**Byte。
  - 更改了spi通讯速率为**9Mbit/s**，原来为**18Mbit/s**，由于硬件布线导致的有时数据传输错误问题。
  - 修复了**停机模式**下用户阀**V5**进入安全模式后没有关闭的问题。

## 简介

- 气站控制器主要由 **STM32F103VET6** 和 **迪文屏幕DMT80480T043_01WTR** 两部分构成。
- 硬件IO构成情况：
  - 输入信号分为：``8`` 路数字输入 和 ``8`` 路模拟输入。
  - 输出信号分为：``7`` 路数字输出 和 ``4`` 路模拟输出。
  - 特别说明：
    > 数字输出和模拟输出，目前仅支持远程模块4G进行控制，数字输出（部分引脚占用）受用户逻辑影响。

## 功能

- 气站控制器，通过模拟PLC的外部数据采集策略（数字信号高、低电平和模拟信号4-20mA），映射到内存指定地址，内部从机号(4G远程访问站号)``0x02``。
- 四种信号：数字输入、数字输出、模拟输入、模拟输出均在内存中设有独立地址，操作策略遵循``Modbus``协议约定。
- ``RS485`` 访问站号 ``0x03`` ,目前暂未和站号 ``0x02`` 关联，故内部变量地址并不关联。
- 目前上层用户逻辑，主要通过检测``4``路模拟信号输入，分别组合控制``5``个电磁阀动作。

## 使用方式

- 注意按照IO外部接线图纸，正确连接各部分线路。
- 除用户逻辑占用的部分IO外，剩余输出IO可自由通过远程设置其状态。
- 用户默认登录名为：``2022`` , 密码为 ``1314``。
- 其内部阈值设定界面参数，均与用户逻辑紧密相关，参数设定与选取需查看需求文档后``谨慎``操作。

## 关键点

- 硬件部分：
  - MCU程序下载有时需要下载器提供``+3.3V``电源，尽管此时外部已经接通``+24V``电源。
  - 原理图上存在**错误**：ADC采集时，芯片引脚``-VREF``未与``GND``相连接，造成ADC采样值**正漂移**。
  ![原理图错误](Document/错误1.jpg)
  - ADC采样实际通道与DMA实际通道无法很好对应。（软件层已经屏蔽差异）
  - 硬件看门狗实际并未启用[2022.05.06]
  - 迪文屏幕使用串口功能时，要短接背面的**串口功能选择**点，否则默认走RS232通讯，串口通讯将失败。[2022.05.10]

- 软件部分：

    - **内存地址**映射关系

   | 寄存器组         | 映射对象 | 地址范围      | 操作类型 | 规则     |
   | ---------------- | -------- | ------------- | -------- | -------- |
   | InputCoil        | 数字输入 | 10001 - 19999 | R        | 顺序对应 |
   | Coil             | 数字输出 | 00001 - 09999 | R/W      | 顺序对应 |
   | Input Register   | 模拟输入 | 30001 - 39999 | R        | 顺序对应 |
   | Holding Register | 模拟输出 | 40001 - 49999 | R/W      | 顺序对应 |

   -  **站号** 对应外设
 
    | 站号id | 对应外设                 | 访问参数                | 访问方式 | 访问周期       |
    | ------ | ------------------------ | ----------------------- | -------- | -------------- |
    | 0x02   | 4G云平台                 | 115200 + 无校验位 + CRC | 云端轮询 | 1Min           |
    | 0x02   | 用户线程**Control_Task** | 线程参数                | 内核调度 | 1S             |
    | 0x03   | 外部RS485请求对象        | 9600 + 无校验位 +CRC    | 被动回复 | 发起请求方决定 |

    - 说明：目前应用场景比较简单，内部从站 ``0x03`` 与四种信号实际并无关联。
    - 模拟量与物理量换算关系说明：[CSDN博客](https://blog.csdn.net/weixin_36443823/article/details/112775994)
  
    - **OTA Flash**分区

     | 类型        | 起始地址    | 大小  | 中断向量偏移量 |
     | ----------- | ----------- | ----- | -------------- |
     | bootloader  | 0x0800 0000 | 20KB  | 0              |
     | app1        | 0x0800 5000 | 244KB | 0x5000         |
     | app2        | 0x0804 2000 | 244KB | 0x42000        |
     | update_flag | 0x0807 F000 | 2KB   | NULL           |
     | user_flag   | 0x0807 F800 | 2KB   | NULL           |

    - **ADC**自动校准说明
      - ``8``路模拟输入，默认选用第``AI0``作为校准参考源，ADC电流输入校准规则如下：
        | 参考源输入标准电流值 | ADC参考值 |
        | -------------------- | --------- |
        | 1.00mA               | 45        |
        | 4.00mA               | 376       |
        | 8.00mA               | 816       |
        | 12.00mA              | 1258      |
        | 16.00mA              | 1700      |
        | 20.00mA              | 2130      |
    - **DAC**自动校准说明
      - ``4``路模拟输出，每一路单独校准，DAC电流输出校准规则如下:
        | DAC默认输出值 | 电流参考值 |
        | ------------- | ---------- |
        | 0             | 0.05mA     |
        | 511           | 2.54mA     |
        | 1023          | 5.06mA     |
        | 1535          | 7.60mA     |
        | 2047          | 10.16mA    |
        | 2559          | 12.71mA    |
        | 3071          | 15.26mA    |
        | 3583          | 17.79mA    |
        | 4095          | 20.31mA    |
      - **注意**：``1``、``2``路使用``MCP4822``进行输出，精度高，``3``、``4``路使用芯片自带的DAC进行输出，某些输出段非线性，导致精度较低。
## 测试项目

- **数字量输入** 和 **数字量输出** 与硬件引脚对应关系。
- **模拟量输入** 对应ADC与电流间关系。
- **模拟量输出** 对应DAC与电流间关系。
- **屏幕各组件** 间地址映射、数据变量下发、上传与阈值界定测试。
- **用户逻辑** 测试各输入物理量与对应电磁阀间动作关系。
- **远程OTA** 测试下载和上载不同大小的程序块。

## 测试说明

- 模拟量与数字量所有测试均通过。
- 屏幕组件测试通过（控制部分：**数字量输出**和**模拟量输出**并未测试）。
- 用户逻辑，启动模式、停机模式、安全模式基本保障点以按照文档测试通过。（上柜后，出现控制**动作不满足**逻辑组合现象，但后续测试又未复现）


## 函数说明

- 暂略。


## 存在问题

- **4G模块**远程升级暂未实现[2022.05.06:已经实现]。
- **Shell**端口内部配置行为未启用[2022.07.18:已经实现]。
- **迪文屏幕**对数字量输出和模拟量输出暂未支持。
- **迪文屏幕**阈值设定界面中参数输入小数点仅支持一位（迪文屏幕原因:用``定点小数``表示浮点数）。
- **ADC与DAC**参数自动校准程序暂未匹配[2022.07.18:已经实现]。
- **模拟量输出**仅云平台具有相应权限后可以修改。
- **OTA升级**不同版本程序，中断向量偏移量必须与``App1``和``App2``启动地址一一对应。
- **Save_Param结构**当向内部怎加成员时，要在**Report_Backparam**中减去非阈值界面参数成员所占用的地址，否则可能覆盖迪文屏幕其他图标变量。
-  **屏幕页面缺失**在阈值设定界面，输入参数时，超过/低于参数界限时，缺少必要提示。（2个提醒页面：一个固定地址显示上下界限值）
-  **云平台模板**新版本数据上报均采用``小端``模式，并增加硬件版本显示。
-  **硬件参数**保存区域由原来的``code``区变更为``flash参数``专用区域。