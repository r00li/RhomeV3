/*
 * RemoteSwitch library v2.2.1 (20120314) made by Randy Simons http://randysimons.nl/
 *
 * License: GPLv3. See license.txt
 */

 /*
  * ARM port by Andrej Rolih (2014)
  * http://www.r00li.com
  */

#ifndef RemoteTransmitter_h
#define RemoteTransmitter_h

//
// C++ and C support
//
#ifdef __cplusplus
extern "C"
{
#endif

#include "essentials.h"

#define RF_Transmit_Port			GPIOA
#define RF_Transmit_Pin				GPIO_Pin_15


/**
* RemoteTransmitter provides a generic class for simulation of common RF remote controls, like the 'Klik aan Klik uit'-system
* (http://www.klikaanklikuit.nl/), used to remotely switch lights etc.
*
* Many of these remotes seem to use a 433MHz SAW resonator and one of these chips: LP801B,  HX2262, PT2262, M3E.
* Datasheet for the HX2262/PT2262 ICs:
* http://www.princeton.com.tw/downloadprocess/downloadfile.asp?mydownload=PT2262_1.pdf
*
* Hardware required for this library: a 433MHz/434MHz SAW oscillator transmitter, e.g.
* http://www.sparkfun.com/products/10534
* http://www.conrad.nl/goto/?product=130428
*
* Notes:
* - Since these chips use (and send!) tri-state inputs (low, high and floating) I use 'trits' instead of 'bits',
*   when appropriate.
* - I measured the period lengths with a scope.  Thus: they work for my remotes, but may fail for yours...
*   A better way would be to calculate the 'target'-timings using the datasheets and the resistor-values on the remotes.
*/
class RemoteTransmitter {
    private:
        void initGPIO();
	public:
		/**
		* Constructor.
		*
		* To obtain the correct period length, an oscilloscope is convenient, but you can also read the
		* datasheet of the transmitter, measure the resistor for the oscillator and calculate the freqency.
		*
		* @param pin		output pin on Arduino to which the transmitter is connected
		* @param periodusec	[0..511] Duration of one period, in microseconds. A trit is 6 periods.
		* @param repeats	[0..7] The 2log-Number of times the signal is repeated. The actual number of repeats will be 2^repeats. 2 would be bare minimum, 4 seems robust.
		*/
		//RemoteTransmitter(unsigned short pin, unsigned int periodusec, unsigned short repeats);
		RemoteTransmitter(unsigned int periodusec, unsigned short repeats);

		/**
		* Encodes the data base on the current object and the given trits. The data can be reused, e.g.
		* for use with the static version of sendTelegram, so you won't need to instantiate costly objects!
		*
		* @return The data suited for use with RemoteTransmitter::sendTelegram.
		*/
		unsigned long encodeTelegram(unsigned short trits[]);

		/**
		* Send a telegram, including synchronisation-part.
		*
		* @param trits	Array of size 12. "trits" should be either 0, 1 or 2, where 2 indicaties "float"
		*/
		void sendTelegram(unsigned short trits[]);

		/**
		* Send a telegram, including synchronisation-part. The data-param encodes the period duration, number of repeats and the actual data.
		* Note: static method, which allows for use in low-mem situations.
		*
		* Format data:
		* pppppppp|prrrdddd|dddddddd|dddddddd (32 bit)
		* p = period (9 bit unsigned int
		* r = repeats as 2log. Thus, if r = 3, then signal is sent 2^3=8 times
		* d = data
		*
		* @param data data, period and repeats.
		* @param pin Pin number of the transmitter.
		*/
		static void sendTelegram(unsigned long data);

		/**
		* The complement of RemoteReceiver.	Send a telegram with specified code as data.
		*
		* Note: static method, which allows for use in low-mem situations.
		*
		* @param pin		output pin on Arduino to which the transmitter is connected
		* @param periodusec	[0..511] Duration of one period, in microseconds. A trit is 6 periods.
		* @param repeats	[0..7] The 2log-Number of times the signal is repeated. The actual number of repeats will be 2^repeats. 2 would be bare minimum, 4 seems robust.
		* @param code		The code to transmit. Note: only the first 20 bits are used, the rest is ignored. Also see sendTelegram.
		*/
		static void sendCode(unsigned long code, unsigned int periodusec, unsigned short repeats);

		/**
		* Compares the data received with RemoteReceive with the data obtained by one of the getTelegram-functions.
		* Period duration and repetitions are ignored by this function; only the data-payload is compared.
		*
		* @return true, if the codes are identical (the 20 least significant bits match)
		*/
		static bool isSameCode(unsigned long encodedTelegram, unsigned long receivedData);

	protected:
		unsigned int _periodusec;	// Oscillator period in microseconds
		unsigned short _repeats;	// Number over repetitions of one telegram
};

/**
* ActionTransmitter simulatos a remote, as sold in the Dutch 'Action' stores. But there are many similar systems on the market.
* If your remote has setting for 5 address bits, and can control 5 devices on or off, then you can try to use the ActionTransmitter.
* Otherwise you may have luck with the ElroTransmitter, which is similar.
*/
class ActionTransmitter: public RemoteTransmitter {
	public:
		/**
		* Constructor
		*
		* @param pin		output pin on Arduino to which the transmitter is connected
		* @param periodsec	Duration of one period, in microseconds. Default is 190usec
		* @param repeats	[0..7] The 2log-Number of times the signal is repeated.
		* @see RemoteTransmitter
		*/
		ActionTransmitter(unsigned int periodusec=190, unsigned short repeats=4);

		/**
		* Send a on or off signal to a device.
		*
		* @param systemCode	5-bit addres (dip switches in remote). Range [0..31]
		* @param device	Device to switch. Range: [A..E] (case sensitive!)
		* @param on	True, to switch on. False to switch off,
		*/
		void sendSignal(unsigned short systemCode, char device, bool on);

		/**
		* Generates the telegram (data) which can be used for RemoteTransmitter::sendTelegram.
		* See sendSignal for details on the parameters
		*
		* @return Encoded data, including repeats and period duration.
		*/
		unsigned long getTelegram(unsigned short systemCode, char device, bool on);
};

/**
* BlokkerTransmitter simulatos a remote, as sold in the Dutch 'Blokker' stores. But there are many similar systems on the market.
* These remotes have 4 on, 4 off buttons and a switch to switch between device 1-4 and 5-8. No futher configuration
* possible.
*/
class BlokkerTransmitter: public RemoteTransmitter {
	public:
		/**
		* Constructor
		*
		* @param pin		output pin on Arduino to which the transmitter is connected
		* @param periodsec	Duration of one period, in microseconds. Default is 307usec
		* @param repeats	[0..7] The 2log-Number of times the signal is repeated.
		* @see RemoteTransmitter
		*/
		BlokkerTransmitter(unsigned int periodusec=230, unsigned short repeats=4);

		/**
		* Send a on or off signal to a device.
		*
		* @param device	Device to switch. Range: [1..8]
		* @param on	True, to switch on. False to switch off,
		*/
		void sendSignal(unsigned short device, bool on);

		/**
		* @see RemoteTransmitter::getTelegram
		*/
		unsigned long getTelegram(unsigned short device, bool on);
};

/**
* KaKuTransmitter simulates a KlikAanKlikUit-remote, but there are many clones.
* If your transmitter has a address dial with the characters A till P, you can try this class.
*/
class KaKuTransmitter: public RemoteTransmitter {
	public:
		/**
		* Constructor
		*
		* @param pin		output pin on Arduino to which the transmitter is connected
		* @param periodsec	Duration of one period, in microseconds. Default is 375usec
		* @param repeats	[0..7] The 2log-Number of times the signal is repeated.
		* @see RemoteTransmitter
		*/
		KaKuTransmitter(unsigned int periodusec=375, unsigned short repeats=4);

		/**
		* Send a on or off signal to a device.
		*
		* @param address	addres (dial switches in remote). Range [A..P] (case sensitive!)
		* @param group	Group to switch. Range: [1..4]
		* @param device	Device to switch. Range: [1..4]
		* @param on	True, to switch on. False to switch off,
		*/
		void sendSignal(char address, unsigned short group, unsigned short device, bool on);

		/**
		* Send a on or off signal to a device.
		*
		* @param address	addres (dip switches in remote). Range [A..P] (case sensitive!)
		* @param device	device (dial switches in remote). Range [1..16]
		* @param on	True, to switch on. False to switch off,
		*/
		void sendSignal(char address, unsigned short device, bool on);

		/**
		* @see RemoteTransmitter::getTelegram
		*/
		unsigned long getTelegram(char address, unsigned short group, unsigned short device, bool on);

		/**
		* @see RemoteTransmitter::getTelegram
		*/
		unsigned long getTelegram(char address, unsigned short device, bool on);
};

/**
* ElroTransmitter simulates remotes of the Elro "Home Control" series
* see http://www.elro.eu/en/m/products/category/home_automation/home_control
* There are are many similar systems on the market. If your remote has setting for 5 address bits, and can control
* 4 devices on or off, then you can try to use the ElroTransmitter. Otherwise you may have luck with the
* ActionTransmitter, which is similar.
*/
class ElroTransmitter: public RemoteTransmitter {
	public:
		/**
		* Constructor
		*
		* @param pin		output pin on Arduino to which the transmitter is connected
		* @param periodsec	Duration of one period, in microseconds. Default is 320usec
		* @param repeats	[0..7] The 2log-Number of times the signal is repeated.
		* @see RemoteSwitch
		*/
		ElroTransmitter(unsigned int periodusec=320, unsigned short repeats=4);

		/**
		* Send a on or off signal to a device.
		*
		* @param systemCode	5-bit addres (dip switches in remote). Range [0..31]
		* @param device	Device to switch. Range: [A..D] (case sensitive!)
		* @param on	True, to switch on. False to switch off,
		*/
		void sendSignal(unsigned short systemCode, char device, bool on);

		/**
		* Generates the telegram (data) which can be used for RemoteSwitch::sendTelegram.
		* See sendSignal for details on the parameters
		*
		* @return Encoded data, including repeats and period duration.
		*/
		unsigned long getTelegram(unsigned short systemCode, char device, bool on);
};

//
// END CODE
// ------------------------------------------------------------------------------------------------------------
// C++ and C support
//
#ifdef __cplusplus
}
#endif

#endif
