#include "FileGroup.h"
#include "ComPrivate.h"
#include <libmaple/iwdg.h>

MillisTaskManager mtm(TP_MAX, true);
USBHID HID;

bool UseMtm = true;

void setup()
{
  // 关闭调试端口
  disableDebugPorts();

  StorageDataReg(Sim_ObjectSelect);
  EEPROM_Handle(EEPROM_Chs::ReadData);

  // 需要模拟的外设功能内容初始化
  Sim_DeviceInit();

  // 设置引脚模式
  pinMode(LED_Pin, OUTPUT);
  pinMode(KEY_Pin, INPUT_PULLUP);
  pinMode(NRF_IRQ_Pin, INPUT_PULLUP);
  pinMode(RandomSeed_Pin, INPUT_ANALOG);
  // led 闪烁5次
  __LoopExecute((togglePin(LED_Pin), delay(80)), 5);
  // 产生随机数
  randomSeed(analogRead(RandomSeed_Pin) * 1000);
  // 绑定引脚下降沿到函数用于设置当前的USB设备使用哪种模拟设备（键盘，鼠标，xbox）
  attachInterrupt(KEY_Pin, When_KeyPressEvent, FALLING);

  // 当需要模拟的外设确定了之后 进行无线以及协议的初始化
  Com_Init();

  if (UseMtm)
  { // 注册任务管理器中使用的函数
    mtm.TaskRegister(TP_NRF_Handler, Task_ComHandler, 10);
    mtm.TaskRegister(TP_Sim, Sim_DeviceProcess, 10);
    mtm.TaskRegister(TP_InfoLED, Task_InfoLED, 500);
  }

  while (!RCX::Handshake::Process())
  { // 初始化过程中过如果 只要握手不成功 则进入这里 闪烁LED 直到握手成功
    __IntervalExecute(togglePin(LED_Pin), 2000);
  }

  // 看门狗初始化 定期喂狗（refresh），以防止看门狗超时发生，从而触发系统复位
  // IWDG_PRE_64 是一个常量或枚举值，表示预分频器的分频系数 = 1/64 
  // 625 喂狗的时间间隔
  iwdg_init(IWDG_PRE_64, 625);
}

// Sim -> simulation 模拟的意思

void loop()
{
  // condition ? expression1 : expression2
  UseMtm ? mtm.Running(millis()) : Sim_DeviceProcess();
  iwdg_feed(); // 循环喂狗
}

static void When_KeyPressEvent()
{
  Sim_SetNextObj(); // 按键选择模拟哪种设备
  EEPROM_Handle(EEPROM_Chs::SaveData); //
  NVIC_SystemReset(); // 软件复位
}
