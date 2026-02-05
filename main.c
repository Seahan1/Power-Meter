/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

// --- INA226 地址配置 ---
// 通道1: 默认地址 (A0=GND, A1=GND) -> 0x40 << 1 = 0x80
#define INA226_ADDR1 (0x40 << 1) 
// 通道2: 修改地址 (A0=VCC, A1=GND) -> 0x41 << 1 = 0x82
#define INA226_ADDR2 (0x41 << 1) 

// 100毫欧电阻对应的校准值 (LSB=0.1mA)
#define CAL_VALUE   0x0200      

// --- 升级后的 I2C 读写函数 (增加 addr 参数) ---
void INA226_WriteReg(uint16_t addr, uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;
    data[2] = value & 0xFF;
    HAL_I2C_Master_Transmit(&hi2c1, addr, data, 3, 100);
}

uint16_t INA226_ReadReg(uint16_t addr, uint8_t reg) {
    uint8_t val[2];
    HAL_I2C_Master_Transmit(&hi2c1, addr, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, addr, val, 2, 100);
    return (val[0] << 8) | val[1];
}
/* USER CODE END PV */

int main(void)
{
  // ... (HAL_Init, SystemClock_Config, MX_Init 等保持不变) ...

  /* USER CODE BEGIN 2 */
  char debug_msg[128]; 

  // --- 1. 开机检测两个传感器 ---
  printf("System Start. Checking Dual INA226...\r\n");
  
  // 检测通道 1
  if(HAL_I2C_IsDeviceReady(&hi2c1, INA226_ADDR1, 3, 100) != HAL_OK) {
      sprintf(debug_msg, "ERR: Sensor 1 (0x80) Missing!\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
  } else {
      sprintf(debug_msg, "OK: Sensor 1 (0x80) Found.\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
      
      // 初始化通道 1
      INA226_WriteReg(INA226_ADDR1, 0x00, 0x8000); // 复位
      HAL_Delay(10);
      INA226_WriteReg(INA226_ADDR1, 0x00, 0x4127); // 配置
      INA226_WriteReg(INA226_ADDR1, 0x05, CAL_VALUE); // 校准
  }

  // 检测通道 2
  if(HAL_I2C_IsDeviceReady(&hi2c1, INA226_ADDR2, 3, 100) != HAL_OK) {
      sprintf(debug_msg, "ERR: Sensor 2 (0x82) Missing! Check A0 jumper.\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
  } else {
      sprintf(debug_msg, "OK: Sensor 2 (0x82) Found.\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
      
      // 初始化通道 2
      INA226_WriteReg(INA226_ADDR2, 0x00, 0x8000); 
      HAL_Delay(10);
      INA226_WriteReg(INA226_ADDR2, 0x00, 0x4127); 
      INA226_WriteReg(INA226_ADDR2, 0x05, CAL_VALUE); 
  }

  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // --- 读取通道 1 数据 ---
      uint16_t v1_raw = INA226_ReadReg(INA226_ADDR1, 0x02);
      int16_t  i1_raw = (int16_t)INA226_ReadReg(INA226_ADDR1, 0x04);
      
      float vol1 = v1_raw * 0.00125f;
      float cur1 = i1_raw * 0.0001f;
      float pow1 = vol1 * cur1;

      // 发送通道 1 (加前缀 CH:1)
      sprintf(debug_msg, "CH:1 V=%.3f V | I=%.4f A | P=%.4f W\r\n", vol1, cur1, pow1);
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 20);


      // --- 读取通道 2 数据 ---
      uint16_t v2_raw = INA226_ReadReg(INA226_ADDR2, 0x02);
      int16_t  i2_raw = (int16_t)INA226_ReadReg(INA226_ADDR2, 0x04);
      
      float vol2 = v2_raw * 0.00125f;
      float cur2 = i2_raw * 0.0001f;
      float pow2 = vol2 * cur2;

      // 发送通道 2 (加前缀 CH:2)
      sprintf(debug_msg, "CH:2 V=%.3f V | I=%.4f A | P=%.4f W\r\n", vol2, cur2, pow2);
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 20);

      // 心跳与刷新
      HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
      HAL_Delay(50); 
  }
  /* USER CODE END WHILE */
}
