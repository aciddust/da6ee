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

// RTC
#define SCK_PIN     7
#define IO_PIN      3
#define RST_PIN     2

// Get Lux
const int BH1750_address = 0x23; 
byte luxBuf[2];

// Actuator Status
boolean ledStatus = false;
boolean waterFeed = false;
boolean fanWork = false;

// Auto
boolean autoWork = false;

// Actuator Status on WebPage
char* LED_statusLabel;
char* LED_buttonLabel;
char* WATER_statusLabel;
char* WATER_buttonLabel;
char* FAN_statusLabel;
char* FAN_buttonLabel;
char* AUTO_buttonLabel;
char* AUTO_statusLabel;

// Sensor Status on WebPage
char* tmp_statusLabel;
char* lux_statusLabel;
char* hum_statusLabel;

// Status Characters
char* on = "ON";
char* off = "OFF";
char* RED = "#FF7676";
char* YEL = "#FFFF67";
char* GRN = "#9AFF9E";

// Status Buffer
char timeData[16]="";
char luxData[8]="";
char tmpData[8]="";
char humData[8]="";

DS1302 rtc(RST_PIN, IO_PIN, SCK_PIN);
Servo water_servo; // #define PIN_SERVO 9

int pos = 0;

// ethernet interface mac address, must be unique on the LAN
byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x63 };

//[FOR LOOKUP DNS] [session] [to use buffer on web]
const char website[] PROGMEM = "arduino-tweet.appspot.com";
static byte session;
byte Ethernet::buffer[760];
//Stash stash;

//Chk watering Time
unsigned long prev_time=0;
unsigned long current_time=0;

unsigned long used_prev= 0;
unsigned long used_current=0;

//Sensor datas;
float lux_read = 0;
float tmp_vcc = 0;
float celsiustmp = 0;
float hum_read = 0;

//LUX INIT
void BH1750_Init(int address){  
  Wire.beginTransmission(address);
  Wire.write(0x10); // 1 [lux] aufloesung
  Wire.endTransmission();
}

//GET LUX
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

//GET TIME, NOW
void getTimeData(void) {
  memset(timeData, 0, sizeof(timeData));
  //strcat(timeData, rtc.getDateStr());
  strcat(timeData, rtc.getTimeStr());
}

