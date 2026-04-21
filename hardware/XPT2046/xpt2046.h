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
 * @file xpt2046.h
 * @author Nations Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2019, Nations Technologies Inc. All rights reserved.
 */
#ifndef __XPT2046_H__
#define __XPT2046_H__

#include "n32wb03x.h"

#define KEY1_INPUT_PORT        GPIOB
#define KEY1_INPUT_PIN         GPIO_PIN_11
#define KEY1_INPUT_EXTI_LINE   EXTI_LINE5
#define KEY1_INPUT_PORT_SOURCE GPIOB_PORT_SOURCE
#define KEY1_INPUT_PIN_SOURCE  GPIO_PIN_SOURCE11
#define KEY1_INPUT_IRQn        EXTI4_12_IRQn

#define KEY2_INPUT_PORT        GPIOB
#define KEY2_INPUT_PIN         GPIO_PIN_12
#define KEY2_INPUT_EXTI_LINE   EXTI_LINE6
#define KEY2_INPUT_PORT_SOURCE GPIOB_PORT_SOURCE
#define KEY2_INPUT_PIN_SOURCE  GPIO_PIN_SOURCE12
#define KEY2_INPUT_IRQn        EXTI4_12_IRQn

#define KEY3_INPUT_PORT        GPIOB
#define KEY3_INPUT_PIN         GPIO_PIN_13
#define KEY3_INPUT_EXTI_LINE   EXTI_LINE7
#define KEY3_INPUT_PORT_SOURCE GPIOB_PORT_SOURCE
#define KEY3_INPUT_PIN_SOURCE  GPIO_PIN_SOURCE13
#define KEY3_INPUT_IRQn        EXTI4_12_IRQn


void app_key_configuration(void);
bool XPT2046IsPress(void);
int XPT2046ReadY(void);
int XPT2046ReadX(void);

#endif /* __XPT2046_H__ */
