/********************************************************
 *
 * @file    Ds18b20.c
 * @brief   Ds18b20驱动
 * @author  Chilkings
 * @date    2021/05/29
 *
********************************************************/


#include "ds18b20.h"
#include <stdio.h>
#include "string.h"

// 搜索算法用到的标志位
unsigned char ROM_NO[8]; 
int LastDiscrepancy;
int LastFamilyDiscrepancy;
int LastDeviceFlag;
unsigned char crc8;

// crc校验用
static unsigned char dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};


/**
  * @brief    crc校验
  * @param    需校验的值
  * @retval   crc校验值
*/
unsigned char docrc8(unsigned char value)
{
   crc8 = dscrc_table[crc8 ^ value];
   return crc8;
}

/**
  * @brief    基于滴答定时器的微秒级延时函数
  * @param    延时时间
  * @retval   none
  * @note     stm32全系列通用，只需要将宏定义 CPU_FREQUENCY_MHZ 根据时钟主频修改
*/
void DS18B20_Delay_us(__IO uint32_t delay)
{
    int last, curr, val;
    int temp;

    while (delay != 0)
    {
        temp = delay > 900 ? 900 : delay;
        last = SysTick->VAL;
        curr = last - CPU_FREQUENCY_MHZ * temp;
        if (curr >= 0)
        {
            do
            {
                val = SysTick->VAL;
            }
            while ((val < last) && (val >= curr));
        }
        else
        {
            curr += CPU_FREQUENCY_MHZ * 1000;
            do
            {
                val = SysTick->VAL;
            }
            while ((val <= last) || (val > curr));
        }
        delay -= temp;
    }
}

/**
  * @brief    发送复位信号
  * @param    none
  * @retval   none
*/
static void DS18B20_Send_Reset_Single(void)
{
	DS18B20_OutPut_Mode();
    
    /* 拉低总线 480 - 960 us*/
	DS18B20_Out(0);
	DS18B20_Delay_us(750);
    
    /* 释放总线 15 - 60 us */
	DS18B20_Out(1);
	DS18B20_Delay_us(15);
}

/**
  * @brief    检测DS18B20存在脉冲
  * @param    none
  * @retval   0 DS18B20设备正常
  * @retval   1  DS18B20设备响应复位信号失败
  * @retval   2  DS18B20设备释放总线失败
*/
static uint8_t DS18B20_Check_Ready_Single(void)
{
	uint8_t cnt = 0;
    
	/* 1.检测存在脉冲 */
	DS18B20_InPut_Mode();
    
    //等待DS18B20 拉低总线 （60~240 us 响应复位信号）
	while (DS18B20_In() && cnt < 240) {
		DS18B20_Delay_us(1);
		cnt++;
	}
    
	if (cnt > 240) {
        return 1;
    }
    
	/* 2.检测DS18B20是否释放总线 */	
	cnt = 0;
	DS18B20_InPut_Mode();
    
    //判断DS18B20是否释放总线（60~240 us 响应复位信号之后会释放总线）
	while ((!DS18B20_In()) && cnt<240) {
		DS18B20_Delay_us(1);
		cnt++;
	}
    
	if (cnt > 240) {
        return 2;
    } else {
        return 0;
    }
}

/**
  * @brief    检测DS18B20设备是否正常(先复位再检测脉冲)
  * @param    none
  * @retval   0 DS18B20设备正常
  * @retval   1  DS18B20设备响应复位信号失败
  * @retval   2  DS18B20设备释放总线失败
*/
static uint8_t DS18B20_Check_Device(void)
{
    /*1.主机发送复位信号*/
	DS18B20_Send_Reset_Single();
    
    /*2.检测存在脉冲*/
	return DS18B20_Check_Ready_Single();
}

/**
  * @brief    DS18B20初始化
  * @param    none
  * @retval   none
*/
void DS18B20_Init(void)
{
	/* 1.DS18B20控制引脚初始化 */
    //在main函数中已经初始化，不需要再次重复。

	/*2.检测DS18B20设备是否正常*/
	switch (DS18B20_Check_Device()) {
		case 0:	
            printf("DS18B20_Init OK!\n");
            break;
		case 1:
            printf("DS18B20设备响应复位信号失败！\n");
            break;
		case 2:
            printf("DS18B20设备释放总线失败！\n");
            break;
	}
}

/**
  * @brief    向DS18B20写一个字节
  * @param    cmd 要写入的字节
  * @retval   none
*/
static uint8_t DS18B20_Write_Byte(uint8_t cmd)
{
	uint8_t i = 0;
    
    /* 1. 设置总线为输出模式 */
	DS18B20_OutPut_Mode();
    
    /* 2. 发送数据，低位在前 */
	for (i = 0; i < 8; i++) {
		DS18B20_Out(0);
		DS18B20_Delay_us(2);  
		DS18B20_Out(cmd & 0x01);
		DS18B20_Delay_us(60);
		DS18B20_Out(1);
		cmd >>= 1;
		DS18B20_Delay_us(2);
	}
    
	return 0;
}


