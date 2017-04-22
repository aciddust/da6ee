/*
// <Notice> 트위터 봇 생성
// Twitter client sketch for ENC28J60 based Ethernet Shield. Uses 
// arduino-tweet.appspot.com as a OAuth gateway.
// Step by step instructions:
// 
//  1. Get a oauth token:
//     http://arduino-tweet.appspot.com/oauth/twitter/login
//  2. Put the token value in the TOKEN define below
//  3. Run the sketch!
//
//  WARNING: Don't send more than 1 tweet per minute! (1분에 1개 이상의 트위터를 보내지 마시오)
//  NOTE: Twitter rejects tweets with identical content as dupes (returns 403)
*/
#include <Servo.h>
#include <SPI.h>
#include <EtherCard.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// 트위터 API 이용을 위해 OAUTH 토큰값을 넣는다. (트위터 ID/패스워드 필요)
// http://arduino-tweet.appspot.com/oauth/twitter/login 여기에서 트위터 로그인하고 토큰발급 받는다.
#define TOKEN   "853890302473453568-4jK5BilQKcP0zsYbf795HDwxpXEzRTb"

// tri-coloring LED [Need PWM Control _OR NOT] 
#define RGB_LED     6
#define NUMPIXELS   7
// servoMotor [Need PWM Control]
#define PIN_SERVO   9
#define PIN_SERVO_2 10
// LED_PANNEL [Need PWM Control]
#define PANNEL      5
// FAN [true: GO THROUGH / false: REVERSE] _ maybe need motorDriver
#define FAN_A       4
#define FAN_B       3
// Get Lux, Humidity
#define GetHum      A0
const int BH1750_address = 0x23;
byte luxBuf[2];

//LED INIT
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, RGB_LED, NEO_GRB + NEO_KHZ800);
boolean ledStatus;
boolean waterFeed;
boolean doorOpen;
boolean fanWork;

char* LED_statusLabel;
char* LED_buttonLabel;

char* DOOR_statusLabel;
char* DOOR_buttonLabel;

char* WATER_statusLabel;
char* WATER_buttonLabel;

char* on = "ON";
char* off = "OFF";

Servo door_servo;  // #define PIN_SERVO    9
Servo water_servo; // #define PIN_SERVO_2 10

int pos = 0;

// ethernet interface mac address, must be unique on the LAN
byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x63 };
 
const char website[] PROGMEM = "arduino-tweet.appspot.com";
static byte session;
byte Ethernet::buffer[700];
Stash stash;

void BH1750_Init(int address){
  
  Wire.beginTransmission(address);
  Wire.write(0x10); // 1 [lux] aufloesung
  Wire.endTransmission();
}

byte BH1750_Read(int address){
  
  byte i=0;
  Wire.beginTransmission(address);
  Wire.requestFrom(address, 2);
  while(Wire.available()){
    luxBuf[i] = Wire.read(); 
    i++;
  }
  Wire.endTransmission();  
  return i;
}
 
static void sendToTwitter (const char *tweet) {
  Serial.println("Sending tweet...");
  byte sd = stash.create();
 
  stash.print("token=");
  stash.print(TOKEN);
  stash.print("&status=");
  stash.println(tweet);
  stash.save();
  int stash_size = stash.size();
 
  // Compose the http POST request, taking the headers below and appending
  // previously created stash in the sd holder.
  Stash::prepare(PSTR("POST http://$F/update HTTP/1.0" "\r\n"
    "Host: $F" "\r\n"
    "Content-Length: $D" "\r\n"
    "\r\n"
    "$H"),
  website, website, stash_size, sd);
 
  // send the packet - this also releases all stash buffers once done
  // Save the session ID so we can watch for it in the main loop.
  session = ether.tcpSend();
}
 
