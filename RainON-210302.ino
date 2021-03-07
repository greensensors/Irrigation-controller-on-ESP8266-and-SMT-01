/*
RainON - Irrigation Controller
Developed by Oleksander Savinykh, 
greensensorso@gmail.com

MC: ESP8266-12F
SMT-01 - Soil Moisture & Temperature Sensor
Relay HF3FD/5

IP: 192.168.4.1

*/

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <OneWire.h>

#define DARK  HIGH
#define LIGHT LOW
#define ON    HIGH
#define OFF   LOW

OneWire  ds(5);  // 1-Wire to GPIO5

byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];
float celsius;
int m_err;

unsigned long heating_time = 60000; // in ms

/* Set these to your desired credentials. */
const char *ssid = "RainON";       // name of the Controller
const char *password = "11111111";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;
String htmlstr =""; 

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

int pin_Analog = A0;                // analog input pin for Level sensor
int L_Sensor;                       // Level sensor value (0-1024)
int Level;                          // Level (0,1)

int pin_Led = 2;                    // Led indicator of ESP8266
int pin_Level = 14;                 // Power for Level sensor
int pin_REL_Water = 16;             // Relay signal
int pin_Heater = 4;                 // SMT-01 heater signal 

int DS_found = 0;                   // Initialization of DS18B20 result

float Soil_Temperature;             // measuring value
float Time_Heat_Dissipation;        // Time of Heat Dissipation (THD) measuring value
float sensorDRY = 250.0;            // THD value in DRY Soil
float sensorWET =  30.0;            // THD value in WET Soil
float Soil_Moisture;                // Soil Moisture % calculating value
float Start_level = 60.0;           // Soil Moisture% when Realay ON
float Fin_level = 70.0;             // Soil Moisture% when Realay OFF
int Rain;                           // Rain ON/OFF value
int rain_time = 10;                 // Watering Time period, с 
int rain_time_max = 900;            // Watering Max Time period, с (15 minutes)
int j, k;

// EEPROM coefficints
int smem = 512;                     
float kx = 0.0;
int l = 6;                          
char floatbufVar[7];
int Setup_data;

// Functions
void DS18B20_init(void);
void DS18B20_measure(void);
void Measure_SMT (void);
void Tla_measure (void);
void Rain_ON(void);   
void Header_f(void);
void HTML_page(void);
void ReadFromMem(void);
void WriteToMem(void);
void WiFiAccessPoint (void);

// Timing coefficients
unsigned long resettime;          
unsigned long mtime;              
unsigned long set_resettime = 86400000;     // Reset MC Time period, ms (one/day) 
unsigned long set_mtime;                    // Measurement Time period, ms
float mtimesetup = 5.0;                     // Measurement Time period, minutes


void setup()
{
   resettime  = millis();
   set_mtime = 5000;        // 1-st measurement will start after starting, the next measurements - every "mtimesetup" minutes
   mtime      = millis();
   
// pins setup   
   pinMode(pin_Led,OUTPUT);
   pinMode(pin_Heater, OUTPUT);
   pinMode(pin_REL_Water, OUTPUT);
   pinMode(pin_Level, OUTPUT);
   
// pins initialization
   digitalWrite(pin_Led,DARK);
   digitalWrite(pin_Heater, LOW);
   digitalWrite(pin_Level, LOW);
   digitalWrite(pin_REL_Water, OFF);
   
// UART communication setup  
   Serial.begin(115200);
   delay(10);
   Serial.println("");

// Reading setup coefficient from EEPROM
   //WriteToMem();    // need for first time
   ReadFromMem();

// Initialization of Thermometer of SMT-01 
   Serial.println("Initialization of DS18B20 ... ");

   DS_found = 0;
   DS18B20_init();
   if (DS_found == 1){
       digitalWrite(pin_Led, LIGHT); 
       Serial.println("Initialization is Ok");
       delay(1000);digitalWrite(pin_Led,DARK);
      }

// WiFi Initialization
   WiFi.mode(WIFI_AP_STA);
   WiFiAccessPoint ();

   server.begin();
   Serial.println("HTTP server started");

   header = "";
   Rain = 0;

  
}//setup

