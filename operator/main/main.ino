#include <Servo.h>
#include <SPI.h>
#include <EtherCard.h>
#include <Wire.h>

// tri-coloring LED [Need PWM Control _OR NOT] 
#define RGB_LED     6
#define NUMPIXELS   7
// servoMotor [Need PWM Control]
#define PIN_SERVO   9

// FAN [Only one way]
#define FAN         4

// Get Humidity, Celsius
#define GetHum      A0
#define GetTmp      A2

// Get Lux
const int BH1750_address = 0x23; 
byte luxBuf[2];

boolean ledStatus = false;
boolean waterFeed = false;
boolean fanWork = false;

char* LED_statusLabel;
char* LED_buttonLabel;
char* WATER_statusLabel;
char* WATER_buttonLabel;
char* FAN_statusLabel;
char* FAN_buttonLabel;

char* on = "ON";
char* off = "OFF";

Servo water_servo; // #define PIN_SERVO_2 10

int pos = 0;

// ethernet interface mac address, must be unique on the LAN
byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x63 };
 
const char website[] PROGMEM = "arduino-tweet.appspot.com";
static byte session;
byte Ethernet::buffer[650];
//Stash stash;

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
void setup () {

// GPIO -set
  pinMode(RGB_LED, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(GetHum, INPUT);
  pinMode(GetTmp, INPUT);
  
// to get Lux
  Wire.begin();
  BH1750_Init(BH1750_address);

// init Servo
  water_servo.attach(PIN_SERVO);

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
}
 
void loop () {

  // 조도 측정
  float valf = 0;
  if(BH1750_Read(BH1750_address)==2){
    valf=((luxBuf[0]<<8)|luxBuf[1])/1.2;

    /*
    if(valf<0)Serial.print("> 65535");
    else Serial.print((int)valf,DEC); 
    
    Serial.println(" lx"); 
    */
  }

  // 온도 측정 (섭씨기준)
  int tmp_read = analogRead(GetTmp);
  float tmp_vcc = tmp_read * 5.0 / 1024.0;
  float celsiustmp = (tmp_vcc - 0.5) * 100 ; 
  // float fahrenheittmp= celsiustmp * 9.0/5.0 + 32.0; // 화씨

  // 토양 내부 습도 측정
  int hum_read = analogRead(GetHum); // 센서 감도는 알아서 조절

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

    if(strstr((char *)Ethernet::buffer + pos, "GET /?water=ON") != 0) {
      Serial.println("[WATER]: Received ON command");
      waterFeed = true;
      water_servo.write(120);
      delay(1000);
      water_servo.write(1);
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
    
    BufferFiller bfill = ether.tcpOffset();
    bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\n"
      "Content-Type: text/html\r\nPragma: no-cache\r\n\r\n"
      "<html>"
        "<head>"
          "<meta charset=\"utf-8\">"
          "<title>Da6eE</title>"
        "</head>"
        "<body>"
          "LIGHT : $S "
            "<a href=\"/?light=$S\"><input type=\"button\" value=\"$S\"></a> <br><br>"
          "FAN: $S "
            "<a href=\"/?fan=$S\"><input type=\"button\" value=\"$S\"></a> <br><br>"  
          "WATER : "
            "<a href=\"/?water=$S\"><input type=\"button\" value=\"Feed\"></a>  <br>"
            "_LAST TIME : [_RTC 장착 이후 마지막 물준 시간 기록예정_] <br><br>"
          "Lux : $S &nbsp&nbsp Tmp : $S &nbsp&nbsp <br><br>"
          "Hum : $S<br>"
            "</body></html>"            
      ), LED_statusLabel, LED_buttonLabel, LED_buttonLabel,
         FAN_statusLabel, FAN_buttonLabel, FAN_buttonLabel,
         WATER_buttonLabel,
         "_조도_","_온도_","_습도_");

    ether.httpServerReply(bfill.position());
  }
}
