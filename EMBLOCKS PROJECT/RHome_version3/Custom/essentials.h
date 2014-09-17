/*
**
**                           essentials.h
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

#ifndef essentials_h
#define essentials_h

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

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_spi.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"
#include "misc.h"




////////////////////////////////////////////////////
/////  PRIVATE
////////////////////////////////////////////////////

////////////////////////////////////////////////////

void init_essentials_time();
void delay(uint32_t ms);
void delay_micro(uint32_t micro);

unsigned int millis();
unsigned int micros();


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