//Check Environment 
void chk_ENV_Status(void){
  if(lux_read >= 400)
    lux_statusLabel = GRN;
  else if(lux_read > 250)
    lux_statusLabel = YEL;
  else
    lux_statusLabel = RED;
    
  if(celsiustmp >= 32)
    tmp_statusLabel = RED;
  else if(celsiustmp >= 25)
    tmp_statusLabel = YEL;
  else
    tmp_statusLabel = GRN;  
    
  if(hum_read > 650)
    hum_statusLabel = GRN;
  else if(hum_read > 350)
    hum_statusLabel = YEL;
  else 
    hum_statusLabel = RED;
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
  
//Serial Begin.. (# Ethernet Module works on 57600 speed)
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
  if(BH1750_Read(BH1750_address)==2){
    lux_read=((luxBuf[0]<<8)|luxBuf[1])/1.2;
  }
  
  // 온도 측정 (섭씨기준)
  tmp_vcc = (int)analogRead(GetTmp) * 5.0 / 1024.0;
  celsiustmp = (tmp_vcc - 0.5) * 100 ; 
  // float fahrenheittmp= celsiustmp * 9.0/5.0 + 32.0; // 화씨
  
  // 토양 내부 습도 측정
  hum_read = analogRead(GetHum); // 센서 감도는 알아서 조절

  // SENSOR DATAS - buffer set 0 + [float to char]
  memset(luxData, 0 , sizeof(luxData));
  memset(tmpData, 0 , sizeof(tmpData));
  memset(humData, 0 , sizeof(humData));
  dtostrf(celsiustmp, 7, 2, tmpData);
  dtostrf(hum_read, 7, 2, humData);
  dtostrf(lux_read, 7, 2, luxData);
    
  //웹 페이지 받을 준비
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);
  
  if(pos) {    

    used_prev = millis();
      
    if(strstr((char *)Ethernet::buffer + pos, "GET /?light=ON") != 0) {
      //Serial.println("[LIGHT]: Received ON command");
      ledStatus = true;
    }

    else if(strstr((char *)Ethernet::buffer + pos, "GET /?light=OFF") != 0) {
      //Serial.println("[LIGHT]: Received OFF command");
      ledStatus = false;
    }
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?water=ON") != 0) {
      //Serial.println("[WATER]: Received ON command");
      current_time = millis();
      waterFeed = true;
      water_servo.write(120); 
      
      getTimeData(); 
    }
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?water=OFF") != 0) {
      //Serial.println("[WATER]: Received OFF command");
      current_time = millis();
      waterFeed = false;
      water_servo.write(120);

      getTimeData();
    }
    
    if(strstr((char *)Ethernet::buffer + pos, "GET /?fan=ON") != 0) {
      //Serial.println("[FAN]: Received ON command");
      fanWork = true;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?fan=OFF") != 0) {
      //Serial.println("[FAN]: Received OFF command");
      fanWork = false;
    }

    if(strstr((char *)Ethernet::buffer + pos, "GET /?auto=ON") != 0) {
      //Serial.println("[FAN]: Received OFF command");
      autoWork = true;
    }

    //??
    if(strstr((char *)Ethernet::buffer + pos, "GET /?auto=OFF") != 0) {
      //Serial.println("[FAN]: Received OFF command");
      autoWork = false;
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

//auto
    if(autoWork) {
     AUTO_statusLabel = on;
     AUTO_buttonLabel = off;

      if(lux_read <400) {
        Serial.println("LED_ON");
        ledStatus = true;
        LED_statusLabel = on;
        LED_buttonLabel = off;
        digitalWrite(LED_B, HIGH);
      }
      else {
        Serial.println("LED_OFF");
        ledStatus = false;
        LED_statusLabel = off;
        LED_buttonLabel = on;
        digitalWrite(LED_B, LOW);  
      }

      if(celsiustmp > 25) {
        fanWork = true;
        FAN_statusLabel = on;
        FAN_buttonLabel = off;
        digitalWrite(FAN, HIGH);
      }
      else {
        fanWork = false;
        FAN_statusLabel = off;
        FAN_buttonLabel = on;
        digitalWrite(FAN, LOW);
      }
      if(hum_read < 350) {
        waterFeed = true;
        WATER_statusLabel = on;
        WATER_buttonLabel = off;
        water_servo.write(120);
      }
      else {
        waterFeed = false;
        WATER_statusLabel = off;
        WATER_buttonLabel = on;
        water_servo.write(1);
      }
    } else {
     AUTO_statusLabel = off;
     AUTO_buttonLabel = on; 
    }
//
    BufferFiller bfill = ether.tcpOffset();
    bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\n"
      "Content-Type:text/html\r\nPragma:no-cache\r\n\r\n"
      "<html>"
        "<head>"
          "<meta http-equiv=\"refresh\" content=\"2\" charset=\"utf-8\">"
          "<title>Da6eE</title>"
        "</head>"
        "<body>"   
            "LED: <a href=\"/?light=$S\"><input type=\"button\" value=\"$S\"></a><br>"
            "FAN: <a href=\"/?fan=$S\"><input type=\"button\" value=\"$S\"></a><br>"
            "WATER: <a href=\"/?water=$S\"><input type=\"button\" value=\"Feed\"></a>""last?: $S<br>"
          "<br>Environment:<br>"
          "<table>"
            "<tr><th>lux</th><th bgcolor=$S>$S</th></tr>"
            "<tr><th>tmp</th><th bgcolor=$S>$S</th></tr>"
            "<tr><th>hum</th><th bgcolor=$S>$S</th></tr>"
          "</table>"
          "<br>Auto: <a href=\"/?auto=$S\"><input type=\"button\" value=\"$S\"></a>"
        "</body>"
      "</html>"            
      ), LED_buttonLabel, LED_buttonLabel,
         FAN_buttonLabel, FAN_buttonLabel,
         WATER_buttonLabel, timeData,
         
         lux_statusLabel, luxData,
         tmp_statusLabel, tmpData,
         hum_statusLabel, humData,
         AUTO_buttonLabel, AUTO_buttonLabel );

    ether.httpServerReply(bfill.position());  
  } 
  
  //WATER_SERVO TURN BACK, AFTER '2' Sec(s)
  if(current_time - prev_time > 2000) {
        water_servo.write(1); 
        prev_time = current_time;
  }
  
  chk_ENV_Status( );
  delay(1);
}
