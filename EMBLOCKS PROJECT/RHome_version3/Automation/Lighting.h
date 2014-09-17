/*
**
**                           Lighting.h
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

#ifndef LIGHTING_H
#define LIGHTING_H

//
// C++ and C support
//
#ifdef __cplusplus
extern "C"
{
#endif
//
// ------------------------------------------------------------------------------------------------------------
// CODE GOES HERE
//

#include "RemoteTransmitter.h"
#include "RemoteReceiver.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

const char allLightTypes[][11] = {"KakuSwitch", "Elro RM", "Blokker RM", "Action RM"/*, "Bluetooth"*/};
#define kAllLightTypes 4

const char lightAddresses1[][11] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P"};
#define kLightAddresses1 16

const char lightAddresses2[][11] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19"};
#define kLightAddresses2 19

typedef enum
{
    RF_KAKU,
    RF_ELRO,
    RF_BLOKKER,
    RF_ACTION,
    BLUETOOTH,
    IR_LED_ANALOG
} LightType;

class Light
{
    private:
    LightType type;
    bool on;
    RemoteTransmitter* rfTransmitter;
    char name[12];

    public:
    unsigned short systemCode;
    char device;
    uint32_t btCode;
    void setTypeKaku(char address, unsigned short device);
    void setTypeElro(unsigned short systemCode, char device);
    void setTypeBlokker(unsigned short device);
    void setTypeAction(unsigned short systemCode, char device);

    void setName(const char* nm);
    const char* getName();

    LightType getType();
    void onOff(bool on);
    bool isOn();

    uint32_t calculateHash();
};


//
// END CODE
// ------------------------------------------------------------------------------------------------------------
// C++ and C support
//
#ifdef __cplusplus
}
#endif
//
//
//

#endif
