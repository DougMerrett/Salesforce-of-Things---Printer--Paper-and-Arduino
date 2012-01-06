////////////////////////////////////////////////////////////
// Internet of Things Contest                             //
//                                                        //
// Author: Doug Merrett - Salesforce.com Copyright 2011   //
//                                                        //
// 28672 is the magic "max size" of sketches (at present) //
// So unfortunately, the DHCP IP address selection had to //
// be removed and the hardwired IP address added to the   //
// CONFIG.TXT file...  :-(                                //
////////////////////////////////////////////////////////////

#include <LiquidCrystal.h>   // Standard Library
#include <SPI.h>             // Standard Library
#include <Ethernet.h>        // Standard Library
#include <SdFat.h>           // http://code.google.com/p/sdfatlib/ - chosen for Stream functions

// change to true/false to contol Serial output of debug statements
#define DEBUG false

// LCD Initialisation: initialise the LCD library with the numbers of the interface pins
// (Done here so the LCDPrint function below can access it)
//       LCD pins: RS,EN,D4,D5,D6,D7
LiquidCrystal lcd ( 3, 5, 6, 7, 8, 9);

//
// Functions to print the messages from FLASH memory
//
void SerialPrint (const prog_uchar msg [])
{
  char c;
  if (!msg)  // if passed NULL
  {
    return;
  }
  
  while ((c = pgm_read_byte (msg++)))
  {
    Serial.print (c);
  }
}

void SerialPrintln (const prog_uchar msg [])
{
  SerialPrint (msg);
  Serial.println ();
}

void LCDPrint (const prog_uchar msg [])
{
  char c;
  if (!msg)  // if passed NULL
  {
    return;
  }
  
  while ((c = pgm_read_byte (msg++)))
  {
    lcd.print (c);
  }
}

// Strings to be displayed via the above functions.  The strings used all the 2K SRAM memory
// so now they are part of the Program memory and not the SRAM...  Saved over 1000 bytes!!
const prog_uchar FreeMem             [] PROGMEM  = {"Free memory : "};
const prog_uchar SDCardInitialising  [] PROGMEM  = {"SD Card Initialising."};
const prog_uchar SDCardInit          [] PROGMEM  = {"SD Card Init"};
const prog_uchar SDCardFailed        [] PROGMEM  = {"SD Card failed"};
const prog_uchar OrNotPresent        [] PROGMEM  = {"or not present"};
const prog_uchar SDCardInitialised   [] PROGMEM  = {"SD Card initialised."};
const prog_uchar ErrorOpeningConfig  [] PROGMEM  = {"Error opening CONFIG.TXT"};
const prog_uchar OpenFailed          [] PROGMEM  = {"Open Failed"};
const prog_uchar ReadingConfig       [] PROGMEM  = {"Reading Config"};
const prog_uchar Chars               [] PROGMEM  = {" chars: "};
const prog_uchar Skipping            [] PROGMEM  = {"skipping"};
const prog_uchar MACAddr             [] PROGMEM  = {"MAC addr  : "};
const prog_uchar ErrorParsingMACAddr [] PROGMEM  = {"Error parsing MAC address"};
const prog_uchar MyIP                [] PROGMEM  = {"My IP     : "};
const prog_uchar ErrorParsingMyIP    [] PROGMEM  = {"Error: couldn't parse my IP address"};
const prog_uchar ProxyIP             [] PROGMEM  = {"Proxy IP  : "};
const prog_uchar ErrorParsingProxy   [] PROGMEM  = {"Error: couldn't parse Proxy IP address"};
const prog_uchar ProxyPort           [] PROGMEM  = {"Proxy Port: "};
const prog_uchar ProxyPortInvalid    [] PROGMEM  = {"Error: Proxy port number invalid"};
const prog_uchar ErrorLocNameSpace   [] PROGMEM  = {"Error: location name has embedded spaces"};
const prog_uchar Location            [] PROGMEM  = {"Location  : "};
const prog_uchar ErrorMustBeSPB      [] PROGMEM  = {"Error: Must be 'S'cales, 'P'rinter or 'B'oth"};
const prog_uchar IsPrinter           [] PROGMEM  = {"isPrinter : "};
const prog_uchar IsScales            [] PROGMEM  = {"isScales  : "};
const prog_uchar ScalesPin           [] PROGMEM  = {"Scales pin: "};
const prog_uchar ScalesPinInvalid    [] PROGMEM  = {"Error: Scales input pin invalid or > 5"};
const prog_uchar PaperPin            [] PROGMEM  = {"Paper pin : "};
const prog_uchar PaperPinInvalid     [] PROGMEM  = {"Error: Paper out input pin invalid or > 5"};
const prog_uchar ScalesEqualsPaper   [] PROGMEM  = {"Error: ScalesPin = PaperPin"};
const prog_uchar ErrorNonEmptyLines  [] PROGMEM  = {"Error: Non empty lines after last config line"};
const prog_uchar ErrorInConfig       [] PROGMEM  = {"Error in Config"};
const prog_uchar IPAddr              [] PROGMEM  = {"IP Address:"};