/**
  * @brief    向DS18B20写一个位
  * @param    cmd 要写入的位
  * @retval   none
*/
static void DS18B20_Write_Bit(uint8_t cmd)
{    
    /* 1. 设置总线为输出模式 */
		DS18B20_OutPut_Mode();
    
    /* 2. 发送数据 */
		DS18B20_Out(0);
		DS18B20_Delay_us(2);  
		DS18B20_Out(cmd);
		DS18B20_Delay_us(60);
		DS18B20_Out(1);
		DS18B20_Delay_us(2);
}
 
/**
  * @brief    从DS18B20读一个字节
  * @param    none
  * @retval   读取到的一个字节数据
*/
uint8_t DS18B20_Read_Byte(void)
{
	uint8_t i = 0;
    uint8_t data = 0;
    
    /* 读取数据 */
	for (i  =0; i < 8; i++)	{
		DS18B20_OutPut_Mode();
		DS18B20_Out(0);  
		DS18B20_Delay_us(2);
		DS18B20_Out(1);
        
		DS18B20_InPut_Mode();
		DS18B20_Delay_us(10);
		data >>= 1 ;
		if (DS18B20_In()) {
            data |= 0x80;
        }
        
		DS18B20_Delay_us(60);
		DS18B20_Out(1);
	}
    
	return data;
}


/**
  * @brief    从DS18B20读一个位
  * @param    none
  * @retval   读取到的一个位数据
*/
uint8_t DS18B20_Read_Bit(void)
{
		uint8_t data=0;
    /* 读取数据 */
		
		DS18B20_OutPut_Mode();
		DS18B20_Out(0);  
		DS18B20_Delay_us(2);
		DS18B20_Out(1);
        
		DS18B20_InPut_Mode();
		DS18B20_Delay_us(10);
		if (DS18B20_In()) {
            data=1;
        }
        
		DS18B20_Delay_us(60);
		DS18B20_Out(1);
				
		return data;
}

/**
  * @brief    跳过匹配 DS18B20 ROM
  * @param    none
  * @retval   none
  * @note     none
*/
void DS18B20_SkipRom (void)
{
	if(DS18B20_Check_Device()==0)
	  DS18B20_Write_Byte(0XCC);                /* 跳过 ROM */        
}


 /**
  * @brief    执行匹配 DS18B20 ROM
  * @param    none
  * @retval   none
  * @note     none
*/
static void DS18B20_MatchRom ()
{
	if(DS18B20_Check_Device()==0)                 
	  DS18B20_Write_Byte(0X55);                /* 匹配 ROM */        
}


/**
  * @brief    从DS18B20读取一次数据(跳过rom检测)
  * @param    none
  * @retval   读取到的温度数据
  * @note     适用于总线上只有一个DS18B20的情况
*/
float DS18B20_GetTemp_SkipRom (void)
{
	uint8_t tpmsb, tplsb;
	short s_tem;
	float f_tem;
	
	
	DS18B20_SkipRom ();
	DS18B20_Write_Byte(0X44);                                /* 开始转换 */
	
	
	DS18B20_SkipRom ();
	DS18B20_Write_Byte(0XBE);                                /* 读温度值 */
	
	tplsb = DS18B20_Read_Byte();
	tpmsb = DS18B20_Read_Byte();
	
	
	s_tem = tpmsb<<8;
	s_tem = s_tem | tplsb;
	
	if( s_tem < 0 )                /* 负温度 */
		f_tem = (~s_tem+1) * 0.0625;        
	else
		f_tem = s_tem * 0.0625;
	
	return f_tem;         
}

/**
  * @brief    在匹配 ROM 情况下获取 DS18B20 温度值
  * @param    ds18b20_id 存放 DS18B20 序列号的数组的首地址(需强制转换位(uint8_t *))
  * @retval   温度值
  * @note     none
*/
float DS18B20_GetTemp_MatchRom ( uint8_t * ds18b20_id)
{
	uint8_t tpmsb, tplsb, i;
	short s_tem;
	float f_tem;
	
	
	DS18B20_MatchRom ();            //匹配ROM
	
	for(i=0;i<8;i++)
		DS18B20_Write_Byte ( ds18b20_id [ i ] );        
	
	DS18B20_Write_Byte(0X44);                                /* 开始转换 */

	
	DS18B20_MatchRom ();            //匹配ROM
	
	for(i=0;i<8;i++)
		DS18B20_Write_Byte ( ds18b20_id [ i ] );        
	
	DS18B20_Write_Byte(0XBE);                                /* 读温度值 */
	
	tplsb = DS18B20_Read_Byte();                 
	tpmsb = DS18B20_Read_Byte(); 
	
	
	s_tem = tpmsb<<8;
	s_tem = s_tem | tplsb;
	
	if( s_tem < 0 )                /* 负温度 */
		f_tem = (~s_tem+1) * 0.0625;        
	else
		f_tem = s_tem * 0.0625;
	
	return f_tem;                 
}


