/***************************************************
  This is a library for the Janus plug in modem arduino shield

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
#include <avr/pgmspace.h>
    // next line per http://postwarrior.com/arduino-ethershield-error-prog_char-does-not-name-a-type/
#define prog_char  char PROGMEM

#if (ARDUINO >= 100)
  #include "Arduino.h"
  #ifndef __SAM3X8E__  // Arduino Due doesn't support SoftwareSerial
    #include <SoftwareSerial.h>
  #endif
#else
  #include "WProgram.h"
  #include <NewSoftSerial.h>
#endif

#include "Janus_Plugin.h"

//Create object
Janus_Plugin::Janus_Plugin(int8_t enable, int8_t onoff, int8_t vbusenable, int8_t pwrmon)
{
  _enablepin = enable;
  _onoffpin = onoff;
  _vbusenpin = vbusenable;
  _pwrmonpin = pwrmon;
  
  _modem = HE910;	//placeholder for eventual modem class usage.

  ModemSerial = 0;
}

//Starting function
//Initializes the I/O correctly, we leave higher level control to the user
//This is because they may want to automatically start it, or control the I/O differently.
boolean Janus_Plugin::begin(Stream &port) {
  ModemSerial = &port;

  //Set the I/O
  pinMode(_enablepin, OUTPUT);
  pinMode(_onoffpin, OUTPUT);
  pinMode(_vbusenpin, INPUT);  	//OC Configuration. Off by default
  pinMode(_pwrmonpin, INPUT);   //ADC Usage

  return true;
}

/***************************************************************
 HIGH LEVEL
***************************************************************/

/*********************************
Modem low level control functions
*********************************/
void Janus_Plugin::turnOnOff(int OnPin, int inDelay)
{ 
  digitalWrite(OnPin, HIGH);
  delay(inDelay);
  digitalWrite(OnPin, LOW);
}

boolean Janus_Plugin::vbus_ON(int VBUSPin)
{
	//Open collector control
	pinMode (VBUSPin, OUTPUT) ; // drive pin low, turn ON
return true;
}

boolean Janus_Plugin::vbus_OFF(int VBUSPin)
{
	//Open collector control
	pinMode (VBUSPin, INPUT) ;  // hi-Z state, turn OFF
return false;
}

void Janus_Plugin::enable(int ENPin)
{
  digitalWrite(ENPin, LOW);
}

void Janus_Plugin::disable(int ENPin)
{
  digitalWrite(ENPin, HIGH);
}

//Function to check the PWRMON feedback from the modem
//Arguments: Analog power pin, threshold voltage
uint8_t Janus_Plugin::isModemON(int PWRPin, int inThresh)
{
  int readPWRMON = 0; //PWRMON read value container

  readPWRMON = analogRead(PWRPin);  //Read PWRMON

  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  float voltage = readPWRMON * (5.0 / 1023.0);

  if (voltage > inThresh)
    return 1;
  else
    return 0;
}

/*********************************
Modem higher level control functions
*********************************/
uint8_t Janus_Plugin::setBaudrate(uint32_t baud)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char sndstring[15];    	 //Temporary buffer AT+IPR=115200\r\n

  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT+IPR=%lu",baud); 
    
  //Send the Query
  Fault = sendAT(sndstring, ATC_STD_TIMEOUT);

return Fault;
}

//***************************

uint8_t Janus_Plugin::getSerialNumber(char *outString)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[30];       	 //buffer
  uint8_t i = 0;

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT+CGSN"), string, ATC_STD_TIMEOUT);

  // 357164040646914
  while(string[i] != '\r')
  {
	*outString = string[i];
	*outString++;
	i++;
  }
	
return Fault;
}

//***************************

uint8_t Janus_Plugin::getMake(char *outString)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[30];       	 //buffer
  uint8_t i = 0;

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT+CGMI"), string, ATC_STD_TIMEOUT);

  // Telit
  while(string[i] != '\r')
  {
	*outString = string[i];
	*outString++;
	i++;
  }

return Fault;
}

//***************************

uint8_t Janus_Plugin::getModel(char *outString)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[30];       	 //buffer
  uint8_t i = 0;

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT+CGMM"), string, ATC_STD_TIMEOUT);

  // GE865-QUAD
  while(string[i] != '\r')
  {
	*outString = string[i];
	*outString++;
	i++;
  }

return Fault;
}

//***************************

uint8_t Janus_Plugin::getFirmware(char *outString)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[30];       	 //buffer
  uint8_t i = 0;

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT+CGMR"), string, ATC_STD_TIMEOUT);

  // 12.00.006
  while(string[i] != '\r')
  {
	*outString = string[i];
	*outString++;
	i++;
  }

return Fault;
}

//***************************

uint8_t Janus_Plugin::waitForSIMReady(int Timeout, boolean *SIMStatus)
{
    uint8_t Fault = FAULT_OK;   //Initialize Fault State
    char string[20];           //Temp buffer
    int ScaledTimeout = Timeout/1000;  //Scale the timeout to more of a 'tries' type

    memset(string,0,sizeof(string));
  
    *SIMStatus = false;  //Initialize the variable
  
    while(ScaledTimeout >= 0)
    {
      ScaledTimeout--;
      
      Fault = sendATStr(F("AT+CPIN?"), string, ATC_STD_TIMEOUT);
      if (!Fault)
      {
        // +CPIN: READY
        if (strncmp(string,"+CPIN: READY",12) == 0) 
        {
          *SIMStatus = true;  //Update variable
          return Fault;
        }    
        //TODO - Add extra faults to handle things like needing a PIN
      }
      delay(1000); //Delay a second to avoid hammering the SIM
    }

//SIM not ready, return a timeout
return FAULT_TIMEOUT;
}

//***************************

uint8_t Janus_Plugin::getSIMSerial(char *outString)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[30];      //buffer

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT#CIMI"), string, ATC_STD_TIMEOUT);

  // #CIMI: 310260584850247
  if (strncmp(string,"#CIMI: ",7) == 0) 
    strncpy(outString,&string[7], 15);