// Global vars
//
// Used in the setup ()
byte           mac [6];                    // MAC address for the Arduino board
byte           arduinoIP [4];              // Arduino's IP address
String         locationName;               // The printer/location name
boolean        isScales;                   // Are we weighing paper?
boolean        isPrinter;                  // Are we a printer?
byte           proxy [4];                  // Java Webservice Proxy IP address
unsigned int   port = 0;                   // Java Webservice Proxy port
unsigned int   scalesPin = 0;              // The pin that the Weight scale is attached to
unsigned int   paperPin  = 0;              // The pin that the paper out sensor is attached to
EthernetClient client;                     // The Arduino process to call the Java Proxy

// Used in the loop ()
unsigned long  lastSampleTime = 0;         // Used to hold the last millisecond time for the sensing function
int            timeBetweenReadings = 500;  // 500ms between readings
byte           nowReams = 0;               // how many reams are on the scales now?
byte           oldReams = 255;             // how many reams were on the scales before?
byte           nowPaper = 0;               // do we have paper now? (0 = no, 1 = yes)
byte           oldPaper = 255;             // did we have paper before?

////////////////////////////////////////////////////////////////
// Uncomment the following section to add a check program.    //
// Call it when you want to see how much SRAM you have left   //
////////////////////////////////////////////////////////////////
/*
uint8_t *heapptr, *stackptr;
void check_mem ()
{
  stackptr = (uint8_t *) malloc (4);      // use stackptr temporarily
  heapptr = stackptr;                     // save value of heap pointer
  free (stackptr);                        // free up the memory again (sets stackptr to 0)
  stackptr = (uint8_t *) (SP);            // save value of stack pointer
  SerialPrint (FreeMem);
  Serial.println (stackptr - heapptr);
}
*/

//
// Function prototypes for later
//
byte calcReams   (unsigned int val);
byte calcPaper   (unsigned int val);
void sendMessage (String mesg);

