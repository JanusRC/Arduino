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
Socket Dial Demo
This demo configures the modem to dial out to a remote host. 
Once the socket has been opened it creates a basic serial bridge and passes data in 64B packets.

The bridge is done via command mode, allowing AT command access during the bridge
This is useful if you want to send AT response data to this host (RSSI, etc.)

It's a little slower, but for this platform it allows the most flexibility. 
Using data/online mode can be done with some minor edits to the bridge loop, but remember
that this utilizes software serial at a lower rate, so if speed is a requirement you MUST
adjust this to use hardware serial. Don't expect blazing speeds from online mode without it.
 
Telit modems by default run 115200, and we bring it down
to 19200 since the sofware serial is far too slow to catch the higher rate.

Please see the baud rate example if you haven't set the modem's baud rate yet.

REQUIRED - Uncomment USE_SOCKET in Janus_Plugin.h
REQUIRED - Edit the GPRS information for your APN and host server.
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
#define COMMANDMODE 1     //Socket type, command
#define DATAMODE 0        //Socket type, online/data

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
  Serial.begin(BAUDRATE);       //Start debug output at 19200
  ModemSerial->begin(BAUDRATE); //Start the serial interface to the modem, just access thru the pointer
  myModem.begin(*ModemSerial);  //Pass in the pointer-to-address and start the library
  
  Serial.println(F("Plug in Terminus Socket Dial Demo"));

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
  uint8_t NetworkDataRegState = 0;  //Initial state, network registration status

  //GPRS
  boolean NetworkActivationState;  //Context Activation: 0 = Not Active, 1 = Active
  char NetworkIP[20];              //Ex: 255.255.255.255
  uint8_t ConnID = 1;              //Socket Connection ID
  char SocketReceiveBuf[64];       //Socket command mode receive buffer
  char SocketSendBuf[64];          //Socket command mode sending buffer
  int i;                           //Counter
  char *res;
  
  //Context Information - REQUIRED TO UPDATE THIS
  char AccessPointName[] = "yourapn"; //Ex: i2gold, internet, proxy
  char UserName[] = "";
  char Password[] = "";
  
  //Socket Information - REQUIRED TO UPDATE THIS
  char RemoteServer[] = "your.server.here"; //Ex: www.google.com
  char RemotePort[] = "yourport";           //Ex: 80

  /***************
  Application Begin
  ***************/
  //Loop this for continual checks/on time
  while(1)
  {
    //Adjust this if not using a SIM based module (maybe check the modem type firt)
    ModemSIMReady = false;
    Fault = myModem.waitForSIMReady(NET_QRY_TIMEOUT, &ModemSIMReady);
  
    if (!Fault) Serial.println(F("SIM Ready"));
  
    //Adjust this if not using a SIM based module (maybe check the modem type firt)
    if (ModemSIMReady)
    {  
      //*****************************
      // Registration Checks
      //*****************************
      Serial.println(F("Waiting for registration"));
      NetworkCellRegState = 0;
      //Check cellular against defined HOME/ROAMING states
      while (NetworkCellRegState != HOME && NetworkCellRegState != ROAMING)
      {
        Fault = myModem.getRegStatus(&NetworkCellRegState);
        delay(500); //Delay half second to avoid hammering the interface
      }

      //Check data against defined HOME/ROAMING states
      NetworkDataRegState = 0;
      while (NetworkDataRegState != HOME && NetworkDataRegState != ROAMING)
      {
        Fault = myModem.getDataStatus(&NetworkDataRegState);
        delay(500); //Delay half second to avoid hammering the interface
      }

      //*****************************
      // Context Activation
      //*****************************
      Serial.println(F("Registered, Activating Context"));
      Fault = myModem.setupContext(AccessPointName);
      
      //Check if we already have a context up
      if (!Fault) Fault = myModem.getContext(NetworkIP, &NetworkActivationState);

      //If not, activate it
      if (!NetworkActivationState)
        Fault = myModem.activateContext(UserName, Password, NetworkActivationState);
      
      if (!Fault)
      {
        //Update the context information for display/use
        Fault = myModem.getContext(NetworkIP, &NetworkActivationState);
        Serial.print(F("Activation State:  ")); Serial.println(NetworkActivationState, DEC);
        Serial.print(F("Context IP:        ")); Serial.println(NetworkIP); 
      }

      //*****************************
      // Socket/GPRS Handling
      //*****************************
      //Setup the socket information for SID #1
      Fault = myModem.socketSetup(1);

      Serial.println(F("Closing previous socket instances")); 
      //Ensure the socket is closed by the modem.
      Fault = myModem.socketClose(ConnID);  

      Serial.print(F("Opening New Socket to listener: ")); 
      Serial.print(RemoteServer); Serial.print(F(":")); Serial.println(RemotePort);

      if (!Fault) Fault = myModem.socketDial(ConnID, RemoteServer, RemotePort, COMMANDMODE);

      //Keep trying until we get connection or find there's no context anymore
      while (Fault && NetworkActivationState)
      {
        delay(2000);  //Delay 2s - No reason to grind as fast as possible
        myModem.getContext(NetworkIP, &NetworkActivationState);
        Fault = myModem.socketDial(ConnID, RemoteServer, RemotePort, COMMANDMODE);
      }

      if (!NetworkActivationState)
        continue; //Go back to the beginning and check everything prior

      if (!Fault) Serial.println(F("Socket Open."));
      //Drop into a loop. Broken if there's a problem.

      //Flush the UARTs
      Serial.println(F("Flushing UARTs."));
      while (Serial.available() || myModem.available())
      {
        myModem.read();   //Flush the RX
        Serial.read();    //Flush the RX
      }

      //*****************************
      // Data Forwarding/Bridge
      //*****************************
      Serial.println(F("Ready."));
      while(1)
      {  
        //****************** 
        // Socket Receive
        //****************** 
        //Read the AT interface
        res = NULL; //reset res
        res = myModem.parseLine();  //Grab the raw line data
        
        //Buffer is not null, we found something
        if (res)
        {
          //Fast reference to a fault code
          Fault = myModem.modemToFaultCode(res);

          //Check for incoming expected message types, then handle accordingly
          // SRING: x - Valid Data
          if (Fault == FAULT_SRING)
          {
            //Grab 64 bytes (max of 255) from the buffer
            memset(SocketReceiveBuf,0,sizeof(SocketReceiveBuf));
            Fault = myModem.socketReceive(ConnID, 64, SocketReceiveBuf);
  
            if (!Fault) Serial.print(F("Received data: ")); Serial.println(SocketReceiveBuf);
          }

          // NO CARRIER - Server has closed the connection or we lost the network.
          if (Fault == FAULT_NO_CARRIER)
          {
            Serial.println(F("Socket Closed."));
            break;
          }
          // ELSE - An error or odd failure, either way exit and check everything again.
          if (Fault == FAULT_ERROR)
          {
            Serial.println(F("Socket Closed - Error."));
            break;
          }
        }

        i = 0;
        //****************** 
        // Socket Send
        //****************** 
        while (Serial.available()) 
        {
          //Aggregate the serial data
          SocketSendBuf[i] = Serial.read();
          i++;
          delay(1); //Small delay to catch the full string/data entered.
        }

        if (i > 0)
        {
          //Send the buffered data out
          Fault = myModem.socketSend(ConnID, SocketSendBuf);
          if (!Fault) Serial.print(F("Sent data: ")); Serial.println(SocketSendBuf);
          memset(SocketSendBuf,0,sizeof(SocketSendBuf));  //clear buffer
          i = 0;  //Reset the counter
        }
      }//whileend2

      Serial.println(F("10s Before Retry."));
      delay(10000);   //Delay 10 seconds before trying the connection/loop again
    }//SIMcheckend
  }//whileend1

}//Mainend