void loop()
{

  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
   
   if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
       client.println("HTTP/1.1 200 OK");
       client.println("Content-type:text/html");
       client.println("Connection: close");
       client.println();

// Checking changes in Setup
   Header_f();

// Display the HTML WEB page
   htmlstr ="";
   HTML_page();
   client.println(htmlstr);
              
// The HTTP response ends with another blank line
   client.println();
// Break out  
       break;
     }//if (currentLine.length() == 0) 
          
   else { // if you got a newline, then clear currentLine
         currentLine = ""; }
        
     }//if (c == '\n')
         
   else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      
     }//client.available()
   }//while (client.connected()
    
// Close the connection
   client.stop();
   Serial.println("Client disconnected.");
   Serial.println("");
    
  }// If a new client connects

  else { //If NOT NEW client connects  - Measurements ---------------------------------------

// if Setup coefficients are changed
if (Setup_data == 1) { WriteToMem(); }

if (millis() - mtime > set_mtime) 
     {
  
  digitalWrite(pin_Led, LIGHT);

// Temperature of Soil and THD measurements 
  Serial.println("Start measurement");
  Measure_SMT();
       
// Soil moisture calculations 
  Soil_Moisture = map(Time_Heat_Dissipation, sensorDRY, sensorWET, 0.0, 100.0);
  if (Soil_Moisture <   0.0) Soil_Moisture = 0.0;
  if (Soil_Moisture > 100.0) Soil_Moisture = 100.0;
  Serial.print("Soil_Moisture = ");
  Serial.println(Soil_Moisture);

// Water tank level measurement
  Tla_measure();
  Serial.print("Analog volume  = ");
  Serial.println(L_Sensor);
  Serial.print("Level = ");
  Serial.println(Level);
 
// Rain control
  if (Level == 1){ Rain_ON(); }

  if (Rain == 0) {set_mtime = (int)mtimesetup * 60 * 1000;}   // 5-60 minutes
  if (Rain == 1) {set_mtime = 5 * 60 * 1000;}                 // 5 minutes
  mtime = millis();

  Serial.print("Measurement Time period, ms  = ");
  Serial.println(set_mtime);

  
  digitalWrite(pin_Led,DARK);

 }// if mtime

  
 if (millis() - resettime > set_resettime) 
     {
      ESP.restart();
     }//if mills


    }// Measurements end --------------------------------------------------------------------- 

  delay(1);
   

}//loop

void DS18B20_init(void){ //-------------------------------------------------------------------

if ( !ds.search(addr)) {
    DS_found = 0;
    Serial.println("Sensor not found.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }//if

Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      DS_found = 0;
      return;
  }
  Serial.println();

// the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      DS_found = 1;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  }//switch

}//DS18B20_init 

void DS18B20_measure(void) {//------------------------------------------------------

  m_err = 0;
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);    // start conversion, with parasite power on at the end
  
  delay(1000);          // time need for conversion
    
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);       // Read Scratchpad

  //Serial.print("  Data = ");
  //Serial.print(present, HEX);
  //Serial.print(" ");
  for ( i = 0; i < 12; i++) {           // we need 12 bytes resolution
    data[i] = ds.read();
    //Serial.print(data[i], HEX);
    //Serial.print(" ");
  }
  //Serial.print(" CRC=");
  //Serial.print(OneWire::crc8(data, 8), HEX);
  //Serial.println();


 if (OneWire::crc8(data, 8) != data[8]) {
      Serial.println("CRC is not valid!");
      m_err = 1;
  }

  
  // Convert the data to actual temperature
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  
  celsius = (float)raw / 16.0;
  
  //Serial.print("  Temperature = ");
  //Serial.print(celsius);
  //Serial.print(" Celsius, ");
  //Serial.println();
  
 }//DS18B20_measure

