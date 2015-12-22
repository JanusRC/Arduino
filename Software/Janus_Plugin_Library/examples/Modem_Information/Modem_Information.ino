/***************************************************
  This is an example for the Janus Plug in Modem arduino library

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

/*************
Modem Information Demo
This demo boots the modem up, issues a sequence of query commands, and prints the information
 
Telit modems by default run 115200, and we bring it down
to 19200 since the sofware serial is far too slow to catch the higher rate.

Please see the baud rate example if you haven't set the modem's baud rate yet.
************/

#include <Janus_Plugin.h>

/***************
User defines for the shield/Arduino
***************/
//Basic Arduino pins
#define LEDPIN 13

//UART
#define MDM_SW_TXD 7
#define MDM_SW_RXD 8

//Modem Hardware Control
#define MDM_ENABLE 5      //Regulator enable, active drive thru FET switch
#define MDM_ON_OFF 6      //Modem ON/OFF toggle pin, active drive thru FET switch
#define MDM_VBUS_EN 4     //Set as OC, drop low to enable VBUS via FET load switch
#define MDM_PWRMON A0     //Power monitor feedback signal. ADC Channel 0, Digital pin 14

//Misc.
#define ONOFF_DELAY 3000  //On Off Pulse time, General use = 3s. Adjust to specific modem timing if required.

//Application defines
#define BAUDRATE 19200    //New, lower baud rate that's useful for our library and SWSerial

/***************
Library Implementations
***************/
// This is to handle the absence of software serial on platforms
// like the Arduino Due. Modify this code if you are using different
// hardware serial port, or if you are using a non-avr platform
// that supports software serial.
#ifdef __AVR__
#include <SoftwareSerial.h>
SoftwareSerial ModemSS(MDM_SW_RXD, MDM_SW_TXD); // RX, TX
SoftwareSerial *ModemSerial = &ModemSS;         //Create pointer to the address
#else
HardwareSerial *ModemSerial = &Serial1;         //Use HW Serial when required
#endif

//Instance the modem constructor
Janus_Plugin myModem = Janus_Plugin(MDM_ENABLE, MDM_ON_OFF, MDM_VBUS_EN, MDM_PWRMON);

//**********************************
//**********************************

void setup() {
  Serial.begin(BAUDRATE); //Start debug output at 19200
  ModemSerial->begin(BAUDRATE); //Start the serial interface to the modem, just access thru the pointer
  myModem.begin(*ModemSerial);  //Pass in the pointer-to-address and start the library
  
  Serial.println(F("Plug in Terminus Modem Information"));

  //Initial check
  //Not on, turn everything on, check against threshold voltage (2v)
  if (myModem.isModemON(MDM_PWRMON, 2) == 0)
  { 
    myModem.enable(MDM_ENABLE);   //Ensure the modem is enabled
    delay(200);                   //Delay 200ms for regulator to stabilize
    
    Serial.println(F("Modem OFF, turning ON"));
    myModem.turnOnOff(MDM_ON_OFF, ONOFF_DELAY); //Built in pulse w/ delay
  }

  Serial.println(F("Modem ON\r\n"));
  delay(5000);  //Let the modem warm up, 5s is more than enough

  myModem.sendAT(F("ATE0"), ATC_STD_TIMEOUT);       //Disable echo
  myModem.sendAT(F("AT+CMEE=2"), ATC_STD_TIMEOUT);  //Turn on verbose
}

