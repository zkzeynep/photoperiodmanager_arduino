#include <SPI.h>
#include <WString.h>
#include <DHT.h>
#include <Wire.h> //BH1750 IIC Mode 
#include <math.h> 
#include <stdio.h>
#include <string.h>
#include <DS1302.h>
#include <Ethernet.h>

//Ethernet Shield
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x94, 0xF8 };
byte ip[] = {
  192,168,1,33};
byte gateway[] = { 
  192, 168, 1, 1 };
byte subnet[] = { 
  255, 255, 255, 0 };
EthernetServer server(80);
EthernetClient client;
String HTTPreq;            // stores the HTTP request
//IPAddress webservice(173,248,230,248); // home
IPAddress webservice(192,168,1,19); // local
int port = 85;
const unsigned long requestInterval = 600000; //600000 = 10 min
unsigned long lastAttemptTime = 0;  

/* Set the appropriate digital clock */
uint8_t CE_PIN   = 8;
uint8_t IO_PIN   = 7;
uint8_t SCLK_PIN = A5;
char buf[50];
char day[10];
DS1302 rtc(CE_PIN, IO_PIN, SCLK_PIN);

//Temperature sensor
#define DHT_PIN 9   
#define DHT_TYPE DHT11 
//#define DHT_TYPE DHT22   // DHT 22  (AM2302)
//#define DHT_TYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHT_PIN, DHT_TYPE);

//Moisture sensor
#define moisture A0

//Relays
#define RELAY_ON 0
#define RELAY_OFF 1
#define Relay_1  A1 
#define Relay_2  A2
#define Relay_3  A3
#define Relay_4  A4

//Motor
#define PUMP 6  
int motorDelay;

//MESSAGES
#define MSG_METHOD_SUCCESS 0                      //Code which is used when an operation terminated  successfully
#define MSG_SERIAL_CONNECTED 1                    //Code which is used to indicate that a new serial connection was established
//WARNINGS
#define WRG_NO_SERIAL_DATA_AVAIBLE 250            //Code indicates that no new data is avaible at the serial input buffer
//ERRORS
#define ERR_SERIAL_IN_COMMAND_NOT_TERMINATED -1
//Read serial
int readSerialInputString(String *command);
//LightSensor
int BH1750address = 0x23; //setting i2c address
byte buff[2];


void setup() {
  delay(50);
  Serial.begin(9600);
  //Serial1.begin(9600);
  Wire.begin();
  dht.begin();
  Ethernet.begin(mac,ip,gateway,subnet);
  server.begin();
  //Relay
  digitalWrite(Relay_1, RELAY_OFF);
  digitalWrite(Relay_2, RELAY_OFF);
  digitalWrite(Relay_3, RELAY_OFF);
  digitalWrite(Relay_4, RELAY_OFF);
  pinMode(Relay_1, OUTPUT);   
  pinMode(Relay_2, OUTPUT);  
  pinMode(Relay_3, OUTPUT);  
  pinMode(Relay_4, OUTPUT);

  //Motor
  digitalWrite (PUMP,LOW);   
  pinMode(PUMP,OUTPUT);
  motorDelay = 1000;
  
  
  //Time
   /* Initialize a new chip by turning off write protection and clearing the
     clock halt flag. These methods needn't always be called. See the DS1302
     datasheet for details. */
  //rtc.write_protect(false);
  //rtc.halt(false);
  /*   yyyy, m ,d ,hh(24), m,s, dayweek(0-sunday) */
  //Time t(2013, 9, 21, 22, 35, 0, 6);
  //rtc.time(t);
}


void loop() {
  client.stop();
  
  //dht11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  //relay
  int r1 = digitalRead(Relay_1);
  int r2 = digitalRead(Relay_2);
  int r3 = digitalRead(Relay_3);
  int r4 = digitalRead(Relay_4);

  //Motor
  int motor = digitalRead(PUMP); 

  //Moisture
  //0-300 dry
  //300-700 humid
  // 700-950 in water
  float m = analogRead(moisture);

 //time
  Time c = rtc.time();
  memset(day, 0, sizeof(day));  /* clear day buffer */
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           c.yr, c.mon, c.date,
           c.hr, c.min, c.sec);
