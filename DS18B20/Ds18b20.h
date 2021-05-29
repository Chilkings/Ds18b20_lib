/********************************************************
 *
 * @file    Ds18b20.c
 * @brief   Ds18b20驱动
 * @author  Chilkings
 * @date    2021/05/29
 *
********************************************************/


#ifndef _DS18B20_H_
#define _DS18B20_H_

#include "stm32f1xx.h"

#define FALSE 0
#define TRUE  1

#define CPU_FREQUENCY_MHZ    72		// STM32时钟主频

/* DS18B20 引脚定义 */
#define DS18B20_GPIO_PORT     GPIOB
#define DS18B20_GPIO_PIN      GPIO_PIN_8

/* DS18B20控制IO模式配置 */
#define DS18B20_OutPut_Mode() {DS18B20_GPIO_PORT->CRL &= 0x0FFFFFFF;DS18B20_GPIO_PORT->CRL |= 0x30000000;}
#define DS18B20_InPut_Mode()  {DS18B20_GPIO_PORT->CRL &= 0x0FFFFFFF;DS18B20_GPIO_PORT->CRL |= 0x80000000;}

/* DS18B20控制IO操作函数 */
#define DS18B20_Out(n)        (n?HAL_GPIO_WritePin(DS18B20_GPIO_PORT,DS18B20_GPIO_PIN,GPIO_PIN_SET):HAL_GPIO_WritePin(DS18B20_GPIO_PORT,DS18B20_GPIO_PIN,GPIO_PIN_RESET))
#define DS18B20_In()           HAL_GPIO_ReadPin(DS18B20_GPIO_PORT,DS18B20_GPIO_PIN)


void DS18B20_Init(void);
float DS18B20_GetTemp_SkipRom (void);
float DS18B20_GetTemp_MatchRom ( uint8_t * ds18b20_id);
int DS18B20_Search_AllID(uint64_t * Ds18b20_ID);

#endif /* _DS18B20_H_ */