return Fault;
}

//***************************

uint8_t Janus_Plugin::getSIMCCID(char *outString)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[40];       //buffer

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT#CCID"), string, ATC_STD_TIMEOUT);

  // #CCID: 8901260580048502477
  if (strncmp(string,"#CCID: ",7) == 0) 
    strncpy(outString,&string[7], 20);

return Fault;
}

//***************************

uint8_t Janus_Plugin::getPhoneNum(char *outPN)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault
  char string[32];      //buffer
  char PhoneNumber[16]; //Initialize phone number buffer
  char *r;              //Create pointer to hold search location

  memset(string,0,sizeof(string));
    
  //Send the Query
  Fault = sendATStr(F("AT+CNUM"), string, ATC_STD_TIMEOUT);

  //**********************
  // 129 National Numbering, 145 International "+1"
  //Example Responses:
  //AT+CNUM (DE910)
  //+CNUM: "",2223334444,129
  //OK

  //AT+CNUM (HE910)
  //+CNUM: "","12223334444",129
  //OK
  //**********************
  
  //If no fault found
  if (!Fault)
  {
    //Check the "1xxxxxxxxxx" type of response
    //+CNUM: "","12223334444",129
    
    // Search for end of valid number: ",129
    r = strstr(string, "\",129"); 
  
    //Found
    if (r)
    {
      *r = 0; //Reset
      
      //Check for the beginning of valid number: ","1
      r = strstr(string, "\",\"1"); 

      //Found
      if (r)
      {
        uint8_t i = 0;  //Initialize main increment
        uint8_t j = 4;  //Initialize position

        //Fill in phone number, excluding initial '1': 2223334444
        while((i < 16) && (r[j] != 0))
          PhoneNumber[i++] = r[j++];

        PhoneNumber[i] = 0;
        
        //Copy to the outbound buffer
        strcpy(outPN, PhoneNumber);

        //Success
        return(Fault);
      }
    }

    //Check the "xxxxxxxxxx" type of response
    //+CNUM: "",2223334444,129
    
    // Search for end of valid number: ",129
    r = strstr(string, ",129");

    //Found
    if (r)
    {
      *r = 0; //Mark the end of valid number

      //Check for the beginning of valid number: ",
      r = strstr(string, "\",");

      //Found
      if (r)
      {
        uint8_t i = 0;  //Initialize main increment
        uint8_t j = 2;  //Initialize position

        //Fill in phone number, 2223334444
        while((i < 16) && (r[j] != 0))
          PhoneNumber[i++] = r[j++];

        PhoneNumber[i] = 0;

        //Copy to the outbound buffer
        strcpy(outPN, PhoneNumber);

        //Success
        return(Fault);
      }
    }

    //Check the "+1xxxxxxxxxx" type of response
    //+CNUM: "","+12223334444",129
    
    // Search for end of valid number: ",145
    r = strstr(string, ",145");

    //Found
    if (r)
    {
      *r = 0; //Reset

      //Check for the beginning of valid number:  ","+1
      r = strstr(string, "\",+1");

      //Found
      if (r)
      {
        uint8_t i = 0;  //Initialize main increment
        uint8_t j = 4;  //Initialize position

        //Fill in phone number, excluding initial '+1': 2223334444
        while((i < 16) && (r[j] != 0))
          PhoneNumber[i++] = r[j++];

        PhoneNumber[i] = 0;

        //Copy to the outbound buffer
        strcpy(outPN, PhoneNumber);
        
        //Success
        return(Fault);
      }
    }
  }

  return(Fault);  //Fault return
}

/*********************************
Network Functions
Network query and handling higher level functions
*********************************/
uint8_t Janus_Plugin::getRegStatus(uint8_t *cellRegStatus)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault = 0
  char string[20];      //Temporary buffer

  memset(string, 0, sizeof(string)); 

  //Send the Query
  Fault = sendATStr(F("AT+CREG?"), string, ATC_STD_TIMEOUT);

  //**********************
  // State Responses
  // +CREG: Mode,State
  // +CREG: 0,x
  // 0 - not registered, ME is not currently searching for a new operator
  // 1 - registered, home network
  // 2 - not registered, but ME is currently searching a new operator to register
  // 3 - registration denied
  // 4 - unknown
  // 5 - registered, roaming
  //**********************

  *cellRegStatus = 0;  //Initialize variable

  //If no fault found
  if (!Fault)
  {
    //Search for recognized response value and update structure accordingly
    if (strstr(string, ",1"))
      *cellRegStatus = 1;

    if (strstr(string, ",2"))
      *cellRegStatus = 2;

    if (strstr(string, ",3"))
      *cellRegStatus = 3;
    
    if (strstr(string, ",4"))
      *cellRegStatus = 4;

    if (strstr(string, ",5"))
      *cellRegStatus = 5;
  }
  
return Fault;
}

//***************************

uint8_t Janus_Plugin::getDataStatus(uint8_t *dataRegStatus)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault = 0
  char string[20];      //Temporary buffer

  memset(string, 0, sizeof(string)); 

  //Send the Query
  Fault = sendATStr(F("AT+CGREG?"), string, ATC_STD_TIMEOUT);

  //**********************
  // State Responses
  // +CGREG: Mode,State
  // +CGREG: 0,x
  // 0 - not registered, ME is not currently searching for a new operator
  // 1 - registered, home network
  // 2 - not registered, but ME is currently searching a new operator to register
  // 3 - registration denied
  // 4 - unknown
  // 5 - registered, roaming
  //**********************

  *dataRegStatus = 0; //Initialize variable

  //If no fault found
  if (!Fault)
  {
    //Search for recognized response value and update structure accordingly
    if (strstr(string, ",1"))
      *dataRegStatus = 1;

    if (strstr(string, ",2"))
      *dataRegStatus = 2;

    if (strstr(string, ",3"))
      *dataRegStatus = 3;
    
    if (strstr(string, ",4"))
      *dataRegStatus = 4;

    if (strstr(string, ",5"))
      *dataRegStatus = 5;
  }
  