Serial.print("DateTime: ");
//Serial1.print("DateTime: ");
Serial.println(buf);
//Serial1.println(buf);

  //LightSensor
  int i;
  uint16_t val=0;
  BH1750_Init(BH1750address);
  if(2==BH1750_Read(BH1750address))
  {
    val=((buff[0]<<8)|buff[1])/1.2;
    Serial.print("Light: ");
    Serial.print(val,DEC);     
    Serial.println("[lx]"); 
  }

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT");
  } 
  else {
    Serial.print("Humidity: "); 
    Serial.print(h);
    Serial.println(" %");
    Serial.print("Temperature: "); 
    Serial.print(t);
    Serial.println(" *C");
  }

  //Moisture
  if (isnan(m)) {
    Serial.println("Failed to read from Moisture Sensor");
  } 
  else {
    Serial.print("Moisture: ");
    Serial.print(m/10);
    Serial.println(" %");
  }

  if (isnan(r1) || isnan(r2) || isnan(r3) || isnan(r4) ) {
    Serial.println("Failed to read from relays");
  } 
  else {
    Serial.print("Relay: ");
    Serial.print(r1);
    Serial.print(r2);
    Serial.print(r3);
    Serial.println(r4);
  }
  if (isnan(motor)) {
    Serial.println("Failed to read from motor");
  } 
  else {
    Serial.print("Motor: ");
    Serial.println(motor);
    Serial.print("Motor Delay: ");
    Serial.println(motorDelay); 
  }

  String command = "";
  int serialResult = 0; 
  serialResult = readSerialInputCommand(&command);
  if(serialResult == WRG_NO_SERIAL_DATA_AVAIBLE)
  {
    //If there is no data avaible at the serial port
    Serial.println("Listening! Ver.2.02");
  }
  else
  {
    if(serialResult == ERR_SERIAL_IN_COMMAND_NOT_TERMINATED)
    {
      Serial.println("Unrecognized command!");
    }
    else
    {
      processCommand(command);
    }
  }
//web server
client = server.available();  // try to get client
    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                HTTPreq += c;  // save the HTTP request 1 char at a time
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: keep-alive");
                    client.println();
                    if (HTTPreq.indexOf("command") > -1) {
                        // read switch state and send appropriate paragraph text
                        //todo commands
                        int ind1 = HTTPreq.indexOf('=');
                        int ind2 = HTTPreq.indexOf('&');
                        String fs = HTTPreq.substring(ind1+1, ind2);
			fs += "#";
			processCommand(fs);
			//client.println(finalstring);
                        //client.println(HTTP_req);
                    }
                    //else {
                        client.println("<!DOCTYPE html>");
                        client.println("<html>");
                        client.println("<head>");
                        client.println("<title>PhotoperiodManager</title>");
                        client.println("</head>");
                        client.println("<body>");
                        client.println("<h3>System status:</h3>");
                        client.print("<p> Relays :");
                        client.print(r1);
                        client.print(r2);
                        client.print(r3);
                        client.print(r4);
                        client.println("</p>");
                        client.print("<p> Light: ");
                        client.print(val,DEC);     
                        //client.println("[lx]");
                        client.println(" lux</p>");
                        client.print("<p>Motor: ");
                        client.println(motor);
                        client.println("</p>");
                        client.print("<p>Motor delay: ");
                        client.println(motorDelay);  
                        client.println("ms</p>");
                        client.print("<p>Moisture: ");
                        client.print(m/10);
                        //client.println(" %");
                        client.println("%</p>");
                        client.print("<p>Humidity: "); 
                        client.print(h);
                        //client.println(" %");
                        client.println("%</p>");
                        client.print("<p>Temperature: "); 
                        client.print(t);
                        //client.println(" *C");
                        client.println("*C</p>");
                        client.print("<p>DateTime: ");
                        client.println(buf);
                        client.println("</p>");
                        client.println("</body>");
                        client.println("</html>");
                    //}
                    // display received HTTP re
		    //Serial.print(HTTP_req);
                    HTTPreq = "";
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } 
        } 
        delay(1); 
        client.stop(); 
        
 } 
    //end web server
    //web client
  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data:
  if(!client.connected() && (millis() - lastAttemptTime > requestInterval)) {
        // attempt to connect, and wait a millisecond:
      if (client.connect(webservice, port)) {
        Serial.println("making HTTP request...");
        //client.println("GET /LogData?temperature=");
        //client.print("GET /PhotoPeriodWebService.asmx/CreateLogRecord?time=");
        //client.println(buf);
        client.print("GET /PhotoPeriodWebService.asmx/CreateLogRecord?temp=");
        client.print(t);
        client.print("&devtime=");
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           c.hr, c.min, c.sec);
        client.print(buf);
        client.print("&devdate=");
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           c.yr, c.mon, c.date);
        client.print(buf);
        client.print("&light=");
        client.print(val, DEC);
        client.print("&moisture=");
        client.print(m/10);
        client.print("&motor=");
        client.print(motor);
        client.print("&motordelay=");
        client.print(motorDelay);
        client.print("&r1=");
        client.print(r1);
        client.print("&r2=");
        client.print(r2);
        client.print("&r3=");
        client.print(r3);
        client.print("&r4=");
        client.print(r4);
        client.print("&humid=");
        client.println(h);
        client.println(" HTTP/1.1");
        client.println("Host: http://192.168.1.33");
        client.println("Connection: close");
        client.println(); 
      } 
      else
      {
         Serial.println("No logging API connection, request failed!");
      }
      
      // note the time of this connect attempt:
      lastAttemptTime = millis();
      delay(1);
      client.stop(); 
  }
  //end web client
   
