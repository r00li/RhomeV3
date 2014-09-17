/*
 * RemoteSwitch library v2.2.1 (20120314) made by Randy Simons http://randysimons.nl/
 * See RemoteTransmitter.h for details.
 *
 * License: GPLv3. See license.txt
 */

#include "RemoteTransmitter.h"
#include "RemoteReceiver.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"


/************
* RemoteTransmitter
************/

void RemoteTransmitter::initGPIO()
{
    GPIO_InitTypeDef GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

  GPIO_InitStructure.GPIO_Pin = RF_Transmit_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;


  GPIO_Init(GPIOA, &GPIO_InitStructure);
}

RemoteTransmitter::RemoteTransmitter(unsigned int periodusec, unsigned short repeats) {
	_periodusec=periodusec;
	_repeats=repeats;

	initGPIO();
}

unsigned long RemoteTransmitter::encodeTelegram(unsigned short trits[]) {
	unsigned long data = 0;

	// Encode data
	for (unsigned short i=0;i<12;i++) {
		data*=3;
		data+=trits[i];
	}

	// Encode period duration
	data |= (unsigned long)_periodusec << 23;

	// Encode repeats
	data |= (unsigned long)_repeats << 20;

	return data;
}

void RemoteTransmitter::sendTelegram(unsigned short trits[]) {
	sendTelegram(encodeTelegram(trits));
}

/**
* Format data:
* pppppppp|prrrdddd|dddddddd|dddddddd (32 bit)
* p = perioud (9 bit unsigned int
* r = repeats as 2log. Thus, if r = 3, then signal is sent 2^3=8 times
* d = data
*/
void RemoteTransmitter::sendTelegram(unsigned long data) {
    RemoteReceiver::disable();

	unsigned int periodusec = (unsigned long)data >> 23;
	unsigned short repeats = ((unsigned long)data >> 20) & 0b111;

	sendCode(data, periodusec, repeats);

	RemoteReceiver::enable();
}

void RemoteTransmitter::sendCode(unsigned long code, unsigned int periodusec, unsigned short repeats) {
	code &= 0xfffff; // Truncate to 20 bit ;
	// Convert the base3-code to base4, to avoid lengthy calculations when transmitting.. Messes op timings.
	unsigned long dataBase4 = 0;

	for (unsigned short i=0; i<12; i++) {
		dataBase4<<=2;
		dataBase4|=(code%3);
		code/=3;
	}

	repeats = 1 << (repeats & 0b111); // repeats := 2^repeats;

	for (unsigned short int j=0;j<repeats;j++) {
		// Sent one telegram

		// Recycle code as working var to save memory
		code=dataBase4;
		for (unsigned short i=0; i<12; i++) {
			switch (code & 0b11) {
				case 0:
					GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec);
					GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec*3);
					GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec);
					GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec*3);
					break;
				case 1:
					GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec*3);
					GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec);
					GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec*3);
					GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec);
					break;
				case 2: // KA: X or float
					GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec);
					GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec*3);
					GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec*3);
					GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
					delay_micro(periodusec);
					break;
			}
			// Next trit
			code>>=2;
		}

		// Send termination/synchronisation-signal. Total length: 32 periods
		GPIO_SetBits(RF_Transmit_Port, RF_Transmit_Pin);
		delay_micro(periodusec);
		GPIO_ResetBits(RF_Transmit_Port, RF_Transmit_Pin);
		delay_micro(periodusec*31);
	}
}

bool RemoteTransmitter::isSameCode(unsigned long encodedTelegram, unsigned long receivedData) {
	return (receivedData==(encodedTelegram & 0xFFFFF)); // ompare the 20 LSB's
}


/************
* ActionTransmitter
************/

ActionTransmitter::ActionTransmitter(unsigned int periodusec, unsigned short repeats) : RemoteTransmitter(periodusec,repeats) {
	// Call contructor
}


void ActionTransmitter::sendSignal(unsigned short systemCode, char device, bool on) {
	sendTelegram(getTelegram(systemCode,device,on));
}

