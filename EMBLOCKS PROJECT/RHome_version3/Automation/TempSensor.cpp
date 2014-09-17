/*
**
**                           TempSensor.cpp
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

#include "TempSensor.h"

#include "FreeRTOS.h"
#include "task.h"

void InternalTempSensor::initTempSensor()
{
    temp = 0;
    calibration = 0;

    ADC_DeInit();

    ADC_InitTypeDef ADC_InitStruct;
    ADC_CommonInitTypeDef ADC_CommonInitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div8;
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStruct);

    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStruct);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_TempSensor, 1, ADC_SampleTime_144Cycles);
    ADC_TempSensorVrefintCmd(ENABLE);
    ADC_Cmd(ADC1, ENABLE);
}

void InternalTempSensor::measureTemp(bool calibrate)
{
    // ADC Conversion to read temperature sensor
    // Temperature (in °C) = ((Vsense – V25) / Avg_Slope) + 25
    // Vense = Voltage Reading From Temperature Sensor
    // V25 = Voltage at 25°C, for STM32F407 = 0.76V
    // Avg_Slope = 2.5mV/°C
    // This data can be found in the STM32F407VF Data Sheet


    taskENTER_CRITICAL();
    ADC_SoftwareStartConv(ADC1); //Start the conversion
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {} ; //Processing the conversion

    temp = ADC_GetConversionValue(ADC1); //Return the converted data
    temp *= 3300;
    temp /= 0xfff; //Reading in mV
    temp /= 1000.0; //Reading in Volts
    temp -= 0.760; // Subtract the reference voltage at 25°C
    temp /= .0025; // Divide by slope 2.5mV
    temp += 25.0; // Add the 25°C

    if (calibrate)
    {
        temp -= calibration;
    }

    taskEXIT_CRITICAL();
}

float InternalTempSensor::getTemp(bool calibrate)
{
    measureTemp(calibrate);
    float t1 = temp;
    measureTemp(calibrate);
    float t2 = temp;
    measureTemp(calibrate);
    float t3 = temp;
    measureTemp(calibrate);
    float t4 = temp;

    temp = (t1+t2+t3+t4)/4.0;

    return temp;
}