void Measure_SMT ()
   {
    float t_current, t_last, tj;
    int m_cycle, j, m;
    unsigned long dtime;              

    Time_Heat_Dissipation = 0.0;
    j = 0;
    m = 0;
    tj = 0.0;

    Serial.println("Temperature of Soil measurement ...");
    for (m_cycle = 0; m_cycle<10; m_cycle++)
        { 
          DS18B20_measure();
          if (m_err == 0)
             {
              tj = tj + celsius; 
              j++; 
              Serial.println(celsius);
             }//if
          else {m++;}   
                    
         }//for

    if (j > 0 && m < 7)  { Soil_Temperature = tj/j; }
    else {Soil_Temperature = -21.0;} 

    Serial.print("Temperature of Soil = ");
    Serial.println(Soil_Temperature);
        
    if (Soil_Temperature > 0.0) {

    Serial.print("Heating ..."); 
    digitalWrite(pin_Heater, HIGH);
    delay(heating_time);    
    digitalWrite(pin_Heater, LOW);
    
    Serial.print("Heat dissipation ..."); 
    dtime = millis(); // start time 

    t_current = (Soil_Temperature + 5.0); 
    DS18B20_measure();
    if (m_err == 0) {t_current = celsius;}
    
    m_cycle = 0;
        
    while (t_current > (Soil_Temperature + 1.0) && m_cycle < 250)
        { 
         DS18B20_measure();
         if (m_err == 0) {t_current = celsius;}
         Serial.println(t_current );
         m_cycle++;
                  
        }//while


     Time_Heat_Dissipation = (millis() - dtime)/1000.0;
     Serial.print("Time of Heat Dissipation = ");
     Serial.println(Time_Heat_Dissipation);
     
    }//Soil_Temperature > 0
    
  if (m > 0) {Serial.println(m);}
   
}// Measure_SMT

void Tla_measure (void){
  
  // Water tank level measurement
  
  digitalWrite(pin_Level, HIGH);
  L_Sensor = analogRead(pin_Analog);
  digitalWrite(pin_Level, LOW);
     
  if(L_Sensor > 100) {Level = 1;}
  else {Level = 0;}
    
}//Tl_measure

void Rain_ON(void){   // Rain control
            
      if (Soil_Moisture < Fin_level && Rain == 1) 
       { 
        Rain = 1;
        Serial.println("Rain continue ...");
        digitalWrite(pin_REL_Water, ON);
        for (j=0; j<rain_time; j++)
            {
              Tla_measure ();
              if (Level == 1){digitalWrite(pin_REL_Water, ON);}
              else {digitalWrite(pin_REL_Water, OFF);}
              delay(1000);
            }//for
        digitalWrite(pin_REL_Water, OFF);
        Serial.println("Rain OFF");
      }
      
      if (Soil_Moisture < Start_level && Rain == 0)  
      { 
       Rain = 1;
       Serial.println("Rain start");
       digitalWrite(pin_REL_Water, ON);
       for (j=0; j< rain_time; j++)
            {
              Tla_measure ();
              if (Level == 1){digitalWrite(pin_REL_Water, ON);}
              else {digitalWrite(pin_REL_Water, OFF);}
              delay(1000);
            }//for
       
       digitalWrite(pin_REL_Water, OFF);
       Serial.println("Rain OFF");
      }
      
      if (Soil_Moisture >= Start_level && Rain == 0) 
       { 
       Rain = 0; 
       Serial.println("Rain not need");
       digitalWrite(pin_REL_Water, OFF);
       }
      
      if (Soil_Moisture >= Fin_level && Rain == 1) 
       { 
       Rain = 0; 
       Serial.println("Rain stop");
       digitalWrite(pin_REL_Water, OFF);
       }
     
}//Rain_ON

