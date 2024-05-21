#ifndef __JOYSTICKMAP_H
#define __JOYSTICKMAP_H

#include <stdint.h>
/**
 * @brief
 * 这个类的目的是处理摇杆输入值与输出值之间的映射关系。
 * 通过设置参考值、曲线和死区等参数，可以调整映射的方式，以便实现更精确和符合需求的摇杆控制。
 */
class JoystickMap
{
public:
    JoystickMap(int16_t *table, uint16_t size);
    ~JoystickMap();

    void SetInputReference(int16_t min, int16_t mid, int16_t max);
    void SetInput(int16_t in_val);
    void SetCurve(float startK, float endK);
    void SetOutputMax(int16_t max);
    void SetInputDeadZone(uint16_t in_dz);
    float GetOutputCurve(int16_t in_val, int16_t max);
    int16_t GetOutput();
    int16_t GetNext(int16_t in_val);

private:
    int16_t *CurveTable;
    uint16_t CurveTableSize;
    int16_t InputMin, InputMid, InputMax;
    int16_t OutputMax;
    int16_t Output;
    int16_t InputDeadZone;

    float LinearMap(float value, float in_min, float in_max, float out_min, float out_max);
    float NonlinearMap(float value, float in_min, float in_max, float out_min, float out_max, float startK, float endK);
};

#endif
