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

 // 初始化设备
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

  Com_Init();

  if (UseMtm)
  {
    mtm.TaskRegister(TP_NRF_Handler, Task_ComHandler, 10);
    mtm.TaskRegister(TP_Sim, Sim_DeviceProcess, 10);
    mtm.TaskRegister(TP_InfoLED, Task_InfoLED, 500);
  }

  while (!RCX::Handshake::Process())
  {
    __IntervalExecute(togglePin(LED_Pin), 2000);
  }

  iwdg_init(IWDG_PRE_64, 625);
}

void loop()
{
  UseMtm ? mtm.Running(millis()) : Sim_DeviceProcess();
  iwdg_feed();
}

static void When_KeyPressEvent()
{
  Sim_SetNextObj();
  EEPROM_Handle(EEPROM_Chs::SaveData);
  NVIC_SystemReset(); // 软件复位
}
