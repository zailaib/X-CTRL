#include "Basic/FileGroup.h"
#include "GUI/DisplayPrivate.h"
#include "Communication/ComPrivate.h"
#include "MillisTaskManager/MillisTaskManager.h"

// 遥控器 信号发射器代码住函数入口
// 实例化任务管理器
static MillisTaskManager mtmMain(true);

static void mtmMain_TimerCallback()
{
    // 任务进行调度执行
    mtmMain.Running(millis());
}

static void setup()
{
    // 各种设备初始化操作
    X_CTRL_Init();
    
    // 注册相关的设备数据更新任务执行函数
    mtmMain.Register(Com_Update, 10);
    mtmMain.Register(Joystick_Update, 10);
    mtmMain.Register(Button_Update, 20);
    mtmMain.Register(IMU_Update, 20);
    mtmMain.Register(Audio_Update, 20);

    // 遥控器的震动马达(**不是**小车🚗的马达哈)
    mtmMain.Register(MotorLRA_Update, 20);

    // 定时器中断回调函数
    Timer_SetInterruptBase(TIM_MTM_MAIN, 0xFF, 0xFF, mtmMain_TimerCallback, 2, 1);
    Timer_SetInterruptTimeUpdate(TIM_MTM_MAIN, 2000);
    // 定时器使能
    TIM_Cmd(TIM_MTM_MAIN, ENABLE);
}

static void loop()
{
    // 刷新屏幕显示
    Display_Update();
}

/**
  * @brief  Main Function
  * @param  None
  * @retval None
  */
int main(void)
{
    // 设置中断优先级分组 (2 位组优先级，2 位子优先级。)
    NVIC_SetPriorityGrouping(NVIC_PriorityGroup_2);
    Delay_Init();
    setup(); // 启动
    for(;;)loop(); // 进入循环刷新显示器
}
