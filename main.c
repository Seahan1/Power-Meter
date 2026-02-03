/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

// 

/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

// --- INA226 核心配置 ---
#define INA226_ADDR (0x40 << 1) // 0x80
// 100毫欧电阻对应的校准值 (LSB=0.1mA)
#define CAL_VALUE   0x0200      

// --- 简易 I2C 读写函数 ---
void INA226_WriteReg(uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;
    data[2] = value & 0xFF;
    HAL_I2C_Master_Transmit(&hi2c1, INA226_ADDR, data, 3, 100);
}

uint16_t INA226_ReadReg(uint8_t reg) {
    uint8_t val[2];
    HAL_I2C_Master_Transmit(&hi2c1, INA226_ADDR, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, INA226_ADDR, val, 2, 100);
    return (val[0] << 8) | val[1];
}
/* USER CODE END PV */

int main(void)
{
  // ... (HAL_Init, SystemClock_Config, MX_Init 等保持不变) ...

  /* USER CODE BEGIN 2 */
  char debug_msg[128]; // 加大缓冲区以容纳更多数据

  // 1. 开机检测 (不再轮询，直接 Check 0x80)
  printf("System Start. Checking INA226 at 0x80...\r\n");
  
  if(HAL_I2C_IsDeviceReady(&hi2c1, INA226_ADDR, 3, 100) != HAL_OK)
  {
      // 严重错误提示
      sprintf(debug_msg, "ERROR: INA226 Not Found at 0x80!\r\ncheck wiring: SDA=PB7, SCL=PB6\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
      
      // 可以在这里死循环闪灯报警，或者继续尝试
      while(1) {
          HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
          HAL_Delay(100); // 快闪报警
      }
  }
  else
  {
      sprintf(debug_msg, "SUCCESS: INA226 Connected!\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
  }

  // 2. 初始化 INA226 (R=100mΩ 配置)
  INA226_WriteReg(0x00, 0x8000); // 复位
  HAL_Delay(10);
  
  // 配置: 平均16次, 1.1ms转换时间, 连续模式
  INA226_WriteReg(0x00, 0x4127); 
  
  // 校准: R=100mΩ -> Cal=0x0200 (此时 Current_LSB = 0.1mA)
  INA226_WriteReg(0x05, CAL_VALUE); 

  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // 读取原始寄存器
      uint16_t v_raw = INA226_ReadReg(0x02); // Bus Voltage
      int16_t  i_raw = (int16_t)INA226_ReadReg(0x04); // Current

      // 物理量转换
      // 电压: LSB = 1.25 mV = 0.00125 V
      float voltage_f = v_raw * 0.00125f;

      // 电流: LSB = 0.1 mA = 0.0001 A (因为我们用了 Cal=0x0200)
      float current_f = i_raw * 0.0001f;

      // 功率: P = U * I (单位 W)
      float power_f = voltage_f * current_f;

      // 格式化输出 (增加 P=... W 字段)
      // 格式示例: "V=3.305 V | I=0.1052 A | P=0.347 W"
      sprintf(debug_msg, "V=%.3f V | I=%.4f A | P=%.4f W\r\n", 
              voltage_f, current_f, power_f);
      
      HAL_UART_Transmit(&huart2, (uint8_t*)debug_msg, strlen(debug_msg), 50);

      // 心跳灯
      HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);

      // 刷新率: 50ms (每秒20次)，足够流畅且不占用太多带宽
      HAL_Delay(50); 
  }
  /* USER CODE END WHILE */
}
