#include "Basic/FileGroup.h"
#include "MPU6050/MPU6050.h"
#include "math.h"
#include "IMU_Private.h"
#include "Communication/RCX/RCX.h"

#define IMU_RAD(x) (radians(((double)x) * 2000.0f / 32767.0f))

/*实例化MPU6050对象*/
static MPU6050 mpu;

/*MPU角度数据*/
IMU_Axis_TypeDef IMU_Axis;
IMU_Channel_TypeDef IMU_Channel;
static bool IsAxisChannelReset = false;
static float q0 = 1, q1 = 0, q2 = 0, q3 = 0;
static float exInt = 0, eyInt = 0, ezInt = 0;

void IMU_NormReset()
{
    q0 = 1, q1 = 0, q2 = 0, q3 = 0;
    exInt = 0, eyInt = 0, ezInt = 0;
}

void IMU_LimitSetDefault()
{
    /*俯仰*/
    IMU_Axis.Pitch.Limit = 90;

    /*横滚*/
    IMU_Axis.Roll.Limit = 90;

    /*航向*/
    IMU_Axis.Yaw.Limit = 180;
}

/**
 * @brief  姿态解算
 * @param
 * @retval 无
 */
static void IMU_NormUpdate(float ax, float ay, float az, float gx, float gy, float gz)
{
    /*姿态解算数据*/
    const double Kp = 1.0f;
    const double Ki = 0.01f;
    const double halfT = 0.01f; /*20ms*/

    float norm;
    float vx, vy, vz;
    float ex, ey, ez;

    norm = sqrt(ax * ax + ay * ay + az * az);
    ax = ax / norm;
    ay = ay / norm;
    az = az / norm;

    vx = 2 * (q1 * q3 - q0 * q2);
    vy = 2 * (q0 * q1 + q2 * q3);
    vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

    ex = (ay * vz - az * vy);
    ey = (az * vx - ax * vz);
    ez = (ax * vy - ay * vx);

    exInt = exInt + ex * Ki;
    eyInt = eyInt + ey * Ki;
    ezInt = ezInt + ez * Ki;

    gx = gx + Kp * ex + exInt;
    gy = gy + Kp * ey + eyInt;
    gz = gz + Kp * ez + ezInt;

    q0 = q0 + (-q1 * gx - q2 * gy - q3 * gz) * halfT;
    q1 = q1 + (q0 * gx + q2 * gz - q3 * gy) * halfT;
    q2 = q2 + (q0 * gy - q1 * gz + q3 * gx) * halfT;
    q3 = q3 + (q0 * gz + q1 * gy - q2 * gx) * halfT;

    norm = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 = q0 / norm;
    q1 = q1 / norm;
    q2 = q2 / norm;
    q3 = q3 / norm;

    IMU_Axis.Roll.AngleReal = asin(-2 * q1 * q3 + 2 * q0 * q2) * 57.3f;
    IMU_Axis.Pitch.AngleReal = -atan2(2 * q2 * q3 + 2 * q0 * q1, -2 * q1 * q1 - 2 * q2 * q2 + 1) * 57.3f;
    IMU_Axis.Yaw.AngleReal = -atan2(2 * (q1 * q2 + q0 * q3), q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.3f;
}

static bool IsCalibrateStart = false;
static uint32_t CalibrateStartTime = 0;
static uint16_t CalibrateCnt;
static int32_t gx_sum = 0, gy_sum = 0, gz_sum = 0;

void IMU_CalibrateStart()
{
    if (IsCalibrateStart || !CTRL.State->IMU)
        return;

    IsCalibrateStart = true;
    CalibrateStartTime = millis();
    gx_sum = 0, gy_sum = 0, gz_sum = 0;
    CalibrateCnt = 0;
}

static void IMU_Calibrate(int16_t *gx, int16_t *gy, int16_t *gz)
{
    static int16_t gx_cab = 0, gy_cab = 0, gz_cab = 0;
    if (IsCalibrateStart)
    {
        gx_sum += *gx;
        gy_sum += *gy;
        gz_sum += *gz;
        CalibrateCnt++;
        if (millis() - CalibrateStartTime >= 2000)
        {
            gx_cab = gx_sum / CalibrateCnt;
            gy_cab = gy_sum / CalibrateCnt;
            gz_cab = gz_sum / CalibrateCnt;

            IMU_NormReset();
            IsCalibrateStart = false;
        }
    }
    else
    {
        *gx -= gx_cab;
        *gy -= gy_cab;
        *gz -= gz_cab;
    }
}

static void IMU_AngleToChannel(IMU_Angle_TypeDef *angle, IMU_ChBasic_TypeDef *ch)
{
    angle->Angle = constrain(angle->AngleReal, -angle->Limit, angle->Limit);
    ch->Data = (ch->Reverse ? -1 : 1) * __Map(angle->Angle, -angle->Limit, angle->Limit, -RCX_CHANNEL_DATA_MAX, RCX_CHANNEL_DATA_MAX);
    ch->delta = ch->Data - ch->Last;
    ch->Last = ch->Data;
}

static void IMU_ChannelUpdate()
{
    IMU_AngleToChannel(&IMU_Axis.Pitch, &IMU_Channel.Pitch);
    IMU_AngleToChannel(&IMU_Axis.Roll, &IMU_Channel.Roll);
    IMU_AngleToChannel(&IMU_Axis.Yaw, &IMU_Channel.Yaw);
}

static void IMU_AxisChannelReset()
{
    IMU_Axis.Pitch.AngleReal = 0;
    IMU_Axis.Roll.AngleReal = 0;
    IMU_Axis.Yaw.AngleReal = 0;
    IMU_ChannelUpdate();
    IMU_ChannelUpdate();
    IsAxisChannelReset = true;
}

/**
 * @brief  MPU6050初始化
 * @param  无
 * @retval 无
 */
void IMU_Init()
{
    DEBUG_FUNC_LOG();
    __LoopExecute(mpu.initialize(), 50);
}

/**
 * @brief  IMU处理任务
 * IMU代表惯性测量单元（Inertial Measurement Unit）。
 * IMU是一种集成了多个传感器的装置
 * 用于测量和提供关于物体的姿态、加速度和角速度等信息。
 * 加速度计（Accelerometer）：测量物体在三个轴向上的加速度，用于确定物体的线性加速度和倾斜角度。
 * 陀螺仪（Gyroscope）：测量物体绕三个轴向上的角速度，用于检测物体的旋转和转动。
 * 磁力计（Magnetometer）：测量物体周围的磁场强度，用于确定物体的方向和方位。
 * @param  无
 * @retval 无
 */
void IMU_Update()
{
    if (!CTRL.State->IMU) // 访问指针类型成员变量 判断状态 IMU是否开启 
    {
        if (!IsAxisChannelReset)
        {
            IMU_AxisChannelReset();
        }
        return;
    }
    IsAxisChannelReset = false;

    if (I2C_GetLocked())
        return;

    int16_t ax, ay, az, gx, gy, gz;

    // 从IMU传感器（可能是MPU系列的传感器）中获取加速度计和陀螺仪的6轴（三轴加速度和三轴角速度）原始数据，并将其存储在相应的变量中
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    // 这个函数对陀螺仪的原始数据进行校准
    IMU_Calibrate(&gx, &gy, &gz);
    // 将经过校准的加速度计和陀螺仪数据传递给姿态更新函数。它可能会将角度数据转换为弧度，并更新姿态信息。
    IMU_NormUpdate(IMU_RAD(ay), IMU_RAD(ax), IMU_RAD(-az), IMU_RAD(gy), IMU_RAD(gx), IMU_RAD(-gz));
    // 这个函数执行通道数据的更新操作。可能会根据需要对通道数据进行处理或转换。
    IMU_ChannelUpdate();
}
