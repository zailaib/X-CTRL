#include "FileGroup.h"
#include "ComPrivate.h"

// SPI通信引脚 实例化 nrf对象 以操作nrf相关功能
NRF_Basic nrf(
    NRF_MOSI_Pin,
    NRF_MISO_Pin,
    NRF_SCK_Pin,
    NRF_CE_Pin,
    NRF_CSN_Pin
);
// IRQ   MISO
// MOSI  SCK
// CSN   CE
// VCC   GND

// 传输操作对象实例化
static NRF_TRM nrfTRM(&nrf);

// 跳帧频率操作对象实例化
static NRF_FHSS nrfFHSS(&nrf);

// int 类型 32bit的 buffer数组 接收数据存储位置的数组
uint8_t NRF_RxBuff[32]; 
// int 类型 32bit的 buffer数组 发送数据存储位置的数组 
uint8_t NRF_TxBuff[32];


// 定义静态常量字符串指针名为 SlaveName 指针地址存储值为 GOODUSB字符串  
static const char *SlaveName = "GOODUSB";

void Com_SetDescription(const char *name)
{
  SlaveName = name;
}

void Com_Init()
{
  // 无线模块初始化 + 握手协议初始化

ReInit: // goto 标签定义 可以换个名字 比如 NRFInit
  nrf.Init();              // nrf芯片初始化
  nrf.SetRF_Enable(false); // nrf芯片使能 fasle 表示先关闭接收发送信号的开关 NRF_CE_Pin = 0 
  nrf.SetPayloadWidth(sizeof(NRF_TxBuff), sizeof(NRF_RxBuff));
  nrf.SetRF_Enable(true); // NRF_CE_Pin = 1 开启收发信号

  if (!nrf.IsDetect())
  {
    togglePin(LED_Pin); // 闪烁LED
    goto ReInit; // 当没有发现无线模块初始化成功 则一直循环探测
  }

  // 当无线模块初始化成功后则进行握手初始化
  RCX::Handshake::Init(&nrfTRM, &nrfFHSS, SlaveName);
}

void Task_ComHandler()
{
  if (RCX::Handshake::GetPassBack_Enable())
  {
    RCX::TxLoadPack(NRF_TxBuff);
    if (RCX::Handshake::GetFHSS_Enable())
    {
      nrfFHSS.RxProcess(NRF_RxBuff, NRF_TxBuff);
    }
    else
    {
      nrfTRM.RecvTran(NRF_RxBuff, NRF_TxBuff);
    }
  }
  else
  {
    if (RCX::Handshake::GetFHSS_Enable())
    {
      nrfFHSS.RxProcess(NRF_RxBuff);
    }
    else
    {
      if (nrf.GetRF_State() != nrf.State_RX)
      {
        nrf.RX_Mode();
      }
      nrf.Recv(NRF_RxBuff);
    }
  }

  RCX::RxLoadPack(NRF_RxBuff);

  if (RCX::RxGetSignalLost())
    NVIC_SystemReset();
}
