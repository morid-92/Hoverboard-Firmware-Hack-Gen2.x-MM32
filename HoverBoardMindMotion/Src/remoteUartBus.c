

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
////                                                                 ////
////                 Remote UARTBus protocol by robo                 ////
////                                                                 ////
////  https://github.com/RoboDurden/Hoverboard-Firmware-Hack-Gen2.x  ////
////                                                                 ////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
#ifdef TARGET_MM32SPIN25
#include "HAL_device.h"                 // Device header
#else
#include "mm32_device.h"                // Device header
#endif         
#include "math.h" 
#include "../Inc/pinout.h"
#include "../Inc/bldc.h"
#include "../Inc/uart.h"
#include "stdio.h"
#include "string.h"
#include "../Inc/remoteUartBus.h"
#include "hal_crc.h"

#include "../Inc/sim_eeprom.h"
#include "../Inc/calculation.h"


#pragma pack(1)

extern uint32_t millis;
extern uint8_t sRxBuffer[10];

uint8_t bAnswerMaster = 0;
static int16_t iReceivePos = 0;	

extern int pwm;
extern int32_t speed;
extern uint8_t  wState;

extern int32_t iOdom;
extern int fvbat; 							// global variable for battery voltage
extern int fitotal; 									// global variable for current dc
extern int realspeed; 									// global variable for real Speed

typedef struct {			// �#pragma pack(1)� needed to get correct sizeof()
   uint8_t cStart;			//  = '/';
   //uint16_t cStart;		//  = #define START_FRAME         0xABCD
   uint8_t  iDataType;  //  0 = unique id for this data struct
   uint8_t iSlave;			//  contains the slave id this message is intended for
   int16_t  iSpeed;
   uint8_t  wState;   // 1=ledGreen, 2=ledOrange, 4=ledRed, 8=ledUp, 16=ledDown   , 32=Battery3Led, 64=Disable, 128=ShutOff
   uint16_t checksum;
} SerialServer2Hover;
typedef struct {			// �#pragma pack(1)� needed to get correct sizeof()
   uint8_t cStart;			//  = '/';
   //uint16_t cStart;		//  = #define START_FRAME         0xABCD
   uint8_t  iDataType;  //  1 = unique id for this data struct
   uint8_t 	iSlave;			//  contains the slave id this message is intended for
   int16_t  iSpeed;
   int16_t  iSteer;
   uint8_t  wState;   // 1=ledGreen, 2=ledOrange, 4=ledRed, 8=ledUp, 16=ledDown   , 32=Battery3Led, 64=Disable, 128=ShutOff
   uint8_t  wStateSlave;   // 1=ledGreen, 2=ledOrange, 4=ledRed, 8=ledUp, 16=ledDown   , 32=Battery3Led, 64=Disable, 128=ShutOff
   uint16_t checksum;
} SerialServer2HoverMaster;

typedef struct {			// �#pragma pack(1)� needed to get correct sizeof()
   uint8_t cStart;			//  = '/';
   //uint16_t cStart;		//  = #define START_FRAME         0xABCD
   uint8_t  iDataType;  //  2 = unique id for this data struct
   uint8_t 	iSlave;			//  contains the slave id this message is intended for
	 float  fBattFull;    // 10s LiIon = 42.0;
	 float  fBattEmpty;    // 10s LiIon = 27.0;
	 uint8_t  iDriveMode;      //  MM32: 0=COM_VOLT, 1=COM_SPEED, 2=SINE_VOLT, 3=SINE_SPEED
   int8_t 	iSlaveNew;			//  if >= 0 contains the new slave id saved to eeprom
   uint16_t checksum;
} SerialServer2HoverConfig;

static uint8_t aReceiveBuffer[255];	//sizeof(SerialServer2Hover)

#define START_FRAME         0xABCD       // [-] Start frme definition for reliable serial communication
typedef struct{				// �#pragma pack(1)� needed to get correct sizeof()
   uint16_t cStart;
   uint8_t iSlave;		//  the slave id this message is sent from
   int16_t iSpeed;		// 100* km/h
   uint16_t iVolt;		// 100* V
   int16_t iAmp;			// 100* A
   int32_t iOdom;		// hall steps
   uint16_t checksum;
} SerialHover2Server;




uint32_t iTimeLastRx = 0;


uint16_t CalcCRC(uint8_t *ptr, int count){    //file checksum calculation
	
  uint16_t  crc;
  uint8_t i;
  crc = 0;
  while (--count >= 0)
  {
    crc = crc ^ (uint16_t) *ptr++ << 8;
    i = 8;
    do
    {
      if (crc & 0x8000)
      {
        crc = crc << 1 ^ 0x1021;
      }
      else
      {
        crc = crc << 1;
      }
    } while(--i);
  }
  return (crc);
}



