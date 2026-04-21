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
 * @file n32wb03x_it.c
 * @author Nations Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2019, Nations Technologies Inc. All rights reserved.
 */
#include "n32wb03x_it.h"
#include "n32wb03x.h"
#include "n32wb03x_rtc.h"
#include "n32wb03x_exti.h"
#include "app_rtc.h"

extern volatile uint8_t spi_dma_tx_complete;
extern volatile uint32_t system_tick;  // 全局tick计数器，由中断更新
/** @addtogroup N32WB03X_StdPeriph_Template
 * @{
 */

/******************************************************************************/
/*            Cortex-M0 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief  This function handles NMI exception.
 */
void NMI_Handler(void)
{
}

/**
 * @brief  This function handles Hard Fault exception.
 */
void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
    }
}

/**
 * @brief  This function handles SVCall exception.
 */
void SVC_Handler(void)
{
}

/**
 * @brief  This function handles PendSV_Handler exception.
 */
void PendSV_Handler(void)
{
}

/**
 * @brief  This function handles SysTick Handler.
 */
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 N32WB03X Peripherals Interrupt Handlers                     */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_n32wb03x.s).                                                 */
/******************************************************************************/

void DMA_Channel1_2_3_4_IRQHandler(void)
{
	// 检查DMA通道3传输完成中断
	if (DMA_GetIntStatus(DMA_INT_TXC3, DMA) != RESET)
	{
		spi_dma_tx_complete ++;
		DMA_ClrIntPendingBit(DMA_INT_TXC3, DMA);
		// 禁用DMA通道
		DMA_EnableChannel(DMA_CH3, DISABLE);

		// 禁用DMA中断
		DMA_ConfigInt(DMA_CH3, DMA_INT_TXC, DISABLE);
	}
	

//	// 检查DMA通道1传输完成中断（如果需要的话）
//	if (DMA_GetIntStatus(DMA_INT_TXC1, DMA) != RESET)
//	{
//		DMA_ClrIntPendingBit(DMA_INT_TXC1, DMA);
//	}

//	// 检查DMA通道2传输完成中断（如果需要的话）
//	if (DMA_GetIntStatus(DMA_INT_TXC2, DMA) != RESET)
//	{
//		DMA_ClrIntPendingBit(DMA_INT_TXC2, DMA);
//	}

//	// 检查DMA通道4传输完成中断（如果需要的话）
//	if (DMA_GetIntStatus(DMA_INT_TXC4, DMA) != RESET)
//	{
//		DMA_ClrIntPendingBit(DMA_INT_TXC4, DMA);
//	}
}

/**
 * @brief  This function handles TIM3 global interrupt request.
 */
void TIM3_IRQHandler(void)
{
    if (TIM_GetIntStatus(TIM3, TIM_INT_UPDATE) != RESET)
    {
				system_tick++;
        TIM_ClrIntPendingBit(TIM3, TIM_INT_UPDATE);
    }
}

/**
 * @brief  This function handles RTC wake-up interrupt request.
 *         Bumps the epoch-seconds counter each 1 Hz CK_SPRE tick.
 */
void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_INT_WUT) != RESET)
    {
        app_rtc_on_wakeup();
        RTC_ClrIntPendingBit(RTC_INT_WUT);
        EXTI_ClrITPendBit(EXTI_LINE9);
    }
}


/**
 * @brief  This function handles PPP interrupt request.
 */
/*void PPP_IRQHandler(void)
{
}*/

/**
 * @}
 */
