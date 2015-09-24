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
Modem USB Control Demo
This demo boots the modem up and then turns the USB on or off depending on user input.

The USB for the modem is separate to the main UART and can be used for a huge variety
of things as it breaks out secondary AT instances, dedicated GPS, and firmware update ports.

The USB VBUS signal must be controlled when using the Plug in Modems/Telit Modems to ensure
power up is seamless, as it must be activated after the modem is on and ready. 

This board gives a simple interface for controlling this signal, and Janus gives schematic
examples for embedded systems that have lower voltage operation (buffered by a FET).
 
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
  
  Serial.println(F("Plug in Terminus USB Control Demo"));

  //Initial check
  //Not on, turn everything on, check against threshold voltage (2v)
  if (myModem.isModemON(MDM_PWRMON, 2) == 0)
  { 
    myModem.enable(MDM_ENABLE);   //Ensure the modem is enabled
    delay(200);                   //Delay 200ms for regulator to stabilize
    
    Serial.println(F("Modem OFF, turning ON"));
    myModem.turnOnOff(MDM_ON_OFF, ONOFF_DELAY); //Built in pulse w/ delay
  }

  Serial.println(F("Modem ON, use '1' (ON) or '0' (OFF) to control VBUS"));
}

void loop() {
  /***************
  Application Begin
  ***************/
  boolean VbusState = false;  //Vbus current state, passed back by the command
  char ReadCommand = 0;       //Read input command

  Serial.print(F("Command: "));
  while (! Serial.available() ) 
  {
    //Spit out URCs while we wait.
    //if (myModem.available()) 
    //  Serial.write(myModem.read());
  }

  //Command input, read it and print it back to the user
  ReadCommand = Serial.read();
  Serial.print("[");
  Serial.print(ReadCommand);
  Serial.println("]");

  //Branch based on command
  switch (ReadCommand) 
  {
    case '1': 
    {
      VbusState = myModem.vbus_ON(MDM_VBUS_EN); //Turn VBUS ON
      if (VbusState) Serial.println("VBUS turned ON");
      break;
    }

    case '0': 
    {
      VbusState = myModem.vbus_OFF(MDM_VBUS_EN); //Turn VBUS OFF
      if (!VbusState) Serial.println("VBUS turned OFF");
      break;
    }

    default: 
    {
      Serial.println(F("Unknown command, use '1' (ON) or '0' (OFF)"));
      break;
    }
  }
  
}//Mainend