void Header_f(void){// Header handling

            int n;
            String hs;
            float htime;
            Setup_data = 0;

            String hd = header;
            hd.replace(" HTTP", "& "); 

            i = hd.indexOf("start-w");
            if ( i >= 0 && i < 150)
            {
              i = i + 8;
              j = i + 10; 
              hs = hd.substring(i, j); 
              n = hs.indexOf("&");
              if (n > 0) 
                 {
                  j = i + n;
                  hs = header.substring(i, j); 
                  hs.toCharArray(floatbufVar, l);
                  kx = atof(floatbufVar);
                  if (kx > 0.0 ) { Start_level = kx; Setup_data = 1;}
                  if (kx > 90.0) { Start_level = 90.0; }
                  }//if n 
             }//if i

            i = hd.indexOf("stop-w");
            if ( i >= 0 && i < 150)
            {
              i = i + 7;
              j = i + 9; 
              hs = hd.substring(i, j); 
              n = hs.indexOf("&");
              if (n > 0) 
                 {
                  j = i + n;
                  hs = header.substring(i, j); 
                  hs.toCharArray(floatbufVar, l);
                  kx = atof(floatbufVar);
                  if (kx > 0.0 ) { Fin_level = kx; Setup_data = 1;}
                  if (kx > 99.0) { Fin_level = 99.0; }
                 }//if n 
             }//if i
               
            i = hd.indexOf("timew");
            if ( i >= 0 && i < 150)
            {
              i = i + 6;
              j = i + 8; 
              hs = hd.substring(i, j); 
              n = hs.indexOf("&");
              if (n > 0) 
                 {
                  j = i + n;
                  hs = header.substring(i, j); 
                  i = hs.toInt();
                  if (i > 0) { rain_time = i; Setup_data = 1;}
                  if (rain_time > rain_time_max) { rain_time = rain_time_max;}
                  }//if n 
             }//if i

            i = hd.indexOf("sdry");
            if ( i >= 0 && i < 150)
            {
              i = i + 5;
              j = i + 7; 
              hs = hd.substring(i, j); 
              n = hs.indexOf("&");
              if (n > 0) 
                 {
                  j = i + n;
                  hs = header.substring(i, j); 
                  hs.toCharArray(floatbufVar, l);
                  kx = atof(floatbufVar);
                  if (kx > 0.0 ) { sensorDRY = kx; Setup_data = 1;}
                  if (kx > 250.0) { sensorDRY = 250.0; }
                 }//if n 
             }//if i
             

            i = hd.indexOf("swet");
            if ( i >= 0 && i < 150)
            {
              i = i + 5;
              j = i + 7; 
              hs = hd.substring(i, j); 
              n = hs.indexOf("&");
              if (n > 0) 
                 {
                  j = i + n;
                  hs = header.substring(i, j); 
                  hs.toCharArray(floatbufVar, l);
                  kx = atof(floatbufVar);
                  if (kx > 0.0 ) { sensorWET = kx; Setup_data = 1;}
                  if (kx < 30.0) { sensorWET = 30.0; }
                  }//if n 
             }//if i

            i = hd.indexOf("mtime");
            if ( i >= 0 && i < 150)
            {
              i = i + 6;
              j = i + 8; 
              hs = hd.substring(i, j); 
              n = hs.indexOf("&");
              if (n > 0) 
                 {
                  j = i + n;
                  hs = header.substring(i, j); 
                  hs.toCharArray(floatbufVar, l);
                  kx = atof(floatbufVar);
                  if (kx > 0.0)  { mtimesetup = kx; Setup_data = 1;}
                  if (kx < 5.0)  { mtimesetup = 5.0; }
                  if (kx > 60.0) { mtimesetup = 60.0; }
                 }//if n 
             }//if i
             
      // Clear the header variable
    header = "";
    
  }//void Header_f(void)   

 void HTML_page(void) {
 
 htmlstr += "<!DOCTYPE html>";
 htmlstr += "<html>";
 htmlstr += "<head>";
 htmlstr += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
 htmlstr += "<link rel=\"icon\" href=\"data:,\">";
 htmlstr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style>";
 htmlstr += "</head>";
 
 htmlstr += "<body>";
 htmlstr += "<h1>RainON Controller</h1>";

 htmlstr +="<p><b></b></p>";
 htmlstr +="<p><b>";
 htmlstr += ssid;
 htmlstr +="</b></p>";
 htmlstr +="<p><b></b></p>";
 
 htmlstr += "<table>";
 htmlstr += "<table align=\"center\" width=\"300\">";
 htmlstr += "<tr><th></th></tr>";
 htmlstr += "<tr><th align=\"left\">Parameter</th><th align=\"left\">Value</th></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Soil Moisture, %</td><td align=\"left\">";
 htmlstr += Soil_Moisture;
 htmlstr += "</td></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Soil Temperature, oC</td><td align=\"left\">";
 htmlstr += Soil_Temperature;
 htmlstr += "</td></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Tank level</td><td align=\"left\">";
 if (Level == 1){htmlstr += "Fine";}
 if (Level == 0){htmlstr += "Empty";}
 htmlstr += "</td></tr>";  
 htmlstr += "</table>";
 
 htmlstr += "<p><b></b></p>";
 htmlstr += "<p><b>Controller setup:</b></p>";
 htmlstr += "<p><b></b></p>";

 htmlstr += "<form id=\"setup\">";
 
 htmlstr += "<table>";
 htmlstr += "<table align=\"center\" width=\"300\">";
 htmlstr += "<tr><th></th></tr>";
 htmlstr += "<tr><th align=\"left\">Parameter</th><th align=\"left\">Value</th></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Start Rain at:</td><td align=\"left\">";
 htmlstr += "<input placeholder=";
 htmlstr += Start_level; 
 htmlstr += " name=\"start-w\" input type=\"text\" size=\"4\"";
 htmlstr += "</td></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Stop Rain at:</td><td align=\"left\">";
 htmlstr += "<input placeholder=";
 htmlstr += Fin_level;
 htmlstr += " name=\"stop-w\" input type=\"text\" size=\"4\"";
 htmlstr += "</td></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Time Rain, s:</td><td align=\"left\">";
 htmlstr += "<input placeholder=";
 htmlstr += rain_time;
 htmlstr += " name=\"timew\" input type=\"text\" size=\"4\"";
 htmlstr += "</td></tr>";  
 htmlstr += "</table>";

 htmlstr += "<p><input type=\"submit\" value=\"Submit\">";
 htmlstr += "   <input type=\"reset\" value=\"Reset\"></p>";
 
 htmlstr += "</form>";

 htmlstr += "<p><b></b></p>";
 htmlstr += "<p><b>Sensor setup:</b></p>";
 htmlstr += "<p><b></b></p>";

 htmlstr += "<table>";
 htmlstr += "<table align=\"center\" width=\"300\">";
 htmlstr += "<tr><th></th></tr>";
 htmlstr += "<tr><th align=\"left\">Parameter</th><th align=\"left\">Value</th></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Time of Heat Dissipation, s</td><td align=\"left\">";
 htmlstr += Time_Heat_Dissipation;
 htmlstr += "</td></tr>";
 htmlstr += "</table>";

 htmlstr += "<form id=\"sensor\">";
 
 htmlstr += "<table>";
 htmlstr += "<table align=\"center\" width=\"300\">";
 htmlstr += "<tr><th></th></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Sensor DRY:</td><td align=\"left\">";
 htmlstr += "<input placeholder=";
 htmlstr += sensorDRY; 
 htmlstr += " name=\"sdry\" input type=\"text\" size=\"4\"";
 htmlstr += "</td></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Sensor WET:</td><td align=\"left\">";
 htmlstr += "<input placeholder=";
 htmlstr += sensorWET;
 htmlstr += " name=\"swet\" input type=\"text\" size=\"4\"";
 htmlstr += "</td></tr>";
 htmlstr += "<tr><td align=\"left\" width=\"200\">Measurements period, min:</td><td align=\"left\">";
 htmlstr += "<input placeholder=";
 htmlstr += mtimesetup;
 htmlstr += " name=\"mtime\" input type=\"text\" size=\"4\"";
 htmlstr += "</td></tr>";  
 
 htmlstr += "</table>";

 htmlstr += "<p><input type=\"submit\" value=\"Submit\">";
 htmlstr += "   <input type=\"reset\" value=\"Reset\"></p>";
 
 htmlstr += "</form>";
 
  
 htmlstr += "</body>";
 htmlstr += "</html>";
 
 
 }//HTML_page  

