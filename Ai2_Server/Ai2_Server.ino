/*
 * ESP8266 Web server with Web Socket to control an LED.
 *
 * The web server keeps all clients' LED status up to date and any client may
 * turn the LED on or off.
 *
 * For example, clientA connects and turns the LED on. This changes the word
 * "LED" on the web page to the color red. When clientB connects, the word
 * "LED" will be red since the server knows the LED is on.  When clientB turns
 * the LED off, the word LED changes color to black on clientA and clientB web
 * pages.
 *
 * References:
 *
 * https://github.com/Links2004/arduinoWebSockets
 *
 */

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <stdio.h>
#include <Servo.h>
#include <EEPROM.h>
#include "webpage.h"


static const char ssid[] = "oad";
static const char password[] = "Reimiiz0";
MDNSResponder mdns;

static void handleInput(uint8_t* data, size_t length, IPAddress ip);
static void writeROM(char* data, int addr);

ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// GPIO#0 is for Adafruit ESP8266 HUZZAH board. Your board LED might be on 13.
const int LEDPIN = 0;
// Current LED status
bool lightStatus[3] = {false};
int gateStatus = 0;
char dataOut[5] = {'0'};
char usr[16], pwd[16], ssusr[16], sspwd[16];
// Commands sent through Web Socket
bool logOn[256] = {false};
const char LEDON[] = "ns";
const char LEDOFF[] = "fs";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  IPAddress ip = webSocket.remoteIP(num);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
  
        // Send the current LED status
        char conchar[20];
        for(int i = 0; i < 4; i++){
          if (lightStatus[i]) {
            sprintf(conchar, "%s%i", LEDON, i);
            webSocket.sendTXT(num, conchar, 3);
          }
          else {
            sprintf(conchar, "%s%i", LEDOFF, i);
            webSocket.sendTXT(num, conchar, 3);
          }
        }
		if (gateStatus == 2) {
			webSocket.sendTXT(num, "ng0", 3);
		}
		else if (gateStatus == 1) {
			webSocket.sendTXT(num, "fg0", 3);
		}
		else {
			webSocket.sendTXT(num, "sg0", 3);
		}
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\r\n", num, payload);
      
      handleInput(payload, length, ip);
      
      // send data to all connected clients
      webSocket.broadcastTXT(payload, length);
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}
void handleRoot()
{
  IPAddress ip = server.client().remoteIP();
  if(logOn[(int)ip[3]]){
    server.send_P(200, "text/html", INDEX_HTML);
  }else{
    server.send_P(200, "text/html", LOGIN_HTML);
  }
  
}
void handleMain()
{
    server.send_P(200, "text/html", INDEX_HTML);
}
void handleTooMany()
{
  String message = "Too Many User\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

static void handleInput(uint8_t* data, size_t length, IPAddress ip)
{
  if(data[0] == 'x' && data[1] == 'x'){
    for(int i = 0; i < strlen(usr); i++){
      if(data[i+2] != usr[i]) return;
    }
    for(int i = 0; i < strlen(pwd); i++){
      if(data[i+2+strlen(usr)] != pwd[i]) return;
    }
    logOn[(int)ip[3]] = true;
  }else if(data[0] == 'l' && data[1] == 'o'){
    logOn[(int)ip[3]] = false;
  }else if(data[0] == 'p' && data[1] == 'p'){
    for(int i = 0; i < strlen(pwd); i++){
      if(data[i+2] != pwd[i]) return;
    }
    for (int i = 16; i < 32; i++) {
      EEPROM.write(i, 0);
    }
    for(int i = 0; i < length-(strlen(pwd)+2); i++){
      EEPROM.write(i+16, data[i+2+strlen(pwd)]);
    }

  }else{
	  if(data[1] == 's') lightStatus[data[2]-49] = ((char)data[0] == 'n') ? true : false;
	  else {
		  if (data[0] == 'n') gateStatus = 2;
		  else if (data[0] == 'f') gateStatus = 1;
		  else gateStatus = 0;
	  }
  }
  dataOut[0] = lightStatus[0] ? '1' : '0';
  dataOut[1] = lightStatus[1] ? '1' : '0';
  dataOut[2] = lightStatus[2] ? '1' : '0';
  dataOut[3] = (char)(48+gateStatus);
  
  
}
void writeROM(char* data, int addr) {
	for (int i = addr; i < addr + 16; i++) {
		EEPROM.write(i, 0);
	}
	for (int i = addr; i < addr+strlen(data); i++) {
		EEPROM.write(i, data[i-addr]);
	}
	EEPROM.commit();
}
void setup()
{
  pinMode(LEDPIN, OUTPUT);
  Serial1.begin(115200, SERIAL_8N1);
  Serial.begin(115200);
  EEPROM.begin(512);
  //Serial.setDebugOutput(true);
  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\r\n", t);
    Serial.flush();
    delay(1000);
  }

  /*
  char tmp;
  char s[16];
  int i = 0;
  while(true){
    tmp = (char)Serial.read();
    if(tmp == 'a'){
        Serial.print("SSID: ");
        while(i < 16){
          tmp = Serial.read();
          Serial.print(tmp);
          if(Serial.available() > 0){
            if(tmp == '\r') break;
            s[i] = tmp;
            i++;
          }
        }   
        writeROM(s, 32);
        i = 0;
    
       Serial.print("Password: ");
       while(i < 16){
         tmp = Serial.read();
          if(Serial.available() > 0){
            if(tmp == '\r') break;
            s[i] = tmp;
            i++;
          }
        }
        writeROM(s, 48);
        break;
    }else if(tmp == 'a'){
      break;
    }
    delay(10);
  }*/

  
  for (int i = 0; i < 16; i++) {
    usr[i] = EEPROM.read(i);
  }
  for (int i = 16; i < 32; i++) {
    pwd[i-16] = EEPROM.read(i);
  }
  for (int i = 32; i < 48; i++) {
	  ssusr[i-32] = EEPROM.read(i);
  }
  for (int i = 48; i < 64; i++) {
	  sspwd[i-48] = EEPROM.read(i);
  }

  WiFiMulti.addAP(ssusr, sspwd);

  while(WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("ai2esp", WiFi.localIP())) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else {
    Serial.println("MDNS.begin failed");
  }
  Serial.print("Connect to http://ai2esp.local or http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  dataOut[4] = '\r';
  Serial1.print(dataOut);
}

void loop()
{
  Serial1.print(dataOut);
    webSocket.loop();
    server.handleClient();
}