//////////////////////////////////
// Setup function - called once //
//////////////////////////////////
void setup () 
{
  SdFat sd;                               // SD Card file system object
  char sdcardBuf [35];                    // SD card file read buffer for stream
  int intsIn [6];                         // An array to hold the integers parsed in from the config file
  char ch [3];                            // Used to hold the "." between the IP address numbers
  boolean OK;                             // Are there errors in the input file
  int state = 0;                          // State variable for reading the config file:
                                          //   0 - MAC address, 1 - My IP, 2 - Proxy IP, 3 - Proxy Port, 4 - Location Name,
                                          //   5 - "S"cale/"P"rinter/"B"oth, 6 - Scale Input Pin, 7 - Paper Out sensor Pin

  // Start the serial library for detailed information
  Serial.begin (9600);
  
  // set up the LCD's number of columns and rows: 
  lcd.begin (16, 2);
  
  // Wait a bit for everything to settle
  delay (1000);
  
  // Initialise the SD Card to read the config from
  SerialPrintln (SDCardInitialising);
  LCDPrint (SDCardInit);
    
  // Make sure that the default chip select pin is set to OUTPUT
  pinMode (10, OUTPUT);
  
  // see if the card is present and can be initialized (Pin 4 for ChipSelect)
  if (!sd.init (SPI_FULL_SPEED, 4))
  {
    // detailed error to Serial
    sd.initErrorPrint ();

    lcd.clear ();
    LCDPrint (SDCardFailed);
    lcd.setCursor (0, 1);
    LCDPrint (OrNotPresent);
    
    // Not working at all, so just sit here forever! 
    while (true)
      ;
  }

  SerialPrintln (SDCardInitialised);
  lcd.setCursor (0, 1);
  
  // Open the config file.
  ifstream configStream ("CONFIG.TXT");
  if (!configStream.is_open ())
  {
    SerialPrintln (ErrorOpeningConfig);
    LCDPrint (OpenFailed);

    // Not working at all, so just sit here forever! 
    while (true)
      ;    
  }

  LCDPrint (ReadingConfig);

  // Read a line from the SD Card  
  while (configStream.getline (sdcardBuf, sizeof (sdcardBuf), '\n') || configStream.gcount())
  {
    int count = configStream.gcount ();
    
    if (configStream.fail ())  // Did we get to the delimiter?  (fail means we didn't)
    {
      // Read the rest of the line and throw away
      configStream.ignore (1000, '\n');
      configStream.clear (configStream.rdstate() & ~ios_base::failbit);  // Reset the error bits
    }
    else
      if (!configStream.eof ())
      {
        count--;  // Don't include newline char in character count
      }

    // Here we have the line (up to sizeof (sdcardBuf) [35] chars) from the SD Card file
    
    // ignore comments at the beginning of a line and also empty lines
    if (count == 0 || (sdcardBuf [0] == '/' && sdcardBuf [1] == '/'))
    {
      continue;
    }
    
    // State of reading the config file:
    //   0 - MAC address, 1 - My IP, 2 - Proxy IP, 3 - Proxy Port, 4 - Location Name,
    //   5 - "S"cale/"P"rinter/"B"oth, 6 - Scale Input Pin, 7 - Paper Out sensor Pin
    OK = true;

    // Initialize input string into a stream buffer, so C++ stream processing can occur...
    ibufstream inLine (sdcardBuf);

    switch (state)
    {
      case 0:   // MAC Address
        if (inLine >> hex >> intsIn [0] >> hex >> intsIn [1] >> hex >> intsIn [2] >> hex >> intsIn [3] >> hex >> intsIn [4] >> hex >> intsIn [5])
        {
          SerialPrint (MACAddr);
          for (byte i = 0; i < 6; i++)
          {
            mac [i] = (byte) intsIn [i];  // Convert from integers into bytes as the stream reader only reads integers...
            Serial.print (mac [i], HEX);  // Display it in "normal" form, not a character
            Serial.print (' ');
          }        
          Serial.println ();
        }
        else
        {
          SerialPrintln (ErrorParsingMACAddr);
          OK = false;
        }

        break;

      case 1:  // My IP Address
        if (inLine >> intsIn [0] >> ch [0] >> intsIn [1] >> ch [1] >> intsIn [2] >> ch [2] >> intsIn [3])
        {
          SerialPrint (MyIP);
          for (byte i = 0; i < 4; i++)
          {
            arduinoIP [i] = (byte) intsIn [i];
            Serial.print (arduinoIP [i], DEC);
            if (i < 3)
              {
                Serial.print (ch [i]);
                if (ch [i] != '.')
                {
                  OK = false;
                }
              }
          }        
          Serial.println ();
        }
        else
        {
          SerialPrintln (ErrorParsingMyIP);
          OK = false;
        }

        break;

      case 2:  // Proxy IP Address
        if (inLine >> intsIn [0] >> ch [0] >> intsIn [1] >> ch [1] >> intsIn [2] >> ch [2] >> intsIn [3])
        {
          SerialPrint (ProxyIP);
          for (byte i = 0; i < 4; i++)
          {
            proxy [i] = (byte) intsIn [i];
            Serial.print (proxy [i], DEC);
            if (i < 3)
              {
                Serial.print (ch [i]);
                if (ch [i] != '.')
                {
                  OK = false;
                }
              }
          }        
          Serial.println ();
        }
        else
        {
          SerialPrintln (ErrorParsingProxy);
          OK = false;
        }

        break;
        
      case 3:  // Proxy port
        if (inLine >> port)
        {
          SerialPrint (ProxyPort);
          Serial.println (port);
        }
        else
        {
          SerialPrintln (ProxyPortInvalid);
          OK = false;
        }

        break;

      case 4:  // Location name
        locationName = String (sdcardBuf);
        for (byte i = 0; i < count; i++)
        {
          if (sdcardBuf [i] == ' ')
          {
            OK = false;
            SerialPrintln (ErrorLocNameSpace);
          }
        }
        if (OK)
        {
          SerialPrint (Location);
          Serial.println (locationName);
        }
        break;
        
      case 5:  // "S"cale, "P"rinter or "B"oth
        OK = false;  // more chance of failure, so default that way
        if (inLine >> ch [0])
        {
          isPrinter = false;
          isScales  = false;
          if (count == 1)
          {
            if (ch [0] == 'P' || ch [0] == 'p' || ch [0] == 'B' || ch [0] == 'b')
            {
              isPrinter = true;
              OK = true;
            }

            if (ch [0] == 'S' || ch [0] == 's' || ch [0] == 'B' || ch [0] == 'b')
            {
              isScales = true;
              OK = true;
            }
          }
        }
          
        if (!OK)
          {
            SerialPrintln (ErrorMustBeSPB);
          }
        else
          {
            SerialPrint (IsPrinter);
            Serial.println (isPrinter);
            SerialPrint (IsScales);
            Serial.println (isScales);
          }
        break;
            
      case 6:  // Scales pin
        if (inLine >> scalesPin)
        {
          SerialPrint (ScalesPin);         
          Serial.println (scalesPin);

          if (scalesPin > 5)
          {
            OK = false;
          }
        }
        else
          {
            OK = false;
          }

        if (!OK)
        {
          SerialPrintln (ScalesPinInvalid);
          OK = false;
        }

        break;

      case 7:  // Paper Out sensor Pin
        if (inLine >> paperPin)
        {
          SerialPrint (PaperPin);  
          Serial.println (paperPin);

          if (paperPin > 5)
          {
            OK = false;
          }
        }
        else
        {
          OK = false;
        }

        if (!OK)
        {
          SerialPrintln (PaperPinInvalid);
        }
        else
        {
          if (paperPin == scalesPin)
          {
            SerialPrintln (ScalesEqualsPaper);
            OK = false;
          }
        }
        
        break;

      default:
        SerialPrintln (ErrorNonEmptyLines);
        OK = false;
        break;
    }

    if (!OK)
    {
      lcd.setCursor (0, 1);
      LCDPrint (ErrorInConfig);

      // Not working at all, so just sit here forever! 
      while (true)
        ;    
    }
    
    // increment the state and around we go again
    state++;
  }
  configStream.close ();
  
  // Start the Ethernet connection. The arguments are the MAC (hardware) address that
  // is on your Ethernet shield and the IP address to use.  
  Ethernet.begin (mac, arduinoIP);
  
  // wait for 1 second to make sure you could read previous LCD message
  delay (1000);

  // print the Arduino's IP address:
  lcd.clear ();
  LCDPrint (IPAddr);
  lcd.setCursor (0, 1);
  lcd.print (IPAddress (arduinoIP));
  
  // Wait for a second before starting
  delay (1000);
}

