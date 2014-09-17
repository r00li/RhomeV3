/*
 * RemoteSwitch library v2.2.1 (20120314) made by Randy Simons http://randysimons.nl/
 *
 * License: GPLv3. See license.txt
 */

  /*
  * ARM port by Andrej Rolih (2014)
  * http://www.r00li.com
  */

#ifndef RemoteReceiver_h
#define RemoteReceiver_h


#include "essentials.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "misc.h"
#include "stm32f4xx_syscfg.h"
#include "stm32f4xx.h"

typedef void (*RemoteReceiverCallBack)(unsigned long, unsigned int);

/**
* See RemoteSwitch for introduction.
*
* RemoteReceiver decodes the signal received from a 433MHz-receiver, like the "KlikAanKlikUit"-system
* as well as the signal sent by the RemoteSwtich class. When a correct signal is received,
* a user-defined callback function is called.
*
* Note that in the callback function, the interrupts are still disabled. You can enabled them, if needed.
* A call to the callback must b finished before RemoteReceiver will call the callback function again, thus
* there is no re-entrant problem.
*
* When sending your own code using RemoteSwich, disable() the receiver first.
*
* This is a pure static class, for simplicity and to limit memory-use.
*/

class RemoteReceiver {
	public:
		/**
		* Initializes the decoder.
		*
		* If interrupt >= 0, init will register pin <interrupt> to this library.
		* If interrupt < 0, no interrupt is registered. In that case, you have to call interruptHandler()
		* yourself whenever the output of the receiver changes, or you can use InterruptChain.
		*
		* @param interrupt 	The interrupt as is used by Arduino's attachInterrupt function. See attachInterrupt for details.
							If < 0, you must call interruptHandler() yourself.
		* @param minRepeats The number of times the same code must be received in a row before the callback is calles
		* @param callback Pointer to a callback function, with signature void (*func)(unsigned long, unsigned int). First parameter is the decoded data, the second the period of the timing.
		*/
		static void init(unsigned short minRepeats, RemoteReceiverCallBack callback);

		/**
		* Enable decoding. No need to call enable() after init().
		*/
		static void enable();

		/**
		* Disable decoding. You can re-enable decoding by calling enable();
		*/
		static void disable();

		/**
		* Tells wether a signal is being received. If a compatible signal is detected within the time out, isReceiving returns true.
		* Since it makes no sense to transmit while another transmitter is active, it's best to wait for isReceiving() to false.
		* By default it waits for 150ms, in which a (relative slow) KaKu signal can be broadcasted three times.
		*
		* Note: isReceiving() depends on interrupts enabled. Thus, when disabled()'ed, or when interrupts are disabled (as is
		* the case in the callback), isReceiving() will not work properly.
		*
		* @param waitMillis number of milliseconds to monitor for signal.
		* @return boolean If after waitMillis no signal was being processed, returns false. If before expiration a signal was being processed, returns true.
		*/
		static bool isReceiving(int waitMillis = 150);

		static void interruptHandler();

	private:

		static int8_t _interrupt;					// Radio input interrupt
		volatile static int8_t _state;				// State of decoding process. There are 49 states, 1 for "waiting for signal" and 48 for decoding the 48 edges in a valid code.
		static uint8_t _minRepeats;
		static RemoteReceiverCallBack _callback;
		static bool _inCallback;					// When true, the callback function is being executed; prevents re-entrance.
		static bool _enabled;					// If true, monitoring and decoding is enabled. If false, interruptHandler will return immediately.


};


#endif