return Fault;
}

//***************************

uint8_t Janus_Plugin::getSignalQuality(int *RSSIStatus, int *BERStatus)
{
  uint8_t Fault = FAULT_OK;    //Initialize Fault = 0
  char string[20];        //Temporary buffer
  char *r;                //Pointer for searching
  
  memset(string, 0, sizeof(string)); 

  //Send the Query
  Fault = sendATStr(F("AT+CSQ"), string, ATC_STD_TIMEOUT);
  
  //**********************
  // State Responses
  // +CSQ: <rssi>,<ber>
  // +CSQ: x,0
  // 0 - (-113) dBm or less
  // 1 - (-111) dBm
  // 2..30 - (-109)dBm..(-53)dBm / 2 dBm per step
  // 31 - (-51)dBm or greater
  // 99 - not known or not detectable
  //**********************

  int RSSItmp;
  int BERtmp;
  
  *RSSIStatus = 99;  // Signal Strength
  *BERStatus = 0;    // Bit Error Rate
  
  //If no fault found
  if (!Fault)
  {
    //Search for valid match
    r = strstr(string, "+CSQ: ");

    //Found
    if (r)
    {
      //Grab the length and increment the r pointer to be at the values
      r += 6;
      
      //Scan the string and convert to decimal values
      sscanf(r, "%d, %d", &RSSItmp, &BERtmp);
      *RSSIStatus = RSSItmp;
      *BERStatus = BERtmp;
      
    }
  } //Fault endif

return Fault;
}

//***************************

uint8_t Janus_Plugin::getNetworkName(char *outNName)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault = 0
  char string[40];        //Temporary buffer
  char NetworkName[16];   //Ex: Family Mobile, AT&T, etc.
  char *r;                //Pointer for searching

  memset(string, 0, sizeof(string)); 
  memset(NetworkName, 0, sizeof(NetworkName)); 

  //Send the Query
  Fault = sendATStr(F("AT+COPS?"), string, ATC_STD_TIMEOUT);
  
  //**********************
  // State Responses
  // +COPS: <mode>,<format>,<oper>
  // +COPS: 0,0,"AT&T",2
  //**********************
  
  //If no fault found
  if (!Fault)
  {   
    //Check for the beginning of valid name: ,"
    r = strstr(string, ",\""); 
    
    strcpy(NetworkName, "");  //Reset the Name before filling anew
    
    //Found
    if (r)
    {
      uint8_t i = 0;  //Initialize main increment
      uint8_t j = 2;  //Initialize position

      //Fill in the name
      while((i < 16) && (r[j] != '"'))
        NetworkName[i++] = r[j++];

      NetworkName[i] = 0; //Mark the end

      //Copy to the outbound buffer
      strcpy(outNName, NetworkName);
    }
  } //Fault endif
  
return Fault;
}

/*********************************
Network - Context Control
Context control, required for socket usage and others.
*********************************/
uint8_t Janus_Plugin::setupContext(char *inAPN)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault = 0
  char string[128];     //Temporary buffer
  
  //Format and store the full string to the temp buffer
  //Currently supports context 1 only
  sprintf(string, "AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0", inAPN);
  
  //Send the formatted string to the modem to set the context info
  Fault = sendAT(string, ATC_STD_TIMEOUT);