/////////////////////////////////////////////
// The main loop that is called repeatedly //
/////////////////////////////////////////////
void loop ()
{
  unsigned int scalesValue;   // The scales sensor value
  unsigned int paperValue;    // The paper out sensor
  unsigned long now;          // The number of milliseconds
  
  // Is it time to check the readings? 
  now = millis ();
  
  // Check to see if we've wrapped around the millisecond limit (about 49 days)
  // or we are timeBetweenReadings after lastSample time (avoiding integer overflow
  // caused by lastSampleTime + timeBetweenReadings being more than 0xFFFFFFFF)
  
  if ((now < lastSampleTime) || ((now - lastSampleTime) > timeBetweenReadings))
  {
    // get the values
    if (isScales)
    {
      scalesValue = analogRead (scalesPin);
      nowReams = calcReams (scalesValue);
      
      // check to see if the amount of paper has changed
      if ((nowReams != oldReams) && (nowReams != 255))
      {
        // Yep, so let people know
        lcd.clear ();
        lcd.print ("There are now ");
        lcd.print (nowReams); 
        lcd.setCursor (0, 1);       
        lcd.print ("reams of paper");
        
        // Send message to Salesforce via the Proxy
        sendMessage ("S," + locationName + "," + String (nowReams, DEC));
        
        // and set the old reams to the now value for next time around...
        oldReams = nowReams;
      }
    }
    
    if (isPrinter)
    {
      paperValue  = analogRead (paperPin);
      nowPaper = calcPaper (paperValue);   
      
      // check to see if the out of paper status has changed
      if (nowPaper != oldPaper)
      {
        // Yep, so let people know
        lcd.clear ();

        if (nowPaper == 0)
        {
          lcd.print ("The printer is");
          lcd.setCursor (0, 1);  
          lcd.print ("out of paper");
        }
        else
        {
          lcd.print ("The printer");
          lcd.setCursor (0, 1);  
          lcd.print ("now has paper");
        }        
        
        // Send message to Salesforce via the Proxy
        sendMessage ("P," + locationName + "," + String (nowPaper, DEC));
        
        // and set the old paper to the now value for next time around...        
        oldPaper = nowPaper;
      }
    }

    // Keep the sampletime for later
    lastSampleTime = now;
  }
}