void ReadFromMem(void){

     Serial.println("Reading coefficients from EPROM ... ");
     EEPROM.begin(smem);
     
     j = l*0;
     for (i=0; i<l; i++) {floatbufVar[i] = EEPROM.read(i+j);}
     kx = atof(floatbufVar); //преобразование строки в число float
     Start_level = kx;
     Serial.println(Start_level);

     j = l*1;
     for (i=0; i<l; i++) {floatbufVar[i] = EEPROM.read(i+j);}
     kx = atof(floatbufVar);
     Fin_level = kx;
     Serial.println(Fin_level);

     j = l*2;
     for (i=0; i<l; i++) {floatbufVar[i] = EEPROM.read(i+j);}
     kx = atof(floatbufVar);
     rain_time = (int)kx;
     Serial.println(rain_time);

     j = l*3;
     for (i=0; i<l; i++) {floatbufVar[i] = EEPROM.read(i+j);}
     kx = atof(floatbufVar);
     sensorDRY = kx;
     Serial.println(sensorDRY);

     j = l*4;
     for (i=0; i<l; i++) {floatbufVar[i] = EEPROM.read(i+j);}
     kx = atof(floatbufVar);
     sensorWET = kx;
     Serial.println(sensorWET);

     j = l*5;
     for (i=0; i<l; i++) {floatbufVar[i] = EEPROM.read(i+j);}
     kx = atof(floatbufVar);
     mtimesetup = kx;
     Serial.println(mtimesetup);

     
     EEPROM.end();
     Serial.println("Complete");  
     
}// ReadFromMem()   