return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::getContext(char *outNIP, boolean *ActivationState)
{
  uint8_t Fault = FAULT_OK; //Initialize Fault = 0
  char string[40];      //Temporary buffer
  char NetworkIP[20];   //Ex: 255.255.255.255
  char *r;              //Pointer for searching

  memset(string, 0, sizeof(string)); 
  strcpy(NetworkIP, "000.000.000.000"); //Reset the IP before filling anew
  *ActivationState = false;                 //Reset the activation state before filling anew

  //Check for an active context, only supports context 1 currently.
  //We can use CGPADDR to retreive the current CID's IP if available as well.
  
  //Send the query, replace the string with the response
  Fault = sendATStr(F("AT#CGPADDR=1"), string, ATC_STD_TIMEOUT);
  
  //**********************
  // State Responses
  // #CGPADDR: cid,”xxx.yyy.zzz.www”
  // #CGPADDR: 1,”255.255.255.255”
  //**********************  
 

  //If we have a fault the above clearing is just as valid since the context cannot be open
  //If we get an ERROR on the command
  
  //No Fault
  if (!Fault)
  {
    //Search for a decimal, if we don't find one a context is not open (returned "")
    r = strstr(string, "."); 
    
    //Found IP
    if (r)
    {
      // Search for beginning of valid string: just a ": "166.130.102.97"
      r = strstr(string, "\""); 

      //Found BOS
      if (r)
      {
        uint8_t i = 0;  //Initialize main increment
        uint8_t j = 1;  //Initialize position

        //Fill in the IP
        while((i < 16) && (r[j] != '"'))
          NetworkIP[i++] = r[j++];

        NetworkIP[i] = 0;           //Mark the end
        
        strcpy(outNIP, NetworkIP);
        *ActivationState = true; //Set the activation state
        
      }//BOS endif
    } //IPcheck If endif
  } //Fault endif
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::activateContext(char *UserName, char *Password, boolean ActivationState)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault = 0
  char sndstring[64];   //Temporary buffer
  char rcvstring[40];   //Temporary buffer

  memset(rcvstring, 0, sizeof(rcvstring)); 

  //Activate context, only supports context 1 currently

  if (ActivationState == false)
  {
    //Context not already active
    //Format and store the full string to the temp buffer
    sprintf(sndstring, "AT#SGACT=1,1,\"%s\",\"%s\"\r\n", UserName,Password);
    
    //Send the query
    Fault = sendATStr(sndstring, rcvstring, NET_QRY_TIMEOUT);
    
    //**********************
    // State Responses
    // #SGACT: xxx.yyy.zzz.www
    // #SGACT: 255.255.255.255
    //**********************  
    
    //We don't actually care what the response is, we're running GetContext after to update information
  }

return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::deactivateContext(boolean *ActivationState)
{
  uint8_t Fault = FAULT_OK;  //Initialize Fault = 0

  //Deactivate context, only supports context 1 currently

  //Check the structure state
  if (*ActivationState == true)
  {
    //Context active, send the command
    Fault = sendAT(F("AT#SGACT=1,0"), NET_QRY_TIMEOUT);
  }
  
  delay(2000);  //Sleep for 2s to let the modem catch up

return Fault;
}

#ifdef USE_SMS
/*********************************
SMS Functions
SMS sending and handling higher level functions
*********************************/
uint8_t Janus_Plugin::setupSMS(void)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  
  //Enable TEXT format for SMS Message
  Fault = sendAT(F("AT+CMGF=1"), ATC_STD_TIMEOUT);
  
  //No indications, we will poll manually
  if (!Fault) Fault = sendAT(F("AT+CNMI=0,0,0,0,0"), ATC_STD_TIMEOUT);
  
  //Storage location  
  //if (!Fault) Fault = sendAT("AT+CPMS=\"SM\"", ATC_STD_TIMEOUT);  
  
  //Disable SMS extra information display
  if (!Fault) Fault = sendAT(F("AT+CSDH=0"), ATC_STD_TIMEOUT);
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::sendSMS(char *Text, char *Phone, int *outIndex)
{
  uint8_t Fault = FAULT_OK; //Initialize Fault State
  char sndstring[30];       //Temporary buffer: AT+CMGS="8473709261"
  char rcvstring[30];       //Temporary buffer
  char *r;                  //Pointer for searching
  int Index = 0;     		//Parsed SMS Index
  
  //Clear variable
  *outIndex = 0; 
  
  //Clear the receive buffer, the Telit returns "nothing"/OK
  memset(rcvstring, 0, sizeof(rcvstring)); 
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT+CMGS=\"%s\"", Phone);
  
  //Send the formatted string to the modem to start the SMS send
  Fault = sendAT(sndstring, ATC_STD_TIMEOUT);

  //Serial.print("sndstring: "); Serial.println(sndstring);
  //Serial.print("Fault: "); Serial.println(Fault, HEX);
  
  //Easy check for prompt built into the ATC Handling
  if (Fault == FAULT_PROMPT_FOUND)
  {
    //Bare send of the message to the uart
    ModemSerial->print(Text);    //Send the command out, ASCII
    ModemSerial->flush();        //Flush the UART Tx

    delay(500);
    
    ModemSerial->write(0x1A);    //Finish the send with a CTRL+Z (0x1a)
    //ModemSerial->flush();        //Flush the UART Tx
    
    //Read the UART for the Fault condition, receive the SMS index
    Fault = modemToFaultCode(receiveATStr(rcvstring, ATC_LNG_TIMEOUT));

    //Serial.print("Received String: "); Serial.println(rcvstring);
    //Serial.print("Fault: "); Serial.println(Fault, HEX);
	
    //If no Fault occurs during a send
    if (!Fault) 
    {
      //Search for the returned string for the index response
      //"+CMGS: x
      r = strstr(rcvstring, "+CMGS: ");
      
      //Found
      if (r)
      {
        //Increment the r pointer to be at the value based on known length
        r += 7;
        
        //Scan the string and convert to decimal value
        sscanf(r, "%d", &Index);
      }
      
      //Update the send index
      *outIndex = Index;
      
    }//Send Fault endif
	
  }//Prompt Fault endif

//Return standard fault
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::checkSMS(uint8_t *outNumOfStored)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char SMSList[300];      //Large space for storage of possibly multiple lines of text
  char SearchString[16];  //Small Search buffer to hold the CMGL: string
  char *r;                //Pointer for searching
  uint8_t i = 1;          //Counter
  
  //Clear the list buffer, the Telit returns "nothing"/OK
  memset(SMSList, 0, sizeof(SMSList)); 
  
  //Clear variable
  *outNumOfStored = 0; 
  
  //Send the query to check for new messages
  //Make use of the Multi-Line AT sending to retreive ALL messages.
  //If there are no stored messages it'll just be an OK with blank response
  Fault = sendATStr(F("AT+CMGL=\"ALL\""), SMSList, ATC_LNG_TIMEOUT);
  
  //Serial.println("\r\nReceived Response: ");
  //Serial.println("-----------------");
  //Serial.print(SMSList);
  //Serial.println("-----------------");
  
  //No Fault
  if (!Fault)
  {
    //Do an initial probe of the list, starting with 1
    r = strstr(SMSList, "+CMGL: ");

    while (r)
    {
      //Increment counter, will start at 1 for the loop
      i++;
      
      //Format the search string and store it
      sprintf(SearchString,"+CMGL: %d", i);
      
      //Keep searching the incremental message list
      r = strstr(r, SearchString);
    }
    
    //Subtract 1 for the NULL find if loop was utilized
    if (i > 0)
      i--;
    
    //Update container with amount
    *outNumOfStored = i;
  }

//Return standard fault
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::processSMS(uint8_t selection, SMS_t *SMSInfo)
{
  uint8_t Fault = FAULT_OK;     //Initialize Fault State
  char SMSRead[255];        //Large space for storage of possibly multiple lines of text
  char sndstring[12];       //Small buffer to hold the CMGR: string
  char *r;                  //Pointer for searching
  
  //Clear the read buffer, the Telit returns "nothing"/OK
  memset(SMSRead, 0, sizeof(SMSRead)); 
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT+CMGR=%d", selection);
  
  //Send the query to read the specified message
  //Make use of the Multi-Line AT sending to retreive the SMS as \r\n delimited
  //If there are no stored messages it'll just be an OK with blank response
  Fault = sendATStr(sndstring, SMSRead, ATC_LNG_TIMEOUT);
  
  
  //Serial.println("\r\nReceived Response: ");
  //Serial.println("-----------------");
  //Serial.print(SMSRead);
  //Serial.println("-----------------");
  
  //****************************************************************************
  //-Technical Notation-
  //+CMGR: <index>,<stat>,<oa/da>,<alpha>,<scts><CR><LF><data><CR><LF>
  //
  //-Plaintext Example-
  //+CMGR: "REC READ","+12223334444","","15/02/11,12:37:17-24"
  //Test12345
  //OK
  //****************************************************************************

  //If no Fault occurs during a send
  if (!Fault) 
  {
    //Search for the returned string for the index response
    //Looking for CMGR: "
    r = strstr(SMSRead, "+CMGR: \"");
    
    //Found
    if (r)
    {
      //Update the structure with the selected message index
      SMSInfo->StoreIndex = selection;
      
      //Clear the arrays
      memset(SMSInfo->Stat, 0, sizeof(SMSInfo->Stat)); 
      memset(SMSInfo->OriginatingPN, 0, sizeof(SMSInfo->OriginatingPN)); 
      memset(SMSInfo->Alpha, 0, sizeof(SMSInfo->Alpha)); 
      memset(SMSInfo->Date, 0, sizeof(SMSInfo->Date)); 
      memset(SMSInfo->Time, 0, sizeof(SMSInfo->Time)); 
      memset(SMSInfo->Data, 0, sizeof(SMSInfo->Data)); 
      SMSInfo->Length = 0;
      
      //Increment the r pointer to be at the start of the string based on known length
      //+CMGR: "
      r += 8;
      
      //*********************
      //  STAT Value
      //*********************
      
      uint8_t i = 0;  //Initialize value position increment
      uint8_t j = 0;  //Initialize r offset
      
      //Scan for the trailing "
      while(r[j] != '"')
        SMSInfo->Stat[i++] = r[j++];
      
      //Increment r to the next value: j value + ","
      r += (j+3);
      
      //*********************
      //  OriginatingPN Value
      //********************* 
      i = 0;  //Initialize value position increment
      j = 0;  //Initialize r offset
      
      //Scan for the trailing "
      while(r[j] != '"')
        SMSInfo->OriginatingPN[i++] = r[j++];
      
      //Increment r to the next value: j value + ","
      r += (j+3);
      
      //*********************
      //  Alpha Value
      //********************* 
      i = 0;  //Initialize value position increment
      j = 0;  //Initialize r offset
      
      //Scan for the trailing "
      while(r[j] != '"')
        SMSInfo->Alpha[i++] = r[j++];
      
      //Increment r to the next value: j value + ","
      r += (j+3);
      
      
      //*********************
      //  Date Value
      //********************* 
      i = 0;  //Initialize value position increment
      j = 0;  //Initialize r offset
      
      //Scan for the trailing ,
      while(r[j] != ',')
        SMSInfo->Date[i++] = r[j++];
      
      //Increment r to the next value: j value + ,
      r += (j+1);
      
      
      //*********************
      //  Time Value
      //********************* 
      i = 0;  //Initialize value position increment
      j = 0;  //Initialize r offset
      
      //Scan for the trailing "
      while(r[j] != '"')
        SMSInfo->Time[i++] = r[j++];
      
      //Increment r to the next value: j value + "\r\n
      r += (j+3);
      
      //*********************
      //  Data Value
      //********************* 
      i = 0;  //Initialize value position increment
      j = 0;  //Initialize r offset
      
      //Scan for the trailing \r\n EOL
      while(r[j] != '\r')
        SMSInfo->Data[i++] = r[j++];
      
      //*********************
      //  Length Value
      //********************* 
      SMSInfo->Length = strlen(SMSInfo->Data);
      
    }//r search endif
  }//send fault endif

return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::deleteOneSMS(uint8_t Selection)
{
  uint8_t Fault = FAULT_OK; //Initialize Fault State
  char sndstring[16];   //Temporary buffer
  
  //Format and store the full string to the temp buffer
  //Delete specific message, type "REC READ" and "REC UNREAD"
  sprintf(sndstring,"AT+CMGD=%d,0",Selection); 
  
  //Send the formatted string to the modem to start the SMS send
  Fault = sendAT(sndstring, ATC_STD_TIMEOUT);

return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::deleteAllSMS(void)
{
  uint8_t Fault = FAULT_OK; //Initialize Fault State
  
  //Delete all stored messages
  Fault = sendAT(F("AT+CMGD=1,4"), ATC_STD_TIMEOUT);

return Fault;
}

#endif

/*********************************
Socket/GPRS Functions
Socket handlers to setup/open/close/accept connections
*********************************/
#ifdef USE_SOCKET
uint8_t Janus_Plugin::socketSetup(uint8_t SID)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[40];     //Temporary buffer
  
  //------------------------
  
  //Sets the socket information via the SCFG command
  //
  //SCFG=x,1,1500,0,300,1   <- Our Selection
  //     x,1,300,90,600,50  <- Default
  
  //Format and store the full string to the temp buffer
  //Currently supports context 1 only
  sprintf(sndstring,"AT#SCFG=%d,1,1500,0,300,1", SID);
  
  //Send the formatted string to the modem
  Fault = sendAT(sndstring, ATC_STD_TIMEOUT);
  
  //------------------------
  
  //Sets the socket information via the SCFGEXT command
  //AT#SCFGEXT=x,0,0,30,0,0 <- Our Selection
  //           x,2,0,30,1,0 <- Default
  
  //Clear the send buffer
  memset(sndstring, 0, sizeof(sndstring)); 
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SCFGEXT=%d,0,0,30,0,0", SID);
  
  //Send the formatted string to the modem
  if (!Fault) Fault = sendAT(sndstring, ATC_STD_TIMEOUT);
  
  //------------------------
  
  //Skip the escape sequence during transfer
  if (!Fault) Fault = sendAT("AT#SKIPESC=1", ATC_STD_TIMEOUT);
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::socketDial(uint8_t SID, char *IP, char *Port, uint8_t Mode)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[64];     	  //Temporary buffer: EX: AT#SD=1,0,5556,"your.server.here",0,0,1
  
  //Format and store the full string to the temp buffer
  //Currently supports TCP only
  sprintf(sndstring,"AT#SD=%d,0,%s,\"%s\",0,0,%d", SID, Port, IP, Mode);
  
  //Send the command to open the socket in the specified manner
  Fault = sendAT(sndstring, ATC_LNG_TIMEOUT);
  
  //If we're opening a data mode socket
  if (!Mode)
  {
    //Check for the CONNECT response
    if (Fault == FAULT_CONNECT)
      Fault = FAULT_OK;   //Connect found, simply reassign fault back to OK for main processing
  }
  
  //If we're opening a command mode socket, we get an OK, so just return it

//Return OK, ERROR, or socket error types
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::socketListen(uint8_t SID, char *Port)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[16];     //Temporary buffer
  char rcvstring[35];     //Temporary buffer
  char Match[16];         //Match buffer
  char *r;                //Pointer for searching

  //Clear the receive buffer
  memset(rcvstring, 0, sizeof(rcvstring)); 
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SL=%d,1,%s", SID, Port);
  
  //Send the command to open the socket in the specified manner
  Fault = sendAT(sndstring, ATC_STD_TIMEOUT);
  
  //Check for a possible "CME: already listening" error response
  if (Fault == FAULT_CME)
  {
    //Send an AT#SL? query
    Fault = sendATStr("AT#SL?", rcvstring, ATC_STD_TIMEOUT);

    //No Fault
    if (!Fault)
    {
      //Format and store a compare string to the temp buffer
      //#SL: SID,Port,x
      sprintf(Match,"AT#SL: %d,%s", SID, Port);
    
      //Serach for the match
      r = strstr(rcvstring, Match);
      
      //Found
      if (r)
        Fault = FAULT_OK; //Already open socket found, simply reassign fault back to OK for main processing
      
      //Not found, just an OK response meaning socket is listening, just no open socket yet
      
    }//Fault endif
  }//CME check endif
  
  //If we're opening a listening socket, we get an OK, so just return it

//Return OK, ERROR, or socket error types
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::setupFirewall(char *IP, char *Mask)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[40];     //Temporary buffer
  
  /*****************
  Socket Listening Whitelist
  Connection is accepted if the criteria is met:
    incoming_IP & <net_mask> = <ip_addr> & <net_mask>
    
  Example:
    Possible IP Range = 197.158.1.1 to 197.158.255.255
    We set:
    IP = "197.158.1.1",
    Mask = "255.255.0.0"
  ******************/ 
  
  //Clear the previous whitelist if there is one
  Fault = sendAT(F("AT#FRWL=2"), ATC_STD_TIMEOUT);
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#FRWL=1,\"%s\",\"%s\"", IP, Mask);
  
  //Send the formatted string to the modem
  if (!Fault) Fault = sendAT(sndstring, ATC_STD_TIMEOUT);
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::enterCMDMode(void)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State

  delay(2000);             //Slight delay, guard time + some overkill
  
  //Bare send of the message to the uart
  ModemSerial->write('+');   //Finish the send with a CTRL+Z (0x1a)
#ifdef DEBUG
  Serial.print("\t---> "); Serial.print('+');
#endif
  delay(50);
  ModemSerial->write('+');   //Send second escape
#ifdef DEBUG
  Serial.print('+');
#endif
  delay(50);
  ModemSerial->write('+');   //Send third escape
#ifdef DEBUG
  Serial.println('+');
#endif
  delay(50);
  
  ModemSerial->flush();    //Flush the UART Tx 
  
  delay(1000);            //Slight delay, guard time + some overkill
  
  //Read the UART for the Fault condition, receive an OK
  Fault = modemToFaultCode(receiveAT(ATC_LNG_TIMEOUT));
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::enterDataMode(uint8_t SID)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[16];     //Temporary buffer
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SO=%d", SID);
  
  //Send the command to open the socket in the specified manner
  Fault = sendAT(sndstring, ATC_LNG_TIMEOUT);
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::socketClose(uint8_t SID)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[16];     //Temporary buffer
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SH=%d", SID);
  
  //Send the command to open the socket in the specified manner
  Fault = sendAT(sndstring, ATC_LNG_TIMEOUT);
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::socketAccept(uint8_t SID)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[16];     	  //Temporary buffer
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SA=%d", SID);
  
  //Send the command to open the socket in the specified manner
  Fault = sendAT(sndstring, ATC_LNG_TIMEOUT);
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::socketSend(uint8_t SID, char *inSTR)
{
  uint8_t Fault = FAULT_OK;   //Initialize Fault State
  char sndstring[12];     //Temporary buffer
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SSEND=%d", SID);
  
  //Send the command to open the socket in the specified manner
  Fault = sendAT(sndstring, ATC_LNG_TIMEOUT);

  //Easy check for prompt built into the ATC Handling
  if (Fault == FAULT_PROMPT_FOUND)
  {
    //Bare send of the message to the uart
    ModemSerial->print(inSTR);    //Send the command out, ASCII
    ModemSerial->flush();        //Flush the UART Tx

    delay(500);
    
    ModemSerial->write(0x1A);    //Finish the send with a CTRL+Z (0x1a)
    //ModemSerial->flush();        //Flush the UART Tx
    
    //Read the UART for the Fault condition, should be an OK or ERROR only
    Fault = modemToFaultCode(receiveAT(ATC_LNG_TIMEOUT));

    //Serial.print("Fault: "); Serial.println(Fault, HEX);
    //Serial.print("Received String: "); Serial.println(rcvstring);

  }//Prompt Fault endif
  
return Fault;
}

//****************************************************************************

uint8_t Janus_Plugin::socketReceive(uint8_t SID, uint8_t Length, char *outReceived)
{
  uint8_t Fault = FAULT_OK;   	//Initialize Fault State
  char sndstring[13];     		//Temporary buffer
  char rcvstring[269];     		//Temporary buffer (255B + 14 response B)
  char SocketReceiveBuf[255];	//Socket command mode receive buffer
  uint8_t i = 0;              	//Initialize value position increment
  uint8_t j = 0;              	//Initialize r offset
  char *r;                		//Pointer for searching

  //Clear the receive buffer
  memset(rcvstring, 0, sizeof(rcvstring)); 
  
  //Format and store the full string to the temp buffer
  sprintf(sndstring,"AT#SRECV=%d,%d", SID, Length);
  
  //Send the command to open the socket in the specified manner
  Fault = sendATStr(sndstring, rcvstring, ATC_LNG_TIMEOUT);

  //**********************
  // State Responses
  // #SRECV: <SID>,<length>
  // <data>
  //**********************

  //If no fault found
  if (!Fault)
  {
    //Search for valid match
    r = strstr(rcvstring, "#SRECV: ");

    //Found
    if (r)
    {
      //Increment the r pointer to be at the data, not the response.
      //We dont' really care about what the response is because we can check length
      //in main to verify there's more left
      while(*r != '\r')
        r ++;

      r += 2; //Add the \r\n to finalize  

      //Clear the buffer
      strcpy(SocketReceiveBuf, ""); 
      
      //*********************
      //  Socket incoming data
      //********************* 
      i = 0;  //Initialize value position increment
      j = 0;  //Initialize r offset
      
      //Scan for the trailing \r\n EOL, fill the socket receive buffer
      while(r[j] != '\r')
        SocketReceiveBuf[i++] = r[j++];

      SocketReceiveBuf[i] = 0; //Mark the end

      //Copy to the outbound buffer
      strcpy(outReceived, SocketReceiveBuf);
    }
  } //Fault endif
  
return Fault;
}

#endif

/***************************************************************
 LOW LEVEL
***************************************************************/
/*********************************
UART Functions
Base functions: https://www.arduino.cc/en/Reference/Serial
*********************************/

inline int Janus_Plugin::available(void) {
  return ModemSerial->available();
}

inline size_t Janus_Plugin::write(uint8_t x) {
  return ModemSerial->write(x);
}

inline int Janus_Plugin::read(void) {
  return ModemSerial->read();
}

inline int Janus_Plugin::peek(void) {
  return ModemSerial->peek();
}

inline void Janus_Plugin::flush() {
  ModemSerial->flush();
}

void Janus_Plugin::flushInput() {
    // Read all available serial input to flush pending data.
    uint16_t timeoutloop = 0;
    while (timeoutloop++ < 40) {
        while(available()) {
            read();
            timeoutloop = 0;  // If char was received reset the timer
        }
        delay(1);
    }
}

/**********************************
-Send basic AT commands
Uses the basic ReceiveAT to return fault code
Fault codes make the library flow a little simpler/easier as 
most commands respond with OK or ERROR. 
We can check the response at will be peeking at the global if necessary
**********************************/
uint8_t Janus_Plugin::sendAT(const char *inSTR, uint16_t timeout)
{
  char *res;
  
  flushInput(); //Flush any residual input data

#ifdef DEBUG
  Serial.print("\t---> "); Serial.println(inSTR);
#endif

  ModemSerial->println(inSTR); //Send in string, /r/n end
  ModemSerial->flush();        //Flush output, make sure buffer is clear

  res = receiveAT(timeout);

  return (modemToFaultCode(res));
}

uint8_t Janus_Plugin::sendAT(const __FlashStringHelper *inSTR, uint16_t timeout)
{
  char *res;
  
  flushInput(); //Flush any residual input data

#ifdef DEBUG 
  Serial.print("\t---> "); Serial.println(inSTR);
#endif

  ModemSerial->println(inSTR); //Send in string, /r/n end
  ModemSerial->flush();        //Flush output, make sure buffer is clear

  res = receiveAT(timeout);

  return (modemToFaultCode(res));
}

//***************************

uint8_t Janus_Plugin::sendATStr(const char *inSTR, char *outString, uint16_t timeout)
{
  char *res;
  
  flushInput(); //Flush any residual input data

#ifdef DEBUG
  Serial.print("\t---> "); Serial.println(inSTR);
#endif

  ModemSerial->println(inSTR); //Send in string, /r/n end
  ModemSerial->flush();        //Flush output, make sure buffer is clear

  res = receiveATStr(outString, timeout);

  return (modemToFaultCode(res));
}

uint8_t Janus_Plugin::sendATStr(const __FlashStringHelper *inSTR, char *outString, uint16_t timeout)
{
  char *res;
  
  flushInput(); //Flush any residual input data

#ifdef DEBUG
  Serial.print("\t---> "); Serial.println(inSTR);
#endif

  ModemSerial->println(inSTR); //Send in string, /r/n end
  ModemSerial->flush();        //Flush output, make sure buffer is clear

  res = receiveATStr(outString, timeout);

  return (modemToFaultCode(res));
}

/**********************************
-Read responses to AT commands that have standard responses
Notes: 
Can catch a prompt (>)
Requires echo to be off (ATE0 to be sent first)
**********************************/
char *Janus_Plugin::receiveAT(uint16_t timeout) 
{
  
  char *Str;     	//Local string buffer
  bool EOR = false; //flag for EOR

  while (timeout > 0 && EOR == false) 
  {
    timeout--;

    Str = parseLine();

    if (Str)
    {
      //Check against standard end of reply responses
      // OK, ERROR, NO CARRIER, etc
      if (probeModemResponse(Str))
      {
        EOR = true;
        break;
      }

      //Check against the entry prompt reply
      // ">"
      if (strstr(Str, ">"))
      {
        return ">";
      }
    }
      
    delay(1); //1ms loop delay, rough time keeper
  }
  
  if (timeout == 0)
  {
#ifdef DEBUG
  Serial.print("\t<--- "); Serial.println("TIMEOUT");
#endif
    return("TIMEOUT");
  }

#ifdef DEBUG
  Serial.print("\t<--- "); Serial.println(Str);
#endif
return Str;
}

//***************************

/**********************************
-Read responses to AT commands and pass back the string for use
Notes: 
Handles multiline
Requires echo to be off (ATE0 to be sent first)
**********************************/
char *Janus_Plugin::receiveATStr(char *outString, uint16_t timeout) 
{
  
  char *Str;     //Local string buffer
  bool EOR = false; //flag for EOR

  while (timeout > 0 && EOR == false) 
  {
    timeout--;

    Str = parseLine();
    
    if (Str)
    {
      int len = 0;
  
      //Check against standard end of reply responses
      // OK, ERROR, NO CARRIER, etc
      //If found, exit here
      if (probeModemResponse(Str))
      {
        EOR = true;
        break;
      }

      //Check against the entry prompt reply
      // ">"
      //If found, exit and reteturn the character
      if (strstr(Str, ">"))
      {
        return ">";
      }
      
      //No EOR found, update buffer with length of parsed line
      len = strlen(Str);

      //Valid length/data
      if (len)
      {
        //Add \r\n to separate for later parsing/use
        //This doubles to put back the \r\n in areas that meant to have this in the data (Ex: SMS data)
        Str[len] = '\r';
        Str[len+1] = '\n';
        
        //Copy the local buffered line to the out buffer
        memcpy(outString, Str, len+2);

        //Advance (+2 for the additive)
        outString += len+2; 
        
#ifdef DEBUG
        Serial.print("\t<--- "); Serial.print(Str);
#endif
      }

      Str = NULL;
    }
      
    delay(1); //1ms loop delay, rough time keeper (now in the parse, directly)
  }
  
  if (timeout == 0)
  {
#ifdef DEBUG
  Serial.print("\t<--- "); Serial.println("TIMEOUT");
#endif
    return("TIMEOUT");
  }

#ifdef DEBUG
  Serial.print("\t<--- "); Serial.println(Str);
#endif
return Str;
}

//***************************

char* Janus_Plugin::parseLine(void)
{
  char Linebuff[255];
  int Lineidx = 0;

  //Initialize response buffer to 0/null
  memset(Linebuff,0,sizeof(Linebuff));

  //Initialize the reply index
  Lineidx = 0;

  if (!ModemSerial->available())
    return NULL;
    
#ifdef DEBUG_LOWLVL
  Serial.print("[");
#endif
  while(ModemSerial->available()) 
  {
    //Read the data byte by byte
    char c =  ModemSerial->read();
    //Locate /r/n for specific line catch
    if (c == 0x0D || c == 0x0A) 
    {
      if (Lineidx == 0)
        continue;
#ifdef DEBUG_LOWLVL 
      Serial.print(Linebuff); Serial.println("]");
#endif
      return Linebuff;  //return caught line end
    }

    //Fill the buffer otherwise
    Linebuff[Lineidx] = c; 
    Lineidx++;

    delay(1); //because sofware serial sucks
    
  }

#ifdef DEBUG_LOWLVL
Serial.println("]");
#endif

return Linebuff;  //return anything else for handling (> prompt, raw data, etc.)
}

//***************************

// Expects response string with leading white space stripped
//  Identifies classic modem responses
uint8_t Janus_Plugin::probeModemResponse(char *s)
{
  if (strncmp_P(s, (prog_char*)F("OK"), 2) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("RING"), 4) == 0)
    return(1);
  
  if (strncmp_P(s, (prog_char*)F("SRING"), 5) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("CONNECT"), 7) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("ERROR"), 5) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("+CMS ERROR"), 10) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("+CME ERROR"), 10) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("NO ANSWER"), 9) == 0)
    return(1);

  if (strncmp_P(s, (prog_char*)F("NO CARRIER"), 10) == 0)
    return(1);

  if (strstr_P(s, (prog_char*)F("ERROR"))) // Anywhere as async
    return(1);

  if (strstr_P(s, (prog_char*)F("NO CARRIER"))) // Anywhere as async
    return(1);

  return(0);
}


