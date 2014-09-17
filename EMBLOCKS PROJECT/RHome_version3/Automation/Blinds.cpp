/*
**
**                           Blinds.cpp
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

#include "Blinds.h"
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

void Blind::setType(BlindType type)
{
    this->type = type;
}

BlindType Blind::getType()
{
    return this->type;
}

void Blind::setBounds(int boundMin, int boundMid, int boundMax)
{
    if (boundMin < boundMid)
    {
        if (boundMax < boundMin)
        {
            int temp = boundMin;
            boundMin = boundMax;
            boundMax = temp;
        }
    }
    else
    {
        if (boundMid < boundMax)
        {
            int temp = boundMin;
            boundMin = boundMid;
            boundMid = temp;
        }
        else
        {
            int temp = boundMin;
            boundMin = boundMax;
            boundMax = temp;
        }
    }
    if(boundMax < boundMid)
    {
        int temp = boundMid;
        boundMid = boundMax;
        boundMax = temp;
    }

    minPosition = boundMin;
    maxPosition = boundMax;
    midPosition = boundMid;

    step = abs(boundMax-boundMin)/30;
}

int Blind::getPosition(bool read)
{
    return currPosition;
}

void Blind::enableOutput(bool en)
{
    if (type == BLIND_LOCAL)
    {
        switch (channel)
        {
        case BLIND_CH1:
            TIM_CCxCmd(TIM3, TIM_Channel_1, (en) ? ENABLE : DISABLE);
            break;
        case BLIND_CH2:
            TIM_CCxCmd(TIM3, TIM_Channel_2, (en) ? ENABLE : DISABLE);
            break;
        case BLIND_CH3:
            TIM_CCxCmd(TIM3, TIM_Channel_3, (en) ? ENABLE : DISABLE);
            break;
        case BLIND_CH4:
            TIM_CCxCmd(TIM3, TIM_Channel_4, (en) ? ENABLE : DISABLE);
            break;
        }
    }
}

void Blind::setPosition(int pos)
{
    if (pos >= maxPosition)
        pos = maxPosition;
    else if (pos <= minPosition)
        pos = minPosition;

    currPosition = pos;

    if (type == BLIND_LOCAL)
    {
        switch (channel)
        {
        case BLIND_CH1:
            TIM_SetCompare1(TIM3, pos);
            break;
        case BLIND_CH2:
            TIM_SetCompare2(TIM3, pos);
            break;
        case BLIND_CH3:
            TIM_SetCompare3(TIM3, pos);
            break;
        case BLIND_CH4:
            TIM_SetCompare4(TIM3, pos);
            break;
        }
    }

    int lowestDiff = -1;
    if (abs(currPosition-minPosition) < abs(currPosition-midPosition) && abs(currPosition-minPosition) < abs(currPosition-maxPosition))
    {
        lowestDiff = minPosition;
        currPositionState = kBlindStateMin;
    }
    else if (abs(currPosition-midPosition) < abs(currPosition-minPosition) && abs(currPosition-midPosition) < abs(currPosition-maxPosition))
    {
        lowestDiff = midPosition;
        currPositionState = kBlindStateMid;
    }
    else
    {
        lowestDiff = maxPosition;
        currPositionState = kBlindStateMax;
    }
}

void Blind::toggleState(int newState)
{
    if (newState == -1)
    {
        newState = (currPositionState == kBlindStateMid)? kBlindStateMax : (currPositionState == kBlindStateMax)? kBlindStateMin : kBlindStateMid;
    }

    enableOutput(true);

    if (newState == kBlindStateMin)
    {
        setPosition(minPosition);
    }
    else if (newState == kBlindStateMid)
    {
        setPosition(midPosition);
    }
    else
    {
        setPosition(maxPosition);
    }

    vTaskDelay(7000 / portTICK_RATE_MS);

    enableOutput(false);
}

int Blind::getState()
{
    return currPositionState;
}


void Blind::setChannel(BlindChannel ch)
{
    channel = ch;
}

void Blind::initLocalBlinds()
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    //Set alternate functions for all pins to TIM3
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_TIM3);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = 20000;
    TIM_TimeBaseStructure.TIM_Prescaler = 84;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV2;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1500;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);
    TIM_OC4Init(TIM3, &TIM_OCInitStructure);

    TIM_CCxCmd(TIM3, TIM_Channel_1, DISABLE);
    TIM_CCxCmd(TIM3, TIM_Channel_2, DISABLE);
    TIM_CCxCmd(TIM3, TIM_Channel_3, DISABLE);
    TIM_CCxCmd(TIM3, TIM_Channel_4, DISABLE);

    TIM_Cmd(TIM3,ENABLE);
}

void Blind::setName(const char* nm)
{
    strncpy(name, nm, 11);
    name[11] = '\0';
}

const char* Blind::getName()
{
    return name;
}

uint32_t Blind::calculateHash()
{
    uint32_t hsh = 0;
    for (int i = 0; i < strlen(name); i++)
    {
        hsh += name[i];
    }

    hsh += type;
    hsh += channel;
    hsh += minPosition;

    return hsh;
}
