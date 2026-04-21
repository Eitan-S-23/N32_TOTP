/*****************************************************************************
 * Copyright (c) 2019, Nations Technologies Inc.
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Nations' name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY NATIONS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL NATIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file xpt2046.c
 * @author Nations Firmware Team
 * @version v1.0.4
 *
 * @copyright Copyright (c) 2019, Nations Technologies Inc. All rights reserved.
 */

#include "xpt2046.h"

/** @addtogroup XPT2046
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
volatile uint8_t key = 0;
volatile uint8_t key_flag = 0;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Configures key port.
 */
void app_key_configuration(void)
{
    GPIO_InitType GPIO_InitStructure;
    EXTI_InitType EXTI_InitStructure;
    NVIC_InitType NVIC_InitStructure;

    /* Enable the GPIO Clock */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);

    /*Configure the GPIO pin as input floating */
    GPIO_InitStruct(&GPIO_InitStructure);
    GPIO_InitStructure.Pin          = KEY1_INPUT_PIN;
    GPIO_InitStructure.GPIO_Pull    = GPIO_PULL_UP;
    GPIO_InitPeripheral(KEY1_INPUT_PORT, &GPIO_InitStructure);

    /* Enable the GPIO Clock */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_AFIO, ENABLE);

    /*Configure the GPIO pin as input floating*/
    GPIO_InitStruct(&GPIO_InitStructure);
    GPIO_InitStructure.Pin          = KEY2_INPUT_PIN;
    GPIO_InitStructure.GPIO_Pull    = GPIO_PULL_UP;
    GPIO_InitPeripheral(KEY2_INPUT_PORT, &GPIO_InitStructure);
		
		/*Configure the GPIO pin as input floating*/
    GPIO_InitStruct(&GPIO_InitStructure);
    GPIO_InitStructure.Pin          = KEY3_INPUT_PIN;
    GPIO_InitStructure.GPIO_Pull    = GPIO_PULL_UP;
    GPIO_InitPeripheral(KEY3_INPUT_PORT, &GPIO_InitStructure);


    /*Configure key EXTI Line to key input Pin*/
    GPIO_ConfigEXTILine(KEY1_INPUT_PORT_SOURCE, KEY1_INPUT_PIN_SOURCE);
    GPIO_ConfigEXTILine(KEY2_INPUT_PORT_SOURCE, KEY2_INPUT_PIN_SOURCE);
		GPIO_ConfigEXTILine(KEY3_INPUT_PORT_SOURCE, KEY3_INPUT_PIN_SOURCE);

    /*Configure key EXTI line*/
    EXTI_InitStructure.EXTI_Line    = KEY1_INPUT_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling; 
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&EXTI_InitStructure);

    /*Set key input interrupt priority*/
    NVIC_InitStructure.NVIC_IRQChannel                   = KEY1_INPUT_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
        
    /*Configure key EXTI line*/
    EXTI_InitStructure.EXTI_Line    = KEY2_INPUT_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling; 
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&EXTI_InitStructure);

    /*Set key input interrupt priority*/
    NVIC_InitStructure.NVIC_IRQChannel                   = KEY2_INPUT_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);        
    
		
		/*Configure key EXTI line*/
    EXTI_InitStructure.EXTI_Line    = KEY3_INPUT_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling; 
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&EXTI_InitStructure);

    /*Set key input interrupt priority*/
    NVIC_InitStructure.NVIC_IRQChannel                   = KEY3_INPUT_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);        
}

bool XPT2046IsPress(void)
{
	if(!GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_11) || !GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_12) || !GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_13))
	{
		return true;
	}
	else 
	{
		return false;
	}

}

int XPT2046ReadY(void)
{
	if(!GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_12))
	{
		return 1;
	}
	else if(!GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_13))
	{
		return -1;
	}
	return 0;
}

int XPT2046ReadX(void)
{
//	if(!GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_12))
//	{
//		return 200;
//	}
//	else if(!GPIO_ReadInputDataBit(GPIOB,GPIO_PIN_13))
//	{
//		return -200;
//	}
	return 50;
}

/**
 * @brief  External lines 1 interrupt.
 */
void EXTI4_12_IRQHandler(void)
{ 
    if (EXTI_GetITStatus(KEY1_INPUT_EXTI_LINE) != RESET)
		{
				// 1. 先延时10ms，等待抖动消失（此时不清除中断标志）
				delay_n_10us(1000);
				
				// 2. 延时后检测实际电平：若仍为按下状态（假设低电平有效）
				if (GPIO_ReadInputDataBit(KEY1_INPUT_PORT,KEY1_INPUT_PIN) == 0)  // 电平稳定，确认有效按下
				{
						key_flag++;
					  if(key_flag > 2)
							key_flag = 0;
						key = 1;
				}
				
				EXTI_ClrITPendBit(KEY1_INPUT_EXTI_LINE);
		}

		if (EXTI_GetITStatus(KEY2_INPUT_EXTI_LINE) != RESET)
		{
				delay_n_10us(1000);
				if (GPIO_ReadInputDataBit(KEY2_INPUT_PORT,KEY2_INPUT_PIN) == 0)  // 确认稳定按下
				{
						if(key_flag == 0)
								key = 2;
						else
								key = 4;
				}
				EXTI_ClrITPendBit(KEY2_INPUT_EXTI_LINE);
		}

		if (EXTI_GetITStatus(KEY3_INPUT_EXTI_LINE) != RESET)
		{
				delay_n_10us(1000);
				if (GPIO_ReadInputDataBit(KEY3_INPUT_PORT,KEY3_INPUT_PIN) == 0)  // 确认稳定按下
				{
						if(key_flag == 0)
								key = 3;
						else
								key = 5;
				}
				EXTI_ClrITPendBit(KEY3_INPUT_EXTI_LINE);
		}
}

/**
 * @}
 */