/*********************************************************
Basic result codes
Result Codes, Numeric form, Verbose form Description
0 OK Command executed.
1 CONNECT Entering online state.
2 RING Alerting signal received from network.
3 NO CARRIER Unable to activate the service.
4 ERROR Command not recognized or could not be executed.
6 NO DIALTONE No dial tone detected within time-out period.
7 BUSY Reorder (Busy signal) received.
8 NO ANSWER Five seconds of silence not detected after ring back when @ dial modifier is used.
9 > Prompt response that accompanies SMS and similar command structures
*********************************************************/

uint8_t Janus_Plugin::modemToFaultCode(const char *s)
{
  if (strncmp_P(s, (prog_char*)F("OK"),2) == 0)
    return(FAULT_OK);

  if (strncmp_P(s, (prog_char*)F("RING"),4) == 0)
    return(FAULT_RING);
  
  if (strncmp_P(s, (prog_char*)F("SRING"),5) == 0)
    return(FAULT_SRING);

  if (strncmp_P(s, (prog_char*)F("CONNECT"),7) == 0)
    return(FAULT_CONNECT);

  if (strncmp_P(s, (prog_char*)F("ERROR"),5) == 0)
    return(FAULT_ERROR);
  
  if (strncmp_P(s, (prog_char*)F(">"),1) == 0)
    return(FAULT_PROMPT_FOUND);

  if (strncmp_P(s, (prog_char*)F("+CMS ERROR:"), 11) == 0)
    return(FAULT_CMS);

  if (strncmp_P(s, (prog_char*)F("+CME ERROR:"), 11) == 0)
    return(FAULT_CME);

  if (strncmp_P(s, (prog_char*)F("NO CARRIER"),10) == 0)
    return(FAULT_NO_CARRIER);

  if (strncmp_P(s, (prog_char*)F("TIMEOUT"),7) == 0)
    return(FAULT_TIMEOUT);

  return(FAULT_FAIL);
}
