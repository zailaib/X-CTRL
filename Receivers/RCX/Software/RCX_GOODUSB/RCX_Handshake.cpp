#include "RCX.h"

using namespace RCX;
using namespace Handshake;

static NRF_TRM *nrfTRM = NULL;
static NRF_FHSS *nrfFHSS = NULL;
static uint8_t NRF_TxBuff[32];
static uint8_t NRF_RxBuff[32];
static const uint8_t NRF_ComProcess_TimeMs = 10;
static bool NRF_PassBack_Enable = false;
static bool NRF_FHSS_Enable = false;

static Pack_t Master;   // 主机握手数据包
static Pack_t Slave;    // 从机握手数据包
static Pack_t Attached; // 绑定握手数据包

// 协议状态是全局变量 ： 程序加载的时候是 协议状态是空闲的
static uint8_t HankshakeState = State_Idle;

uint8_t Handshake::GetSlaveID()
{
    return Slave.ID;
}

bool Handshake::GetPassBack_Enable()
{
    return NRF_PassBack_Enable;
}

bool Handshake::GetFHSS_Enable()
{
    return NRF_FHSS_Enable;
}

void Handshake::Init(NRF_TRM *trm, NRF_FHSS *fhss, const char *name)
{
    nrfTRM = trm;
    nrfFHSS = fhss;
    // copy 字符串 name 覆盖 Slave.Description
    strncpy(Slave.Description, name, sizeof(Slave.Description));
    // 初始化后 协议状态是准备阶段
    HankshakeState = State_Prepare;
}

/**
 * @brief  NRF通信处理(T = 10ms)
 * @param  无
 * @retval 无
 */
static void NRF_ComProcess_Handler()
{
    nrfTRM->RecvTran(NRF_RxBuff, NRF_TxBuff);
}

/**
 * 一个静态函数，返回类型为 void，接受一个指向 Pack_t 类型数据的指针 master 作为参数
 * @brief  与主机通信,交换数据 (主从 SPI 数据交换)
 * @param  *master:主机握手包指针
 * @retval 无
 */
static void ComToMaster(Pack_t *master)
{
    // 是一个静态变量，它在函数内部保持其值在多次函数调用之间保持持久性
    static uint32_t lastTime;
    if (millis() - lastTime >= NRF_ComProcess_TimeMs) // 如果处理时长超过10ms则
    {
        lastTime = millis();
        NRF_ComProcess_Handler();
    }

    // NRF_TxBuff 可能是一个指针或数组，而 *(Pack_t *) 则是类型转换操作，将其解释为指向 Pack_t 类型数据的指针。
    *(Pack_t *)NRF_TxBuff = Slave; // 将 Slave 的值复制到 NRF_TxBuff 所指向的位置 也就是发送从设备的数据到缓存中


    // 一个指针操作，将 NRF_RxBuff 所指向的位置的值复制到 master 指针所指向的位置。
    *master = *(Pack_t *)NRF_RxBuff; // 读取主设备发过来的数据
}

/**
 * @brief  主机握手状态机
 * @param  无
 * @retval true: 成功, false: 失败
 */
bool Handshake::Process()
{
    if (nrfTRM == NULL || nrfFHSS == NULL)
        return false;

    uint8_t ret = false;
    uint32_t time = 0;

    switch (HankshakeState)
    {
    case State_Prepare: // 握手准备状态
        nrfTRM->Basic->SetRF_Enable(false);
        nrfTRM->Basic->SetAddress(RCX_BROADCAST_ADDR);  // 设置广播地址
        nrfTRM->Basic->SetFreqency(RCX_BROADCAST_FREQ); // 设置广播频段
        nrfTRM->Basic->SetSpeed(RCX_BROADCAST_SPEED);   // 设置广播通信速度
        nrfTRM->Basic->SetAutoRetryTimeout(10);
        nrfTRM->Basic->RX_Mode();
        nrfTRM->Basic->SetRF_Enable(true);

        Slave.BroadcastHead = BroadcastHead_SlaveWait; // 设置帧头识别码为从机等待
#if (RCX_USE_CUSTOM_ID != 0)
        Slave.ID = RCX_USE_CUSTOM_ID; // 设置自定义从机ID
#else
        Slave.ID = random(1, 0xFF); // 随机ID
#endif

        Slave.Type = RCX_TYPE;                            // 设置从机控制类型
        Slave.EnableFunction.Passback = RCX_USE_PASSBACK; // 设置是否支持回传
        Slave.EnableFunction.FHSS = RCX_USE_FHSS;         // 设置是否支持跳频
        Slave.Speed = 0;                                  // 设置从机通信速度
        Slave.CMD = CMD_ReqConnect;                       // 设置从机命令:请求连接

        HankshakeState = State_Search; // 状态机跳转至搜索状态
        break;

    case State_Search:                                         // 广播搜索状态
        ComToMaster(&Master);                                  // 在广播地址交换数据，将搜到的主机信息存至HandshakePack_Master
        if (Master.BroadcastHead == BroadcastHead_MasterAttach // 帧头识别码为主机请求连接
            && Master.CMD == CMD_AttachConnect                 // 主机命令为请求绑定
            && Master.ID == Slave.ID)                          // 主机ID与从机ID一致
        {
            Attached = Master; // 保存主机发过来的握手信息(地址，跳频表等)

            Slave.BroadcastHead = BroadcastHead_SlaveAttach; // 帧头识别码设置为从机响应绑定
            Slave.CMD = CMD_AgreeConnect;                    // 从机同意连接
            HankshakeState = State_ReqConnect;               // 状态机跳转至申请连接状态
        }
        break;

    case State_ReqConnect: // 申请连接状态
        time = millis();
        while (millis() - time < 1000) // 连续发送1秒数据包给主机表示可以连接
        {
            ComToMaster(&Master);
        }

        HankshakeState = State_Connected; // 状态机跳转至连接之后状态
        break;

    case State_Connected: // 连接之后状态
        nrfTRM->Basic->SetRF_Enable(false);
        nrfTRM->Basic->SetAddress(Attached.Address);      // 应用新地址
        nrfTRM->Basic->SetFreqency(Attached.FerqList[0]); // 应用新频道
        nrfTRM->Basic->SetSpeed(Attached.Speed);          // 应用新通信速率
        nrfTRM->Basic->SetAutoRetryTimeout(NRF_ComProcess_TimeMs);

        if (RCX_USE_PASSBACK)
            NRF_PassBack_Enable = Attached.EnableFunction.Passback;

        if (RCX_USE_FHSS)
        {
            NRF_FHSS_Enable = Attached.EnableFunction.FHSS;
            nrfFHSS->SetFreqHoppingList(Attached.FerqList, sizeof(Attached.FerqList));
        }

        nrfTRM->Basic->RX_Mode();
        nrfTRM->Basic->SetRF_Enable(true);

        HankshakeState = State_Idle;

        RCX::RxSetTimeUpdate();

        ret = true;
        break;
    }
    return ret;
}