// Functions to calculate the real values dependant on the sensor values
byte calcReams (unsigned int val)
{
  // According to the strain guage readings,
  // 0 Reams :  24  
  // 1 Reams : 153
  // 2 Reams : 286
  // 3 Reams : 417
  // 4 Reams : 548
  // 5 Reams : 679  (Max)
  //
  // So a ream is about 131, leaving 20 either side for safety, calc how many reams...
  
  unsigned int calc  = 20;
  byte         reams = 0;
  
  while (calc < 800)
  {
    if ((calc - 20) <= val && val <= (calc + 20))
    {
      return reams;
    }
    else
    {
      // increment to the next ream value..
      calc += 131;
      reams++;
    }
  }
  
  return 255;  // Error!!  Too Many Reams of Paper!! 
}

byte calcPaper (unsigned int val)
{ 
  // if the reading is below 300 then we are paper out
  return (val < 300 ? 0 : 1);
}

void sendMessage (String mesg)
{
  if (client.connect (proxy, port))
  {
    Serial.println ("Sending " + mesg);
    
    client.println (mesg);  // Send to Proxy
  } 
  else
  {
    // if you didn't get a connection to the proxy
    Serial.println ("Connect to proxy failed");
    return;
  }

  // Get the return and display it
  while (true)
  {
    if (client.available ())
    {
      char c = client.read ();
      Serial.print (c);
      if (c == '\n')    // End of Message from Proxy
      {
        client.stop ();
        return;
      }
    }

    // if the server's disconnected, stop the client:
    if (!client.connected ())
    {
      client.stop ();
      return;
    }
  } 
}
