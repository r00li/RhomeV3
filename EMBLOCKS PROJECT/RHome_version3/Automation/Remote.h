/*
**
**                           Remote.h
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

#ifndef REMOTE_H_INCLUDED
#define REMOTE_H_INCLUDED

#include "stm32f4xx.h"

const char typeNames[][8] = {"Light", "Blind", "Action"};

typedef enum
{
    TYPE_LIGHT_ON,
    TYPE_LIGHT_OFF,
    TYPE_LIGHT_TOGGLE,
    TYPE_BLIND_TOGGLE,
    TYPE_ACTION
} RemoteEventType;

class RemoteButton
{
    public:
    uint32_t remoteButton;
    uint32_t eventHash;
    RemoteEventType eventType;
    const char* getEventTypeName();

    static volatile uint32_t remoteCodePressed;
    static volatile bool shouldRunActions;
};


#endif /* REMOTE_H_INCLUDED */
