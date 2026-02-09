/*
 * INA226.c
 *
 * Created on: Jan 27, 2026
 * Author: Embedded Expert
 */

#include "ina226.h"

// 私有辅助函数：写寄存器
static uint8_t INA226_WriteReg(INA226_Handle_t *dev, uint8_t reg, uint16_t value) {
    uint8_t data[2];
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
    return HAL_I2C_Mem_Write(dev->hi2c, dev->address, reg, I2C_MEMADD_SIZE_8BIT, data, 2, 100);
}

// 私有辅助函数：读寄存器
static uint16_t INA226_ReadReg(INA226_Handle_t *dev, uint8_t reg) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(dev->hi2c, dev->address, reg, I2C_MEMADD_SIZE_8BIT, data, 2, 100);
    return (data[0] << 8) | data[1];
}

uint8_t INA226_Init(INA226_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint16_t addr, float r_shunt) {
    // 1. 填充句柄
    dev->hi2c = hi2c;
    dev->address = addr;
    dev->shunt_resistor = r_shunt;

    // 2. 配置寄存器 (0x00)
    // 默认配置: 平均16次, 转换时间1.1ms, 连续模式
    // 这样可以有效过滤噪声
    uint16_t config = INA226_AVG_16 | INA226_CT_1100US | INA226_CT_1100US | INA226_MODE_CONT_SHUNT_BUS;

    if (INA226_WriteReg(dev, INA226_REG_CONFIG, config) != HAL_OK) {
        return HAL_ERROR;
    }
}

float INA226_GetBusVoltage(INA226_Handle_t *dev) {
    int16_t raw = (int16_t)INA226_ReadReg(dev, INA226_REG_BUS);
    // LSB = 1.25mV
    return raw * 0.00125f;
}

float INA226_GetShuntVoltage(INA226_Handle_t *dev) {
    int16_t raw = (int16_t)INA226_ReadReg(dev, INA226_REG_SHUNT);
    // LSB = 2.5uV, 结果转换为 mV
    return raw * 0.0025f;
}

float INA226_GetCurrent(INA226_Handle_t *dev) {
    float v_shunt_mv = INA226_GetShuntVoltage(dev);
    // I = U / R, 结果单位 mA
    return v_shunt_mv / dev->shunt_resistor;
}

float INA226_GetPower(INA226_Handle_t *dev) {
    float voltage = INA226_GetBusVoltage(dev); // V
    float current = INA226_GetCurrent(dev);    // mA
    // P = U * I, 结果单位 mW
    return voltage * current;
}