void WriteToMem(void){
// Запись в память 
      EEPROM.begin(smem);
  
      Serial.println("Запись в память ... ");

      // преобразование числа K1 в строку floatbufVar
      j = l*0;
      kx = Start_level;
      dtostrf(kx, l, 1, floatbufVar);
      // запись строки floatbufVar в память
      for (i=0; i<l; i++) {EEPROM.write(i+j, floatbufVar[i]); }

      j = l*1;
      kx = Fin_level;
      dtostrf(kx, l, 1, floatbufVar);
      for (i=0; i<l; i++) {EEPROM.write(i+j, floatbufVar[i]); }

      j = l*2;
      kx = (float)(rain_time);
      dtostrf(kx, l, 1, floatbufVar);
      for (i=0; i<l; i++) {EEPROM.write(i+j, floatbufVar[i]); }

      j = l*3;
      kx = sensorDRY;
      dtostrf(kx, l, 1, floatbufVar);
      for (i=0; i<l; i++) {EEPROM.write(i+j, floatbufVar[i]); }

      j = l*4;
      kx = sensorWET;
      dtostrf(kx, l, 1, floatbufVar);
      for (i=0; i<l; i++) {EEPROM.write(i+j, floatbufVar[i]); }

      j = l*5;
      kx = mtimesetup;
      dtostrf(kx, l, 1, floatbufVar);
      for (i=0; i<l; i++) {EEPROM.write(i+j, floatbufVar[i]); }
      

      EEPROM.commit();
      EEPROM.end();
      Serial.println("Write complete");

      Setup_data = 0;
      
}//WriteToMem

void WiFiAccessPoint (void){

  Serial.print("Configuring access point...");
  boolean result = WiFi.softAP(ssid, password);
  if(result == true){Serial.println("Ready");}
  else {Serial.println("Failed!");}
    
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  
}//WiFiAccessPoint
