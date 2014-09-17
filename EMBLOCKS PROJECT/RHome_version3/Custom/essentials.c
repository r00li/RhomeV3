/*
**
**                           essentials.c
**
**
**********************************************************************/
/*
   Author:                 Andrej Rolih
                           www.r00li.com
   Version:                0.1
   License:                GNU GPL v3
                           See attached LICENSE file for details
                           External library files do not include such header and
                           are released under GPL v3 or their specific license.
                           Check those files for more details.

**********************************************************************/

#include "essentials.h"





//******************************************************************************************
//TIMER FUNCTIONS

void init_TIM4()
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = UINT32_MAX;
    TIM_TimeBaseStructure.TIM_Prescaler = (84000000 / 1000000) -1; //(SYS_CLK / DELAY_TIM_FREQUENCY) - 1 -> delay time is 1MHz = 1us
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV2;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    //Disable timer if it is not needed
    TIM_Cmd(TIM2,ENABLE);
}


void init_nvic_tim4()
{
}

/*
void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update))
    {
        micros++;
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }
}
*/

void delay(uint32_t ms)
{
    //
    // Call the delay_micro with the correct value
    //

    delay_micro(ms * 1000);
}

void delay_micro(uint32_t micro)
{
    //
    // Reset the number of micros that have passed, enable timer, wait for the specified amount of microseconds and disable timer
    //

  uint32_t start = TIM_GetCounter(TIM2);

  /* use 16 bit count wrap around */
  while ((uint32_t)(TIM_GetCounter(TIM2) - start) <= micro) {}
}

unsigned int millis()
{
    return TIM_GetCounter(TIM2)/1000;
}

unsigned int micros()
{
    return TIM_GetCounter(TIM2);
}
/*

volatile unsigned long micros;


//funkcija inicializira casovnik 4 za potrebe delay
void init_TIM4()
{
    // Deklaracija pod. struktur za casovnik
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    // Nastavitev casovne baze:
    // Naj bo perioda stetja 1us.

    //vklopimo APB1 uro za TIM4
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    //osnovne nastavitve za ?tevec (perioda in na?in ?tetja)

    TIM_TimeBaseStructure.TIM_Period = 83; // TIM4_ARR
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV2;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    //Disable timer if it is not needed
    TIM_Cmd(TIM4,DISABLE);
}


void init_nvic_tim4()
{
    //Enable interrupts for TIM4
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
}


void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update))
    {
        micros++;
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }
}


void delay(unsigned int ms)
{
    //
    // Call the delay_micro with the correct value
    //

    delay_micro(ms * 1000);
}

void delay_micro(unsigned int micro)
{
    //
    // Reset the number of micros that have passed, enable timer, wait for the specified amount of microseconds and disable timer
    //

    micros = 0;
    TIM_Cmd(TIM4,ENABLE);

    unsigned int end_t = micros+micro;
    while(micros < end_t) { }

    TIM_Cmd(TIM4,DISABLE);
}

*/
void init_essentials_time()
{
    init_TIM4();
    init_nvic_tim4();
}
