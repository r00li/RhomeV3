/*
**
**                           Remote.cpp
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

#include "Remote.h"


const char* RemoteButton::getEventTypeName()
{
    if (eventType == TYPE_BLIND_TOGGLE)
        return typeNames[1];
    else if (eventType == TYPE_ACTION)
        return typeNames[2];
    else
        return typeNames[0];
}


volatile uint32_t RemoteButton::remoteCodePressed;
volatile bool RemoteButton::shouldRunActions;