void loop() {
  /***************
  Application variables
  ***************/
  uint8_t Fault = FAULT_OK;         //Initialize Fault State.
  boolean ModemSIMReady = false;    //Initial state, SIM status
  uint8_t NetworkCellRegState = 0;  //Initial state, network registration status
  uint8_t NetworkDataRegState = 0;  //Initial state, data registration status

  //Network Information
  char NetworkName[16];         //Ex: Family Mobile, AT&T, etc.
  int NetworkRSSI = 99;
  int NetworkBER = 0;

  //Modem Characteristics
  char ModemMake[10];        //CGMI  - Manufacturer Information
  char ModemModel[10];       //CGMM  - Modem Type
  char ModemFirmware[16];    //CGMR  - Modem Firmware
  char ModemSerialNum[16];   //IMEI  - International Mobile Equipment Identity, modem serial number

  //SIM Information
  char SIMSerial[30];     //IMSI  - International Mobile Subscriber Identity
  char SIMIdentity[40];   //ICCID - Integrated Circuit Card Identification
  char SIMPNumber[16];    //CNUM  - SIM Given phone number

  //Initialize the arrays
  memset(NetworkName,0,sizeof(NetworkName)); 
  memset(ModemMake,0,sizeof(ModemMake)); 
  memset(ModemModel,0,sizeof(ModemModel)); 
  memset(ModemFirmware,0,sizeof(ModemFirmware)); 
  memset(ModemSerialNum,0,sizeof(ModemSerialNum)); 
  memset(SIMSerial,0,sizeof(SIMSerial)); 
  memset(SIMIdentity,0,sizeof(SIMIdentity)); 
  memset(SIMPNumber,0,sizeof(SIMPNumber)); 

  //*****************************
  // Modem Information
  //*****************************
  myModem.getSerialNumber(ModemSerialNum);
  myModem.getMake(ModemMake);
  myModem.getModel(ModemModel);
  myModem.getFirmware(ModemFirmware);

  Serial.println(F("Modem Ready"));
  Serial.println(F("-------------------------"));
  Serial.print(F("Make:     ")); Serial.println(ModemMake);
  Serial.print(F("Model:    ")); Serial.println(ModemModel);
  Serial.print(F("Serial:   ")); Serial.println(ModemSerialNum);
  Serial.print(F("Firmware: ")); Serial.println(ModemFirmware);
  Serial.println(F("-------------------------"));

  //Adjust this if not using a SIM based module (maybe check the modem type firt)
  Fault = myModem.waitForSIMReady(NET_QRY_TIMEOUT, &ModemSIMReady);

  if (!Fault) Serial.println(F("\r\nSIM Ready"));

  //Adjust this if not using a SIM based module (maybe check the modem type firt)
  if (ModemSIMReady)
  {  
    //*****************************
    // SIM Information
    //*****************************
    myModem.getPhoneNum(SIMPNumber);
    myModem.getSIMSerial(SIMSerial);
    myModem.getSIMCCID(SIMIdentity);

    Serial.println(F("-------------------------"));
    Serial.print(F("Serial:  ")); Serial.println(SIMSerial);
    Serial.print(F("ICCID:   ")); Serial.println(SIMIdentity);
    Serial.print(F("Phone #: ")); Serial.println(SIMPNumber);
    Serial.println(F("-------------------------"));
  }//SIMcheckend
  
  //*****************************
  // Registration Checks
  //*****************************
  Serial.println(F("\r\nWaiting for registration"));
  Serial.println(F("-------------------------"));
  //Check against defined HOME/ROAMING states
  while (NetworkCellRegState != HOME && NetworkCellRegState != ROAMING)
  {
    myModem.getRegStatus(&NetworkCellRegState);
    delay(500); //Delay half second to avoid hammering the interface
  }

  if (NetworkCellRegState == HOME) Serial.println(F("Modem Registered (Home)"));
  if (NetworkCellRegState == ROAMING) Serial.println(F("Modem Registered (Raoming)"));

  //Check against defined HOME/ROAMING states
  while (NetworkDataRegState != HOME && NetworkDataRegState != ROAMING)
  {
    myModem.getDataStatus(&NetworkDataRegState);
    delay(500); //Delay half second to avoid hammering the interface
  }

  if (NetworkDataRegState == HOME) Serial.println(F("Data Ready (Home)"));
  if (NetworkDataRegState == ROAMING) Serial.println(F("Data Ready (Raoming)"));

  //*****************************
  // Network Information
  //*****************************
  myModem.getNetworkName(NetworkName);
  Serial.print(F("Network:  ")); Serial.println(NetworkName);
  
  myModem.getSignalQuality(&NetworkRSSI, &NetworkBER);

  Serial.print(F("RSSI:     ")); Serial.print(-113 + (NetworkRSSI * 2)); Serial.println(F("dBm"));
  Serial.print(F("BER:      ")); Serial.println(NetworkBER);
  Serial.println(F("-------------------------"));
      
while(1)
{}
}//Mainend
