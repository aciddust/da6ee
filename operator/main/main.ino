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

/*
// DS1302:  CE pin    -> Arduino Digital 2
//          I/O pin   -> Arduino Digital 3
//          SCLK pin  -> Arduino Digital 7
*/

#include <DS1302.h>
#include <Servo.h>
#include <SPI.h>
#include <EtherCard.h>
#include <Wire.h>
//#include <Adafruit_NeoPixel.h>

// 트위터 API 이용을 위해 OAUTH 토큰값을 넣는다. (트위터 ID/패스워드 필요)
// http://arduino-tweet.appspot.com/oauth/twitter/login 여기에서 트위터 로그인하고 토큰발급 받는다.
#define TOKEN   "853890302473453568-4jK5BilQKcP0zsYbf795HDwxpXEzRTb"

// tri-coloring LED [Need PWM Control _OR NOT] 
#define RGB_LED     6
#define NUMPIXELS   7

// servoMotor [Need PWM Control]
#define PIN_SERVO   9
//#define PIN_SERVO_2 10

// LED_PANNEL [Need PWM Control]
//#define PANNEL      5

// FAN [Only one way]
#define FAN         4

// Get Humidity, Celsius
#define GetHum      A0
#define GetTmp      A2

// Get Lux
const int BH1750_address = 0x23; 
byte luxBuf[2];

//Tri-Coloring LED INIT
//Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, RGB_LED, NEO_GRB + NEO_KHZ800);

//RTC Set
DS1302 rtc(2,3,7);

boolean ledStatus = false;
boolean waterFeed = false;
//boolean doorOpen = false;
boolean fanWork = false;
boolean dummy = false;

boolean isHeCome = false;

char* LED_statusLabel;
char* LED_buttonLabel;

/*
char* DOOR_statusLabel;
char* DOOR_buttonLabel;
*/

char* WATER_statusLabel;
char* WATER_buttonLabel;

char* FAN_statusLabel;
char* FAN_buttonLabel;

char* on = "ON";
char* off = "OFF";

unsigned long prev_time;

char timeData[20]="";
char luxData[4]="";
char tmpData[4]="";
char humData[4]="";

Servo door_servo;  // #define PIN_SERVO    9
Servo water_servo; // #define PIN_SERVO_2 10

int pos = 0;

// ethernet interface mac address, must be unique on the LAN
byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x63 };
 
const char website[] PROGMEM = "arduino-tweet.appspot.com";
static byte session;
byte Ethernet::buffer[600];
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

void getTimeData(void) {
  memset(timeData, 0, sizeof(timeData));
  strcat(timeData, rtc.getDOWStr());
  strcat(timeData, rtc.getDateStr());
  strcat(timeData, rtc.getTimeStr());
}
 