void RemoteUpdate(void){
	
	if (millis - iTimeLastRx > SERIAL_TIMEOUT){
		speed = 0;
	}
	if (bAnswerMaster)
	{
		AnswerMaster();
		bAnswerMaster = 0;
	}
}

extern 	uint32_t steerCounter;

uint32_t iAnswerMaster = 0;

void AnswerMaster(void){
	
	// Ask for steer input
	SerialHover2Server oData;
	oData.cStart = START_FRAME;
	oData.iSlave = SLAVE_ID;
	oData.iVolt = (uint16_t) (fvbat);
	oData.iAmp = (int16_t) 	(fitotal);
	oData.iSpeed = (int16_t) (realspeed	*10);
	oData.iOdom = (int32_t) iOdom;	//pwm	;
	//oData.iOdom = iAnswerMaster++;

	oData.checksum = 	CalcCRC((uint8_t*) &oData, sizeof(oData) - 2);	// (first bytes except crc)

	
	UART_Send_Group((uint8_t*) &oData, sizeof(oData));
	

}

extern uint32_t steerCounter;								// Steer counter for setting update rate

uint8_t iRxDataType;
uint8_t iRxDataSize;

// Update USART steer input
// static int16_t iReceivePos = -1;		// if >= 0 incoming bytes are recorded until message size reached
void serialit(void){


	uint8_t cRead = sRxBuffer[0];
	//DEBUG_LedSet((steerCounter%20) < 10,0)	// 	
	
	if (iReceivePos < 0)		// data reading not yet begun
	{
		if (cRead == '/')	// Start character is captured, start record
			iReceivePos = 0;
		else	
			return;
	}
	
	aReceiveBuffer[iReceivePos++] = cRead;
	
	if (iReceivePos == 1)	// '/' read
			return;

	if (iReceivePos == 2)	// iDataType read
	{
		iRxDataType = aReceiveBuffer[1];
		switch (iRxDataType)
		{
			case 0: iRxDataSize = sizeof(SerialServer2Hover);	break;
			case 1: iRxDataSize = sizeof(SerialServer2HoverMaster);	break;
			case 2: iRxDataSize = sizeof(SerialServer2HoverConfig);	break;
		}
		return;
	}
	
	if (iReceivePos < iRxDataSize)
		return;
		
		
	uint16_t iCRC = (aReceiveBuffer[iReceivePos-1] << 8) | aReceiveBuffer[iReceivePos-2];
	iReceivePos = -1;
	if (iCRC == CalcCRC(aReceiveBuffer, iRxDataSize - 2))	//  first bytes except crc
	{
		if (aReceiveBuffer[2] == SLAVE_ID)
		{
			iTimeLastRx = millis;
			
			switch (iRxDataType)
			{
				case 0:
				{
					SerialServer2Hover* pData = (SerialServer2Hover*) aReceiveBuffer;
					speed = pData->iSpeed;
					wState = pData->wState;
					break;
				}
				case 1: 
				{
					SerialServer2HoverMaster* pData = (SerialServer2HoverMaster*) aReceiveBuffer;
					speed = pData->iSpeed;
					wState = pData->wState;
					break;
				}
				case 2: 
				{
					SerialServer2HoverConfig* pData = (SerialServer2HoverConfig*) aReceiveBuffer;
					if (	(pData->fBattFull > 0) && (pData->fBattFull < 60.0)	)
					{
						BAT_FULL = pData->fBattFull * 1000;
					}
					if (	(pData->fBattEmpty > 0) && (pData->fBattEmpty < 60.0)	)
					{
						BAT_EMPTY = pData->fBattEmpty * 1000;
					}
					if (pData->iDriveMode <= SINE_SPEED)
					{
						DRIVEMODE = pData->iDriveMode;
						PIDrst();
						PID_Init();
						TIMOCInit();
					}
					if (pData->iSlaveNew >= 0 && 0)    //disabled!
						SLAVE_ID = pData->iSlaveNew;
			
					EEPROM_Write((u8*)pinstorage, 2 * 64);    //if the detection failed, the pin is still saved
					break;
				}
			}
			
			//if (speed > 300) speed = 300;	else if (speed < -300) speed = -300;		// for testing this function
			bAnswerMaster = 1;
			iTimeLastRx = millis;  	// Reset the pwm timout to avoid stopping motors
		}
	}
}

