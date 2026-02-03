/*
 * INA226.h
 *
 * Created on: Jan 27, 2026
 * Author: Xiehan
 */

#ifndef INC_INA226_H_
#define INC_INA226_H_

#include "main.h" // 引入 HAL 库定义

// --- 1. 设备地址配置 ---
// A0, A1 接地时的默认地址 (0x40)。HAL库写操作需要左移1位 (0x80)
#define INA226_ADDR_DEFAULT  (0x40 << 1)

// --- 2. 寄存器映射 ---
#define INA226_REG_CONFIG    0x00
#define INA226_REG_SHUNT     0x01
#define INA226_REG_BUS       0x02
#define INA226_REG_POWER     0x03
#define INA226_REG_CURRENT   0x04
#define INA226_REG_CAL       0x05
#define INA226_REG_MASK      0x06
#define INA226_REG_ALERT     0x07
#define INA226_REG_ID        0xFF

// --- 3. 配置参数宏 ---
// 复位
#define INA226_RESET_MASK    0x8000

// 平均采样模式 (Averaging Mode)
#define INA226_AVG_1         0x0000
#define INA226_AVG_4         0x0000
#define INA226_AVG_16        0x0200 // 推荐: 兼顾稳定与速度
#define INA226_AVG_64        0x0400
#define INA226_AVG_128       0x0600
#define INA226_AVG_256       0x0800
#define INA226_AVG_512       0x0A00
#define INA226_AVG_1024      0x0C00

// 转换时间 (Vbus & Vshunt CT)
#define INA226_CT_1100US     0x0080 // 1.1ms (默认)

// 工作模式
#define INA226_MODE_CONT_SHUNT_BUS 0x0007 // 连续读取分流电压和总线电压

// --- 4. 结构体定义 (面向对象风格) ---
typedef struct {
    I2C_HandleTypeDef *hi2c;  // I2C 句柄
    uint16_t address;         // 设备地址
    float shunt_resistor;     // 采样电阻值 (欧姆)
} INA226_Handle_t;

// --- 5. 函数声明 ---
/**
 * @brief 初始化 INA226
 * @param dev: 设备句柄指针
 * @param hi2c: HAL库I2C句柄 (如 &hi2c1)
 * @param addr: 设备地址 (通常是 INA226_ADDR_DEFAULT)
 * @param r_shunt: 采样电阻值 (单位: 欧姆)
 * @return HAL_StatusTypeDef
 */
uint8_t INA226_Init(INA226_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint16_t addr, float r_shunt);


/**
 * @brief 获取总线电压 (负载电压)
 * @return 单位: V
 */
float INA226_GetBusVoltage(INA226_Handle_t *dev);

/**
 * @brief 获取分流电压 (采样电阻两端压差)
 * @return 单位: mV
 */
float INA226_GetShuntVoltage(INA226_Handle_t *dev);

/**
 * @brief 获取电流 (通过 Vshunt / R 计算)
 * @return 单位: mA
 */
float INA226_GetCurrent(INA226_Handle_t *dev);

/**
 * @brief 获取功率 (BusVoltage * Current)
 * @return 单位: mW
 */
float INA226_GetPower(INA226_Handle_t *dev);

#endif /* INC_INA226_H_ */
