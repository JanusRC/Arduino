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
  Modified by SureshVakati, svakati@janus-rc.com
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
#define MDM_SW_TXD 7
#define MDM_SW_RXD 8
int ledPin = 9;
int speakerPin = 10;
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
#include <LiquidCrystal.h>
LiquidCrystal lcd(12, 11, A2, A3, A4, A5);
SoftwareSerial ModemSS(MDM_SW_RXD, MDM_SW_TXD); // RX, TX
SoftwareSerial *ModemSerial = &ModemSS;         //Create pointer to the address
#else
HardwareSerial *ModemSerial = &Serial1;         //Use HW Serial when required
#endif

//Instance the modem constructor
Janus_Plugin myModem = Janus_Plugin(MDM_ENABLE, MDM_ON_OFF, MDM_VBUS_EN, MDM_PWRMON);

//**********************************
//**********************************


int length = 15; // the number of notes
char notes[] = "ccggaagffeeddc "; // a space represents a rest
int beats[] = { 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 4 };
int tempo = 300;

void playTone(int tone, int duration) {
  for (long i = 0; i < duration * 1000L; i += tone * 2) {
    digitalWrite(speakerPin, HIGH);
    delayMicroseconds(tone);
    digitalWrite(speakerPin, LOW);
    delayMicroseconds(tone);
  }
}

void playNote(char note, int duration) {
  char names[] = { 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'C' };
  int tones[] = { 1915, 1700, 1519, 1432, 1275, 1136, 1014, 956 };

  // play the tone corresponding to the note name
  for (int i = 0; i < 8; i++) {
    if (names[i] == note) {
      playTone(tones[i], duration);
    }
  }
}


void setup() {
  
  lcd.begin(16, 2);
  pinMode(speakerPin, OUTPUT);
  Serial.begin(BAUDRATE); //Start debug output at 19200
  ModemSerial->begin(BAUDRATE); //Start the serial interface to the modem, just access thru the pointer
  myModem.begin(*ModemSerial);  //Pass in the pointer-to-address and start the library
  pinMode(13, OUTPUT);
  pinMode(2, OUTPUT);
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
          
          String PhoneNumbe=pSMSInfo->OriginatingPN;      
          String Number="+18159815194"; // Change this to your number 
          String input=pSMSInfo->Data; 
          /**************************************Split the incoming Command into two parts.**************************************************/       
          String Command;
          int Iterations;
          for (int i = 0; i < input.length(); i++) {
          if (input.substring(i, i+1) == ",") {
          Command = input.substring(0, i);
          Serial.println(Command);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(Command);
          Iterations = input.substring(i+1).toInt();
          Serial.println(Iterations);
          lcd.print(",");
          lcd.print(Iterations);
          break;
          }
          }
          /********************************************************if the command is LED,10 then LED on pin 13 blinks for 10 times.*/
          if ((PhoneNumbe)==Number && (Command=="LED"))
          {
            Serial.print(F("13 Blinking:         ")); Serial.println("Yes");
            for (int i=1; i<=Iterations; i++)
            {
              digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
              delay(100);              // wait for a second
              digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW
              delay(100);
              lcd.setCursor(0, 1);
              lcd.print("Executing:");
              lcd.print(i);
              
            }            
          }
          else
          {            
            Serial.print(F("13 Blinking:         ")); Serial.println("No");          }
          /***********************************************************************************************************************/        
           /******************************************If the command is Fade,255 then LED attached to Pin 9 Fades from 0-255.*/
           if ((PhoneNumbe)==Number && (Command=="Fade"))
          {
            Serial.print(F("Fading:         ")); Serial.println("Yes");
            for (int k=1;k<=Iterations;k++){
              lcd.setCursor(0, 1);
          lcd.print("Executing:");
          lcd.print(k);
            
            for (int fadeValue = 0 ; fadeValue <= 255; fadeValue += 5) {
                                                                                // sets the value (range from 0 to 255):
            analogWrite(ledPin, fadeValue);
                                                                                // wait for 30 milliseconds to see the dimming effect
            delay(30);
          }

                                                                                  // fade out from max to min in increments of 5 points:
            for (int fadeValue = 255 ; fadeValue >= 0; fadeValue -= 5) {
                                                                                  // sets the value (range from 0 to 255):
            analogWrite(ledPin, fadeValue);
                                                                                  // wait for 30 milliseconds to see the dimming effect
            delay(30);
          }}
          
            
          }
          else
          {            
            Serial.print(F("Fading:         ")); Serial.println("No");          }

          /***********************************************************************************************************************/
          /******************************************If the command is Music,5 then Buzzer attached to Pin 8 plays the melody for 5 times.********/
         if ((PhoneNumbe)==Number && (Command=="Music"))
          {  
            Serial.print(F("Music:         ")); Serial.println("Yes");
          for (int k=1; k<=Iterations; k++)
            {
               lcd.setCursor(0, 1);
          lcd.print("Executing:");
          lcd.print(k);
          for (int i = 0; i < length; i++) {
             if (notes[i] == ' ') {
              delay(beats[i] * tempo); // rest
            } else {
               playNote(notes[i], beats[i] * tempo);
           }

               // pause between notes
          delay(tempo / 2); 
           }
          
           
          }      
          }
           else
          {            
            Serial.print(F("Music:         ")); Serial.println("No");          }
          
          /***********************************************************************************************************************/
          if ((PhoneNumbe)==Number && (Command=="Blink"))
          {
            Serial.print(F("Blinking:         ")); Serial.println("Yes");
            for (int k=1;k<=Iterations;k++){
              lcd.setCursor(0, 1);
          lcd.print("Executing:");
          lcd.print(k);// wait for a second
            digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
            delay(1000);              // wait for a second
            digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
            delay(1000);
            
          }
          
          }
          else
          {            
            Serial.print(F("Blinking:         ")); Serial.println("No");          }
            /***********************************************************************************************************************/

           /***********************************************************************************************************************/
          if ((PhoneNumbe)==Number && (Command=="FastBlink"))
          {
            Serial.print(F("Blinking:         ")); Serial.println("Yes");
            for (int k=1;k<=Iterations;k++){
               lcd.setCursor(0, 1);
          lcd.print("Executing:");
          lcd.print(k);
            digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
            delay(100);              // wait for a second
            digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
            delay(100);              // wait for a second
           
          }
          
          }
          else
          {            
            Serial.print(F("Blinking:         ")); Serial.println("No");          }

             /***********************************************************************************************************************/
             /***********************************************************************************************************************/
          if ((PhoneNumbe)==Number && (Command=="Voltage"))
          {
            Serial.print(F("Voltage:         ")); Serial.println("Yes");
             int sensorValue = analogRead(A0);
            // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
            float voltage = sensorValue * (5.0 / 1023.0);
            lcd.print (voltage);
            }
         else
          {            
            Serial.print(F("Voltage:         ")); Serial.println("No");          }
             /***********************************************************************************************************************/
            
          //Echo the SMS
          Fault = myModem.sendSMS("Command Recieved", pSMSInfo->OriginatingPN, &SMSIndex);
          if (!Fault) Serial.print(F("SMS Echoed, Index: ")); Serial.println(SMSIndex, DEC);
          
          //Delete the processed SMS
          if (!Fault) Fault = myModem.deleteOneSMS(pSMSInfo->StoreIndex);
          if (!Fault) Serial.println(F("SMS Deleted"));
          if (!Fault) Serial.println(F("Waiting for new SMS"));
          lcd.clear();
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
