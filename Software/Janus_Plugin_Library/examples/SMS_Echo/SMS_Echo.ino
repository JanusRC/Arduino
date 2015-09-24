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
SMS Echo Demo
This demo configures the modem to listen for incoming SMS. Once
received it parses the information, displays it, and then echoes to the sender.
 
Telit modems by default run 115200, and we bring it down
to 19200 since the sofware serial is far too slow to catch the higher rate.

Please see the baud rate example if you haven't set the modem's baud rate yet.

REQUIRED - Uncomment USE_SMS in Janus_Plugin.h
************/

#include <Janus_Plugin.h>

/***************
User defines for the shield/Arduino
***************/
//Basic Arduino pins
#define LEDPIN 13

//UART
#define MDM_SW_TXD 4
#define MDM_SW_RXD 3

//Modem Hardware Control
#define MDM_ENABLE 6      //Regulator enable, active drive thru FET switch
#define MDM_ON_OFF 7      //Modem ON/OFF toggle pin, active drive thru FET switch
#define MDM_VBUS_EN 5     //Set as OC, drop low to enable VBUS via FET load switch
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
  
  Serial.println(F("Plug in Terminus SMS Echo"));

  //Initial check
  //Not on, turn everything on, check against threshold voltage (2v)
  if (myModem.isModemON(MDM_PWRMON, 2) == 0)
  { 
    myModem.enable(MDM_ENABLE);   //Ensure the modem is enabled
    delay(200);                   //Delay 200ms for regulator to stabilize
    
    Serial.println(F("Modem OFF, turning ON"));
    myModem.turnOnOff(MDM_ON_OFF, ONOFF_DELAY); //Built in pulse w/ delay
  }

  Serial.println(F("Modem ON"));
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
  //SMS Info
  uint8_t NumOfStoredSMS = 0;       //Initial state, number of stored SMS
  int SMSIndex = 0;                 //Initial state, index of sent SMS. Useful for controller n amount of texts to send.
  SMS_t SMSInfo;                    //Library SMS structure to hold the processed SMS
  SMS_t *pSMSInfo = &SMSInfo;       //Pointer to the information

  /***************
  Application Begin
  ***************/
  //Loop this for continual checks/on time
  while(1)
  {
    //Adjust this if not using a SIM based module (maybe check the modem type firt)
    Fault = myModem.waitForSIMReady(NET_QRY_TIMEOUT, &ModemSIMReady);
  
    if (!Fault) Serial.println(F("SIM Ready"));
  
    //Adjust this if not using a SIM based module (maybe check the modem type firt)
    if (ModemSIMReady)
    {  
      //*****************************
      // Registration Checks
      //*****************************
      Serial.println(F("Waiting for registration"));
      //Check against defined HOME/ROAMING states
      while (NetworkCellRegState != HOME && NetworkCellRegState != ROAMING)
      {
        Fault = myModem.getRegStatus(&NetworkCellRegState);
        delay(500); //Delay half second to avoid hammering the interface
      }

      //*****************************
      // SMS Initialization
      //*****************************
      Serial.println(F("Registered, setting up SMS"));
      Fault = myModem.setupSMS();

      //Clear the memory
      Fault = myModem.deleteAllSMS();

      //*****************************
      // SMS Echo Begin
      //*****************************
      Serial.println(F("Waiting for new SMS"));
      //Drop into an SMS check loop. Broken if there's a problem.
      while(1)
      {
        Fault = myModem.checkSMS(&NumOfStoredSMS);
  
        if (NumOfStoredSMS > 0)
        {
          Serial.print(F("Number of SMS: ")); Serial.println(NumOfStoredSMS, DEC);
          Fault = myModem.processSMS(NumOfStoredSMS, pSMSInfo);
          
          Serial.print(F("\r\nMessage Information\r\n"));
          Serial.print(F("------------------------------\r\n"));
          Serial.print(F("Index value:         ")); Serial.println(pSMSInfo->StoreIndex);
          Serial.print(F("Stat value:          ")); Serial.println(pSMSInfo->Stat);
          Serial.print(F("OriginatingPN value: ")); Serial.println(pSMSInfo->OriginatingPN);
          Serial.print(F("Alpha value:         ")); Serial.println(pSMSInfo->Alpha);
          Serial.print(F("Date value:          ")); Serial.println(pSMSInfo->Date);
          Serial.print(F("Time value:          ")); Serial.println(pSMSInfo->Time);
          Serial.print(F("Data value:          ")); Serial.println(pSMSInfo->Data);
          Serial.print(F("Length value:        ")); Serial.println(pSMSInfo->Length);
          Serial.print(F("------------------------------\r\n\r\n"));

          //Echo the SMS
          Fault = myModem.sendSMS(pSMSInfo->Data, pSMSInfo->OriginatingPN, &SMSIndex);
          if (!Fault) Serial.print(F("SMS Echoed, Index: ")); Serial.println(SMSIndex, DEC);
          
          //Delete the processed SMS
          if (!Fault) Fault = myModem.deleteOneSMS(pSMSInfo->StoreIndex);
          if (!Fault) Serial.println(F("SMS Deleted"));
          if (!Fault) Serial.println(F("Waiting for new SMS"));
        }

        //Fast registration check
        Fault = myModem.getRegStatus(&NetworkCellRegState);
        if (Fault == FAULT_OK && NetworkCellRegState != HOME && NetworkCellRegState != ROAMING)
          break;

        delay(5000);  //Check every 5s, no need to hammer it
      }//whileend2
    }//SIMcheckend
  }//whileend1

}//Mainend