void setup () {

// GPIO -set
  pinMode(RGB_LED, OUTPUT);
  pinMode(PANNEL, OUTPUT);
  pinMode(FAN_A, OUTPUT);
  pinMode(FAN_B, OUTPUT);
  pinMode(GetHum, INPUT);

// to get Lux
  Wire.begin();
  BH1750_Init(BH1750_address);

// init Servo
  water_servo.attach(PIN_SERVO);
  door_servo.attach(PIN_SERVO_2);

//Serial Begin..
  Serial.begin(57600);
  
//twitter -set  
  Serial.println("\n[Twitter Client]");
 
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println(F("Failed to access Ethernet controller"));
  if (!ether.dhcpSetup())
    Serial.println(F("DHCP failed"));
 
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
 
  if (!ether.dnsLookup(website))
    Serial.println(F("DNS failed"));
 
 ether.printIp("SRV: ", ether.hisip);
 // sendToTwitter("[Notice] da6ee operator has came back.");
}
 
void loop () {

  // 트위터 서버로 부터 받은 결과를 출력
 /* ether.packetLoop(ether.packetReceive());
  const char* reply = ether.tcpReply(session);
  if (reply != 0) {
    Serial.println("Got a response!");
    Serial.println(reply);
  }
  */

  // 조도 측정
  float valf = 0;
  if(BH1750_Read(BH1750_address)==2){
    valf=((luxBuf[0]<<8)|luxBuf[1])/1.2;
    
    if(valf<0)Serial.print("> 65535");
    else Serial.print((int)valf,DEC); 
    
    Serial.println(" lx"); 
  }

  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  if(pos) {
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?light=ON") != 0) {
      Serial.println("[LIGHT]: Received ON command");
      ledStatus = true;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?light=OFF") != 0) {
      Serial.println("[LIGHT]: Received OFF command");
      ledStatus = false;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?door=ON") != 0) {
      Serial.println("[DOOR]: Received ON command");
      doorOpen = true;
    }
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?door=OFF") != 0) {
      Serial.println("[DOOR]: Received OFF command");
      doorOpen = false;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?water=ON") != 0) {
      Serial.println("[WATER]: Received ON command");
      waterFeed = true;
    }
    
    if(ledStatus) {
      for(int i=0;i<NUMPIXELS;i++){
        pixels.setPixelColor(i, pixels.Color(255,255,255));
        pixels.show();
      }
      LED_statusLabel = on;
      LED_buttonLabel = off;
    } else {
      for(int i=0;i<NUMPIXELS;i++){
        pixels.setPixelColor(i, pixels.Color(0,0,0));
        pixels.show();
      }
      LED_statusLabel = off;
      LED_buttonLabel = on;
    }

    if(doorOpen) {
      door_servo.write(120);
      DOOR_statusLabel = on;
      DOOR_buttonLabel = off;
    } else {
      door_servo.write(1);
      DOOR_statusLabel = off;
      DOOR_buttonLabel = on;
    }

    if(waterFeed) {
      water_servo.write(120);
   // WATER_statusLabel = off;
      WATER_buttonLabel = on;
      delay(500);
      waterFeed = false;           
      water_servo.write(1);
    }
    else {
    ;
    }
    
    BufferFiller bfill = ether.tcpOffset();
    bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\n"
      "Content-Type: text/html\r\nPragma: no-cache\r\n\r\n"
      "<html>"
        "<head>"
          "<title>Da6eE</title>"
        "</head>"
        "<body>"
          "Light Status : $S "
            "<a href=\"/?light=$S\"><input type=\"button\" value=\"$S\"></a> <br>"
          "Door Status : $S "
            "<a href=\"/?door=$S\"><input type=\"button\" value=\"$S\"></a> <br>"
          "Watering? : _LAST TIME : [$S] "
            "<a href=\"/?water=$S\"><input type=\"button\" value=\"Feed\"></a> <br>"
            "</body></html>"            
      ), LED_statusLabel, LED_buttonLabel, LED_buttonLabel,
         DOOR_statusLabel, DOOR_buttonLabel, DOOR_buttonLabel,
         WATER_buttonLabel, WATER_buttonLabel);

    ether.httpServerReply(bfill.position());
  }
}
