/***************************************************
  This is an Arduino library for the Janus Plug in Modem 

  The hardware & IO control are designed to work with the
  DIP package Terminus Plug in Modem
  http://www.janus-rc.com/terminuscf.html

  Required HW: 2 Pins = TX, RX, ON_OFF (else you need external button)
  Optional HW: 3 Pins = ENABLE, VBUS ENABLE, PWRMON

  The AT command control and higher level functions are usable
  with most Telit modems, however.

  Written by Clayton Knight, cknight@janus-rc.com
  Heavy reference from the Adafruit Fona Library
  BSD license, all text above must be included in any redistribution
 ****************************************************/
#ifndef JANUS_PLUGIN_H
#define JANUS_PLUGIN_H 

#if (ARDUINO >= 100)
  #include "Arduino.h"
  #ifndef __SAM3X8E__  // Arduino Due doesn't support SoftwareSerial
    #include <SoftwareSerial.h>
  #endif
#else
  #include "WProgram.h"
  #include <NewSoftSerial.h>
#endif

/***************
Debug
***************/
//#define DEBUG         //Higher level UART show, both TX and RX
//#define DEBUG_LOWLVL  //Low level, show individual received/parsed lines in brackets

/***************
Application Defines
***************/
//Explicitly define the larger area of the library
//#define USE_SMS
//#define USE_SOCKET


//Modem types
#define HE910 1

//AT handling error codes, standard result codes to expect + extra fail handler
#define FAULT_OK                  0     //No actual fault, OK response
#define FAULT_TIMEOUT             0x01  //ATC timeout
#define FAULT_ERROR               0x02  //Standard ERROR response
#define FAULT_NO_CARRIER          0x03  //NO CARRIER from the network
#define FAULT_RING                0x04  //Incoming RING from a phone call
#define FAULT_SRING               0x05  //Incoming SRING from a socket listen
#define FAULT_CONNECT             0x06  //Connect response when making a data call
#define FAULT_PROMPT_FOUND        0x07  //'>' prompt, usually found in SMS sending or email
#define FAULT_CMS                 0x08  //CMS error
#define FAULT_CME                 0x09  //CME error
#define FAULT_FAIL                0xFF  //Does not fit elsewhere

//AT handling timeouts
#define ATC_STD_TIMEOUT   1500    // 1500 ms ATC standard timeout, more than enough (1s standard)
#define ATC_LNG_TIMEOUT   30000   // 30 Second long timeout for network/SMS/socket usage
#define NET_QRY_TIMEOUT   145000  // 145 Seconds 2.4 minutes, max network query time

//Other helpful defines
#define HOME 1
#define ROAMING 5

//SMS Information
//This structure is a container for all the usable information from an inbound SMS
//Use it by creating a structure of type SMS_t. 
//ex: SMS_t SMSInfo;
//myModem.processSMS(1, &SMSInfo);
//****************************************************************************
//-Technical Notation-
//+CMGR: <index>,<stat>,<oa/da>,<alpha>,<scts><CR><LF><data><CR><LF>
//
//-Plaintext Example-
//+CMGR: 1,"REC UNREAD","+12223334444","","15/02/11,12:37:17-24"
//Test12345
//OK
//****************************************************************************
typedef struct
{
  uint8_t StoreIndex;       //Processed SMS Index number
  char Stat[16];            //Processed SMS Status type
  char OriginatingPN[16];   //Processed Originating phone number
  char Alpha[16];           //Processed Alphanumeric representation of O.PN if a known phonebook entry
  char Date[16];            //Processed TP-Service Centre Date Stamp
  char Time[16];            //Processed TP-Service Centre Time Stamp
  uint8_t Length;           //Processed Length of SMS
  char Data[255];           //Processed SMS Data
} SMS_t;


class Janus_Plugin : public Stream {
 public:
	Janus_Plugin(int8_t enable, int8_t onoff, int8_t vbusenable, int8_t pwrmon);
	boolean begin(Stream &port);

	// Stream/UART control
	int available(void);
	size_t write(uint8_t x);
	int read(void);
	int peek(void);
	void flush();

	uint8_t sendATStr(const char *inSTR, char *outString, uint16_t timeout);
	uint8_t sendATStr(const __FlashStringHelper *inSTR, char *outString, uint16_t timeout);
	char *receiveATStr(char *outString, uint16_t timeout);
	
	void flushInput();
	uint8_t probeModemResponse(char *s);
	uint8_t modemToFaultCode(const char *s);
	
	uint8_t sendAT(const char *inSTR, uint16_t timeout);
	uint8_t sendAT(const __FlashStringHelper *inSTR, uint16_t timeout);
	char *receiveAT(uint16_t timeout);
	char* parseLine(void);

	// Modem I/O Control
	void turnOnOff(int OnPin, int inDelay);
	boolean vbus_ON(int VBUSPin);
	boolean vbus_OFF(int VBUSPin);
	void enable(int ENPin);
	void disable(int ENPin);
	uint8_t isModemON(int PWRPin, int inThresh);
	
	// Modem Basic Control
	uint8_t setBaudrate(uint32_t baud);
	uint8_t getSerialNumber(char *outString);
	uint8_t getMake(char *outString);
	uint8_t getModel(char *outString);
	uint8_t getFirmware(char *outString);
	uint8_t waitForSIMReady(int Timeout, boolean *SIMStatus);
	uint8_t getSIMSerial(char *outString);
	uint8_t getSIMCCID(char *outString);
	uint8_t getPhoneNum(char *outPN);
	
	// Network Commands
	uint8_t getRegStatus(uint8_t *cellRegStatus);
	uint8_t getDataStatus(uint8_t *dataRegStatus);
	uint8_t getSignalQuality(int *RSSIStatus, int *BERStatus);
	uint8_t getNetworkName(char *outNName);
	uint8_t setupContext(char *inAPN);
	uint8_t getContext(char *outNIP, boolean *ActivationState);
	uint8_t activateContext(char *UserName, char *Password, boolean ActivationState);
	uint8_t deactivateContext(boolean *ActivationState);
	
	// SMS Commands	
	uint8_t setupSMS(void);
	uint8_t sendSMS(char *Text, char *Phone, int *outIndex);
	uint8_t checkSMS(uint8_t *outNumOfStored);
	uint8_t processSMS(uint8_t selection, SMS_t *SMSInfo);
	uint8_t deleteOneSMS(uint8_t Selection);
	uint8_t deleteAllSMS(void);
	
	// Socket/GPRS Commands
	uint8_t socketSetup(uint8_t SID);
	uint8_t socketDial(uint8_t SID, char *IP, char *Port, uint8_t Mode);
	uint8_t socketListen(uint8_t SID, char *Port);
	uint8_t setupFirewall(char *IP, char *Mask);
	uint8_t enterCMDMode(void);
	uint8_t enterDataMode(uint8_t SID);
	uint8_t socketClose(uint8_t SID);
	uint8_t socketAccept(uint8_t CID);
	uint8_t socketSend(uint8_t SID, char *inSTR);
	uint8_t socketReceive(uint8_t SID, uint8_t Length, char *outReceived);
	
 protected:
	int8_t _enablepin;
	int8_t _onoffpin;
	int8_t _vbusenpin;
	int8_t _pwrmonpin;
	uint8_t _modem;

	Stream *ModemSerial;
};


#endif
