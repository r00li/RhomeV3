/*
**
**                           TempSensor.h
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

#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H

#include "stm32f4xx_conf.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx.h"


class InternalTempSensor
{
    private:
    float temp;

    public:
    float calibration;
    void initTempSensor();
    float getTemp(bool calibrate = true);
    void measureTemp(bool calibrate = true);
};


#endif /* TEMPSENSOR_H */