/**
  * @brief    单总线搜索算法
  * @param    none
  * @retval   1 成功搜索到设备，并且把设备地址存入ROM_NO
  * @retval   0 未搜索到设备
  * @note     具体原理看文档 1-Wire搜索算法.pdf
*/
int DS18B20_Search(void)
{
   int id_bit_number;
   int last_zero, rom_byte_number, search_result;
   int id_bit, cmp_id_bit;
   unsigned char rom_byte_mask, search_direction;

   // 搜索初始化
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = 0;
   crc8 = 0;

   if (!LastDeviceFlag)
   {
      // 复位
		 if (DS18B20_Check_Device()!=0) //复位失败
      {
         // 复位搜索
         LastDiscrepancy = 0;
         LastDeviceFlag = FALSE;
         LastFamilyDiscrepancy = 0;

         return FALSE;
      }

      // 发送搜索指令
      DS18B20_Write_Byte(0xF0);  

      // 循环搜索
      do
      {
         // 读入一个位以及其补位 
         id_bit = DS18B20_Read_Bit();
         cmp_id_bit = DS18B20_Read_Bit();
				
         // 两个都是1，说明无设备在总线上
         if ((id_bit == 1) && (cmp_id_bit == 1))
         {
					 break;
         } 
         else
         {
            // 当前搜索的这个位，0和1都有
            if (id_bit != cmp_id_bit)
               search_direction = id_bit;  // 设置搜索方向为id_bit
            else
            {
               if (id_bit_number < LastDiscrepancy)
                  search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
               else
                  search_direction = (id_bit_number == LastDiscrepancy);

               if (search_direction == 0)
               {
                  last_zero = id_bit_number;
                  if (last_zero < 9)
                     LastFamilyDiscrepancy = last_zero;
               }
            }

            
            if (search_direction == 1)
              ROM_NO[rom_byte_number] |= rom_byte_mask;
            else
              ROM_NO[rom_byte_number] &= ~rom_byte_mask;

            // 写入搜索方向 此时器件会响应，如果搜索方向与当前位不匹配则自动进入不响应状态
						DS18B20_Write_Bit(search_direction);
            
            id_bit_number++;
            rom_byte_mask <<= 1;

            if (rom_byte_mask == 0)
            {
                docrc8(ROM_NO[rom_byte_number]);  // 计算crc
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  

      
      if (!((id_bit_number < 65) || (crc8 != 0))) // 搜索成功
      {
         // 搜索成功设置标志位
         LastDiscrepancy = last_zero;

         // 检查是否为最后一个器件
         if (LastDiscrepancy == 0)
            LastDeviceFlag = TRUE;
         
         search_result = TRUE;
      }
   }

   // 未搜索到设备，所有标志位置位
   if (!search_result || !ROM_NO[0])
   {
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      search_result = FALSE;
   }
	 	 
   return search_result;
}


/**
  * @brief    搜索第一个设备
  * @param    none
  * @retval   1 成功搜索到设备，并且把设备地址存入ROM_NO
  * @retval   0 未搜索到设备
*/
int DS18B20_FirstID(void)
{
   LastDiscrepancy = 0;
   LastDeviceFlag = FALSE;
   LastFamilyDiscrepancy = 0;

   return DS18B20_Search();
}


/**
  * @brief    搜索下一个设备
  * @param    none
  * @retval   1 成功搜索到设备，并且把设备地址存入ROM_NO
  * @retval   0 未搜索到设备
*/
int DS18B20_NextID(void)
{
   return DS18B20_Search();
}


/**
  * @brief    搜索总线上的所有设备
  * @param    Ds18b20_ID 存放 DS18B20 序列号的数组的首地址
  * @retval   cnt 搜索到的设备数量
  * @retval   0 未搜索到设备
*/
int DS18B20_Search_AllID(uint64_t * Ds18b20_ID)
{
  int cnt = 0;
	int rslt = 0;
	
	rslt = DS18B20_FirstID();
	while (rslt)
	{
//		for (int i = 7; i >= 0; i--)
//				printf("%02X", ROM_NO[i]);
		memcpy((uint8_t *)&Ds18b20_ID[cnt],ROM_NO,8);
		cnt++;
		rslt = DS18B20_NextID();
	}
	return cnt;
}