unsigned long ActionTransmitter::getTelegram(unsigned short systemCode, char device, bool on) {
	unsigned short trits[12];

	device-=65;

	for (unsigned short i=0; i<5; i++) {
		// Trits 0-4 contain address (2^5=32 addresses)
		trits[i]=(systemCode & 1)?1:2;
		systemCode>>=1;

		// Trits 5-9 contain device. Only one trit has value 0, others have 2 (float)!
		trits[i+5]=(i==device?0:2);
    }

	// Switch on or off
	trits[10]=(!on?0:2);
	trits[11]=(on?0:2);

	return encodeTelegram(trits);
}

/************
* BlokkerTransmitter
************/

BlokkerTransmitter::BlokkerTransmitter(unsigned int periodusec, unsigned short repeats) : RemoteTransmitter(periodusec,repeats) {
	// Call contructor
}


void BlokkerTransmitter::sendSignal(unsigned short device, bool on) {
	sendTelegram(getTelegram(device,on));
}

unsigned long BlokkerTransmitter::getTelegram(unsigned short device, bool on) {
	unsigned short trits[12]={0};

	device--;

	for (unsigned short i=1; i<4; i++) {
		// Trits 1-3 contain device
		trits[i]=(device & 1)?0:1;
		device>>=1;
    }

	// Switch on or off
	trits[8]=(on?1:0);

	return encodeTelegram(trits);
}

/************
* KaKuTransmitter
************/

KaKuTransmitter::KaKuTransmitter(unsigned int periodusec, unsigned short repeats) : RemoteTransmitter(periodusec,repeats) {
	// Call contructor
}

void KaKuTransmitter::sendSignal(char address, unsigned short device, bool on) {
	sendTelegram(getTelegram(address, device, on));
}

unsigned long KaKuTransmitter::getTelegram(char address, unsigned short device, bool on) {
	unsigned short trits[12];

	address-=65;
	device-=1;

	for (unsigned short i=0; i<4; i++) {
		// Trits 0-3 contain address (2^4 = 16 addresses)
		trits[i]=(address & 1)?2:0;
		address>>=1;

		// Trits 4-8 contain device (2^4 = 16 addresses)
		trits[i+4]=(device & 1)?2:0;
		device>>=1;
    }

	// Trits 8-10 seem to be fixed
	trits[8]=0;
	trits[9]=2;
	trits[10]=2;

	// Switch on or off
	trits[11]=(on?2:0);

	return encodeTelegram(trits);
}

void KaKuTransmitter::sendSignal(char address, unsigned short group, unsigned short device, bool on) {
	sendTelegram(getTelegram(address, group, on));
}

unsigned long KaKuTransmitter::getTelegram(char address, unsigned short group, unsigned short device, bool on) {
	unsigned short trits[12], i;

	address-=65;
	group-=1;
	device-=1;

	// Address. M3E Pin A0-A3
	for (i=0; i<4; i++) {
		// Trits 0-3 contain address (2^4 = 16 addresses)
		trits[i]=(address & 1)?2:0;
		address>>=1;
    }

	// Device. M3E Pin A4-A5
	for (; i<6; i++) {
		trits[i]=(device & 1)?2:0;
		device>>=1;
	}

	// Group. M3E Pin A6-A7
	for (; i<8; i++) {
		trits[i]=(group & 1)?2:0;
		group>>=1;
	}

	// Trits 8-10 are be fixed. M3E Pin A8/D0-A10/D2
	trits[8]=0;
	trits[9]=2;
	trits[10]=2;

	// Switch on or off, M3E Pin A11/D3
	trits[11]=(on?2:0);

	return encodeTelegram(trits);
}


/************
* ElroTransmitter
************/

ElroTransmitter::ElroTransmitter(unsigned int periodusec, unsigned short repeats) : RemoteTransmitter(periodusec, repeats) {
	//Call contructor
}

void ElroTransmitter::sendSignal(unsigned short systemCode, char device, bool on) {
	sendTelegram(getTelegram(systemCode, device, on));
}

unsigned long ElroTransmitter::getTelegram(unsigned short systemCode, char device, bool on) {
	unsigned short trits[12];

	device-=65;

	for (unsigned short i=0; i<5; i++) {
		//trits 0-4 contain address (2^5=32 addresses)
		trits[i]=(systemCode & 1)?0:2;
		systemCode>>=1;

		//trits 5-9 contain device. Only one trit has value 0, others have 2 (float)!
		trits[i+5]=(i==device?0:2);
    }

	//switch on or off
	trits[10]=(on?0:2);
	trits[11]=(!on?0:2);

	return encodeTelegram(trits);
}
