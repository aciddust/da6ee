#include <DS1302.h>
#include <Servo.h>
#include <SPI.h>
#include <EtherCard.h>
#include <Wire.h>

//LED _ Blue
#define LED_B       6

// servoMotor [Need PWM Control]
#define PIN_SERVO   9

// FAN [Only one way]
#define FAN         4

// Get Humidity, Celsius
#define GetHum      A2
#define GetTmp      A7

#define SCK_PIN     7
#define IO_PIN      3
#define RST_PIN     2

// Get Lux
const int BH1750_address = 0x23; 
byte luxBuf[2];

//RTC Set
DS1302 rtc(RST_PIN, IO_PIN, SCK_PIN);

boolean ledStatus = false;
boolean waterFeed = false;
boolean fanWork = false;

boolean isHeCome = false;

char* LED_statusLabel;
char* LED_buttonLabel;

char* WATER_statusLabel;
char* WATER_buttonLabel;

char* FAN_statusLabel;
char* FAN_buttonLabel;

char* on = "ON";
char* off = "OFF";

//unsigned long prev_time;

char timeData[32]="";
char luxData[4]="";
char tmpData[4]="";
char humData[4]="";

Servo water_servo; // #define PIN_SERVO 9

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
 
void getTimeData(void) {
  memset(timeData, 0, sizeof(timeData));
  strcat(timeData, rtc.getDOWStr());
  strcat(timeData, rtc.getDateStr());
  strcat(timeData, rtc.getTimeStr());
}
 
void setup () {

// GPIO -set
  pinMode(LED_B, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(GetHum, INPUT);
  pinMode(GetTmp, INPUT);
  
// to get Lux
  Wire.begin();
  BH1750_Init(BH1750_address);

// init Servo
  water_servo.attach(PIN_SERVO);

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
      digitalWrite(LED_B, HIGH);
      LED_statusLabel = on;
      LED_buttonLabel = off;
    } else {
      digitalWrite(LED_B, LOW);
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
          "내부 조명: "
            "<a href=\"/?light=$S\"><input type=\"button\" value=\"$S\"></a><br>"
          "팬 작동:  "
            "<a href=\"/?fan=$S\"><input type=\"button\" value=\"$S\"></a><br>"  
          "물??: "
            "<a href=\"/?water=$S\"><input type=\"button\" value=\"Feed\"></a><br>"
            "LAST TIME: $S<br><br>"
          "주변환경:<br>"
            "조도: $S<br>온도: $S<br>화분습도: $S<br>"
            "</body></html>"            
      ), LED_buttonLabel, LED_buttonLabel,
         FAN_buttonLabel, FAN_buttonLabel,
         WATER_buttonLabel, timeData,
         luxData, tmpData, humData);

    ether.httpServerReply(bfill.position());
  }
}
