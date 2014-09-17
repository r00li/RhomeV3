#ifndef BLINDS_H
#define BLINDS_H

#include "stm32f4xx_exti.h"
/*
**
**                           Blinds.h
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

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "misc.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_syscfg.h"
#include "stm32f4xx.h"
#include <string.h>

const char allBlindTypes[][11] = {"Local"/*, "Bluetooth"*/};
#define kAllBlindTypes 1

const char allBlindChannels[][11] = {"1", "2", "3", "4"};
#define kAllBlindChannels 4

#define kBlindStateMin 0
#define kBlindStateMid 1
#define kBlindStateMax 2

const char autoBlindPositions[][11] = {"Min", "Mid", "Max"};
#define kAutoBLindPositions 3

typedef enum
{
    BLIND_LOCAL,
    BLIND_BLE,
    BLIND_RF
} BlindType;

typedef enum
{
    BLIND_CH1,
    BLIND_CH2,
    BLIND_CH3,
    BLIND_CH4
} BlindChannel;

class Blind
{
    private:
    int currPosition;
    int currPositionState;
    char name[12];

    public:
    BlindChannel channel;
    BlindType type;
    void setBounds(int boundMin, int boundMid, int boundMax);
    int minPosition;
    int maxPosition;
    int midPosition;
    int step;
    int btCode;

    BlindType getType();
    void setType(BlindType type);
    void setChannel(BlindChannel ch);
    int getPosition(bool read = false);
    void setPosition(int pos);
    void enableOutput(bool en);
    void setName(const char* nm);
    const char* getName();
    void toggleState(int newState = -1);
    int getState();

    static void initLocalBlinds();

    uint32_t calculateHash();
};

#endif /* BLINDS_H */
