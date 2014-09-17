/*
**
**                           Lighting.cpp
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

#include "Lighting.h"
#include "RemoteReceiver.h"

void Light::setTypeKaku(char address, unsigned short device)
{
    this->type = RF_KAKU;
    this->device = address;
    this->systemCode = device;

    KaKuTransmitter* transmitter = new KaKuTransmitter();
    this->rfTransmitter = transmitter;
}

void Light::setTypeElro(unsigned short systemCode, char device)
{

}

void Light::setTypeBlokker(unsigned short device)
{

}

void Light::setTypeAction(unsigned short systemCode, char device)
{

}

void Light::setName(const char* nm)
{
    strncpy(name, nm, 11);
    name[11] = '\0';
}

const char* Light::getName()
{
    return name;
}

LightType Light::getType()
{
    return this->type;
}

bool Light::isOn()
{
    return this->on;
}

void Light::onOff(bool on)
{
    switch (this->type)
    {
    case RF_KAKU:
        taskENTER_CRITICAL();
        RemoteReceiver::disable();
        ((KaKuTransmitter *)rfTransmitter)->sendSignal(device, systemCode, on);
        this->on = on;
        RemoteReceiver::enable();
        taskEXIT_CRITICAL();
        break;
    }
}

uint32_t Light::calculateHash()
{
    uint32_t hsh = 0;
    for (int i = 0; i < strlen(name); i++)
    {
        hsh += name[i];
    }

    hsh += device;
    hsh += systemCode;
    hsh += btCode;

    return hsh;
}