//end loop
delay(1000);
}

void processCommand(String command)
{
      command.toUpperCase();
      if (command.equals("R1ON#"))
      {   
        digitalWrite(Relay_1, RELAY_ON);
      }
      if (command.equals("R1OFF#"))
      {    
        digitalWrite(Relay_1, RELAY_OFF);
      }
      if (command.equals("R2ON#"))
      {   
        digitalWrite(Relay_2, RELAY_ON);
      }
      if (command.equals("R2OFF#"))
      {   
        digitalWrite(Relay_2, RELAY_OFF);
      }
      if (command.equals("R3ON#"))
      {    
        digitalWrite(Relay_3, RELAY_ON);
      }
      if (command.equals("R3OFF#"))
      {   
        digitalWrite(Relay_3, RELAY_OFF);
      }
      if (command.equals("R4ON#"))
      {   
        digitalWrite(Relay_4, RELAY_ON);
      }
      if (command.equals("R4OFF#"))
      {   
        digitalWrite(Relay_4, RELAY_OFF);
      }
      if (command.equals("OFF#"))
      {
        digitalWrite(PUMP,LOW); 
        digitalWrite(Relay_1, RELAY_OFF); 
        digitalWrite(Relay_2, RELAY_OFF); 
        digitalWrite(Relay_3, RELAY_OFF); 
        digitalWrite(Relay_4, RELAY_OFF); 
      }
      if (command.equals("RELAYSON#"))
      {
        digitalWrite(Relay_1, RELAY_ON);
        digitalWrite(Relay_2, RELAY_ON);
        digitalWrite(Relay_3, RELAY_ON);
        digitalWrite(Relay_4, RELAY_ON);
      }
      if (command.equals("PUMPUP#"))
      {
        motorDelay = motorDelay + 1000;
      }
      if (command.equals("PUMPDOWN#"))
      {
        motorDelay = motorDelay - 1000; 
      }
      if (command.equals("PUMPDELAY#"))
      {
        potor(motorDelay);
      }
      if (command.equals("PUMPON#"))
      {
        digitalWrite(PUMP,HIGH);
      }
      if (command.equals("PUMPOFF#"))
      {      
        digitalWrite(PUMP,LOW); 
      }
}

int readSerialInputCommand(String *command)
{
  int operationStatus = MSG_METHOD_SUCCESS; //Default return is MSG_METHOD_SUCCESS reading data from com buffer.
  //check if serial data is available for reading
  if (Serial.available())
  {      
    char serialInByte;
    //temporary variable to hold the last serial input buffer character
    do{
      //Read serial input buffer data byte by byte 
      serialInByte = Serial.read();
      *command = *command + serialInByte;
      //Add last read serial input buffer byte to *command pointer
    }
    while(serialInByte != '#' && Serial.available());
    //until '#' comes up and serial data is avaible
    if(((String)(*command)).indexOf('#') < 1) {
      operationStatus = ERR_SERIAL_IN_COMMAND_NOT_TERMINATED;
    }
  }
  else
  {
      //If not serial input buffer data is avaible, operationStatus becomes WRG_NO_SERIAL_DATA_AVAIBLE (= No data in the serial input buffer avaible)
      operationStatus = WRG_NO_SERIAL_DATA_AVAIBLE;   
  }
  return operationStatus;
}

void potor(int delayValue)
{   
  digitalWrite(PUMP,HIGH);
  Serial.println("Motor ON ");
  delay(delayValue);  
  digitalWrite(PUMP,LOW);
  Serial.println("Motor OFF ");      
}



int BH1750_Read(int address) 
{
  int i=0;
  Wire.beginTransmission(address);
  Wire.requestFrom(address, 2);
  while(Wire.available()) 
  {
    buff[i] = Wire.read();
    i++;
  }
  Wire.endTransmission();  
  return i;
}

void BH1750_Init(int address) 
{
  Wire.beginTransmission(address);
  Wire.write(0x10);//1lx reolution 120ms
  Wire.endTransmission();
}