void setup () {

// GPIO -set
  pinMode(RGB_LED, OUTPUT);
  //pinMode(PANNEL, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(GetHum, INPUT);
  pinMode(GetTmp, INPUT);
  
// to get Lux
  Wire.begin();
  BH1750_Init(BH1750_address);

// init Servo
  water_servo.attach(PIN_SERVO);
//  door_servo.attach(PIN_SERVO_2);

// RTC_ Set the clock to run-mode, and disable the write protection
  rtc.halt(false);
  rtc.writeProtect(false);
  
  //init time;
  getTimeData(); 
  
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
  float lux_read = 0;
  if(BH1750_Read(BH1750_address)==2){
    lux_read=((luxBuf[0]<<8)|luxBuf[1])/1.2;
    
    if(lux_read<0)Serial.print("> 65535");
    else Serial.print((int)lux_read,DEC); 
    
    Serial.println(" lx"); 
  }

  // 온도 측정 (섭씨기준)
  int tmp_read = analogRead(GetTmp);
  float tmp_vcc = tmp_read * 5.0 / 1024.0;
  float celsiustmp = (tmp_vcc - 0.5) * 100 ; 
  // float fahrenheittmp= celsiustmp * 9.0/5.0 + 32.0; // 화씨

  // 토양 내부 습도 측정
  float hum_read = analogRead(GetHum); // 센서 감도는 알아서 조절

  
  memset(luxData, 0 , sizeof(luxData));
  memset(tmpData, 0 , sizeof(tmpData));
  memset(humData, 0 , sizeof(humData));
  sprintf(luxData, "%f", lux_read);
  sprintf(tmpData, "%f", celsiustmp);
  sprintf(humData, "%f", hum_read);

  //웹 페이지 받을 준비
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  unsigned long prev_time=0;
  unsigned long current_time;
  if(pos) {
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?light=ON") != 0) {
      Serial.println("[LIGHT]: Received ON command");
      ledStatus = true;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?light=OFF") != 0) {
      Serial.println("[LIGHT]: Received OFF command");
      ledStatus = false;
    }
/*
    if(strstr((char *)Ethernet::buffer + pos, "GET /?door=ON") != 0) {
      Serial.println("[DOOR]: Received ON command");
      doorOpen = true;
    }
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?door=OFF") != 0) {
      Serial.println("[DOOR]: Received OFF command");
      doorOpen = false;
    }
*/
    if(strstr((char *)Ethernet::buffer + pos, "GET /?water=ON") != 0) {
      Serial.println("[WATER]: Received ON command");
      waterFeed = true;
      water_servo.write(120);
      delay(1000);
      water_servo.write(1);

      getTimeData(); 
    }
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?water=OFF") != 0) {
      Serial.println("[WATER]: Received OFF command");
      waterFeed = false;
      water_servo.write(120);
      delay(1000);
      water_servo.write(1);
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?fan=ON") != 0) {
      Serial.println("[FAN]: Received ON command");
      fanWork = true;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?fan=OFF") != 0) {
      Serial.println("[FAN]: Received OFF command");
      fanWork = false;
    }

    
    
    if(ledStatus) {
      digitalWrite(RGB_LED, HIGH);
      LED_statusLabel = on;
      LED_buttonLabel = off;
    } else {
      digitalWrite(RGB_LED, LOW);
      LED_statusLabel = off;
      LED_buttonLabel = on;
    }

    if(waterFeed) {
      WATER_statusLabel = on;
      WATER_buttonLabel = off;
    } else {
      WATER_statusLabel = off;
      WATER_buttonLabel = on;
    }
    
    if(fanWork) {
      digitalWrite(FAN, HIGH);
      FAN_statusLabel = on;
      FAN_buttonLabel = off;
    } else {
      digitalWrite(FAN, LOW);
      FAN_statusLabel = off;
      FAN_buttonLabel = on;
    }  

////////////////////////////////////////////////////////////////////////////////////
   //다음 개발은 여기부터. (접속 여부 판단에 따른 물주기나 자동제어 부분)
    if(!isHeCome){
      
    }
    else{
      
    }
////////////////////////////////////////////////////////////////////////////////////
   
    BufferFiller bfill = ether.tcpOffset();
    bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\n"
      "Content-Type:text/html\r\nPragma:no-cache\r\n\r\n"
      "<html>"
        "<head>"
          "<meta charset=\"utf-8\">"
          "<title>Da6eE</title>"
        "</head>"
        "<body>"
          "내부 조명: $S"
            "<a href=\"/?light=$S\"><input type=\"button\" value=\"$S\"></a><br>"
          "팬 작동: $S "
            "<a href=\"/?fan=$S\"><input type=\"button\" value=\"$S\"></a><br>"  
          "물??: "
            "<a href=\"/?water=$S\"><input type=\"button\" value=\"Feed\"></a><br>"
            "LAST TIME: $S<br><br>"
          "주변환경:<br>"
            "조도: $S<br>온도: $S<br>화분습도: $S<br>"
            "</body></html>"            
      ), LED_statusLabel, LED_buttonLabel, LED_buttonLabel,
         FAN_statusLabel, FAN_buttonLabel, FAN_buttonLabel,
         WATER_buttonLabel, timeData,
         luxData, tmpData, humData);

    ether.httpServerReply(bfill.position());
  }
}
