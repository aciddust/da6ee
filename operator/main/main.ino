#include <Servo.h>
#include <SPI.h>
#include <EtherCard.h>
#include <Wire.h>

//파란색 LED 패널
#define LED_B       6

// 서보모터 [ PWM 제어 필요 ]
#define PIN_SERVO   9

// FAN (Relay 모듈에 연결되어있음 +12v)
#define FAN         4

// Hum - 습도 / Tmp - 온도 : 센싱부분
#define GetHum      A2
#define GetTmp      A7

// Lux - 조도 측정
const int BH1750_address = 0x23; //장치 고유주소
byte luxBuf[2]; //측정한 데이터 임시 저장 버퍼

// 액츄에이터들 상태
boolean ledStatus = false;
boolean waterFeed = false;
boolean fanWork = false;

// 자동인지 아닌지 판단
boolean autoWork = false;

// 웹페이지에서 사용되는 액츄에이터 상태 변수
char* LED_statusLabel;
char* LED_buttonLabel;
char* WATER_statusLabel;
char* WATER_buttonLabel;
char* FAN_statusLabel;
char* FAN_buttonLabel;
char* AUTO_buttonLabel;
char* AUTO_statusLabel;

// 웹페이지에서 사용되는 센서 상태 변수
char* tmp_statusLabel;
char* lux_statusLabel;
char* hum_statusLabel;

// 상태 문자들. 위의 상태변수에 대입될 예정
char* on = "ON";
char* off = "OFF";
char* RED = "#FF7676";
char* YEL = "#FFFF67";
char* GRN = "#9AFF9E";

//웹페이지 상태 값이 들어갈 버퍼들
char luxData[8]="";
char tmpData[8]="";
char humData[8]="";

//서보 클래스 생성
Servo water_servo; // #define PIN_SERVO 9

//웹페이지와 주고받는 데이터 버퍼사이에서
//어느 인덱스(위치)의 글자부터 가리킬지 정하는 변수
int pos = 0; 

// 이더넷 인터페이스의 MAC주소
// LAN 위에서 고유 값을 가지고 있어야함. 
// 임의 설정하였음.
byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x63 };

// DNS서버 지정 
// 추후 온실을 트위터(SNS)과 연동할 예정이기 때문에
// (twitter 에서 제공하는 API와 연계예정)
// DNS서버를 다음과 같이 지정하였다.
const char website[] PROGMEM = "arduino-tweet.appspot.com";

// 세션 값 
static byte session;

// 웹페이지를 표현할 수 있는 최대 용량.
byte Ethernet::buffer[760];

//화분에 물을 주는 과정에서 서보를 올리고 내리기 위해 시간을 지정
// 서보내리기(물준시간 기록) -> [현재 시각 - 물을 줬던 시간 > 2000]  -> 서보 올리기(시간초기화)
unsigned long prev_time=0;
unsigned long current_time=0;

// 각 센서의 값, (조도, 온도, 습도)
float lux_read = 0;
float tmp_vcc = 0;
float celsiustmp = 0;
float hum_read = 0;

//조도센서모듈 초기화
void BH1750_Init(int address){  
  Wire.beginTransmission(address);
  Wire.write(0x10); // 1 [lux] aufloesung
  Wire.endTransmission();
}

//조도센서모듈 값 읽기.
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

//온실 내부 환경 점검 : 센서 값에 따라 웹 페이지에서 색상 변경하기
void chk_ENV_Status(void){
  if(lux_read >= 430)
    lux_statusLabel = GRN;
  else if(lux_read > 300)
    lux_statusLabel = YEL;
  else
    lux_statusLabel = RED;
    
  if(celsiustmp >= 30)
    tmp_statusLabel = RED;
  else if(celsiustmp >= 25)
    tmp_statusLabel = YEL;
  else
    tmp_statusLabel = GRN;  
    
  if(hum_read > 700)
    hum_statusLabel = GRN;
  else if(hum_read > 500)
    hum_statusLabel = YEL;
  else 
    hum_statusLabel = RED;
}

// 보드 초기화 ( 센서 및 액츄에이터들 그외 다수 )
void setup () {

// 입출력 핀 설정 
  pinMode(LED_B, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(GetHum, INPUT);
  pinMode(GetTmp, INPUT);
  
// 조도센서 모듈 초기화
  Wire.begin();
  BH1750_Init(BH1750_address);

// 서보모터 Attach
  water_servo.attach(PIN_SERVO);
  
//디버깅을 위한 시리얼 통신 시작 (# 이더넷 모듈은 57600 baud 에서 정상작동)
  Serial.begin(57600);
  
//서버로부터 주소 받아오기 시작
  Serial.println("\n[Client Begin]");

  //이더넷 모듈 초기화하기 : 정해둔 버퍼크기만큼 통신할 준비 및 MAC주소 알려줄 준비
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println(F("Failed to access Ethernet controller"));
  // 주소 동절할당 시작
  if (!ether.dhcpSetup())
    Serial.println(F("DHCP failed"));

  // 이더넷 모듈 초기화 및 구성된 네트워크에 참가한 이후에
  // 이더넷 모듈의 상태를 시리얼로 알려주는 부분.
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  

  // DNS 연결
  if (!ether.dnsLookup(website))
    Serial.println(F("DNS failed"));
  // 서버 주소 확인
   ether.printIp("SRV: ", ether.hisip); 
}
// 보드 초기화 끝

// 메인 프로세스 시작
void loop () {
    
  // 조도 측정
  if(BH1750_Read(BH1750_address)==2){
    lux_read=((luxBuf[0]<<8)|luxBuf[1])/1.2;
  }
  
  // 온도 측정 (섭씨기준)
  tmp_vcc = (int)analogRead(GetTmp) * 5.0 / 1024.0;
  celsiustmp = (tmp_vcc - 0.5) * 100 ; 
  
  // 토양 내부 습도 측정
  hum_read = analogRead(GetHum); // 센서 감도는 알아서 조절 : 가변저항.
      
  // 버퍼 초기화 : 모두 빈공간 만들기
  memset(luxData, 0 , sizeof(luxData));
  memset(tmpData, 0 , sizeof(tmpData));
  memset(humData, 0 , sizeof(humData));
  // 위에서 비워버린 버퍼에 float -> String 형식으로 데이터 집어넣기.
  dtostrf(celsiustmp, 7, 2, tmpData);
  dtostrf(hum_read, 7, 2, humData);
  dtostrf(lux_read, 7, 2, luxData);
    
  //웹 페이지 받을 준비
  
  //클라이언트 요청 패킷
  word len = ether.packetReceive();
  //웹브라우저에서 클라이언트가 요청한 패킷 (URI 판단)
  word pos = ether.packetLoop(len);
  
  if(pos) {   
    //char 타입의 문자열 형식인 [이더넷 버퍼 + pos] 값에서
    //특정 조건을 만족하는 문자열을 발견하면 온실 내부의 환경을 제어하도록 한다.

    //LED 켜기
    if(strstr((char *)Ethernet::buffer + pos, "GET /?light=ON") != 0) {
      //Serial.println("[LIGHT]: Received ON command");
      ledStatus = true;
    }
    
    //LED 끄기
    else if(strstr((char *)Ethernet::buffer + pos, "GET /?light=OFF") != 0) {
      //Serial.println("[LIGHT]: Received OFF command");
      ledStatus = false;
    }
    
    //물 주기 ( 서보 내리기 -> 2초뒤에 서보 다시 올림 )
    if( (strstr((char *)Ethernet::buffer + pos, "GET /?water=ON") != 0)
        || strstr((char *)Ethernet::buffer + pos, "GET /?water=OFF") != 0 ) {
      //Serial.println("[WATER]: Received ON command");
      current_time = millis();
      waterFeed = true;
      water_servo.write(1); 
    }

    //릴레이모듈 ON -> FAN 켜기
    if(strstr((char *)Ethernet::buffer + pos, "GET /?fan=ON") != 0) {
      //Serial.println("[FAN]: Received ON command");
      fanWork = true;
    }

    //릴레이모듈 OFF -> FAN 끄기
    if(strstr((char *)Ethernet::buffer + pos, "GET /?fan=OFF") != 0) {
      //Serial.println("[FAN]: Received OFF command");
      fanWork = false;
    }

    //자동기능 활성화
    if(strstr((char *)Ethernet::buffer + pos, "GET /?auto=ON") != 0) {
      //Serial.println("[FAN]: Received OFF command");
      autoWork = true;
    }

    //자동기능 비활성화
    if(strstr((char *)Ethernet::buffer + pos, "GET /?auto=OFF") != 0) {
      //Serial.println("[FAN]: Received OFF command");
      autoWork = false;
    }

    //상태값 설정
    ///////////////////////////////////////////////////////////////////////
    //설정된 상태값을 기준으로 버튼 레이블 변경하기 및 기기제어
    
    // LED 켜기/끄기 및 버튼 레이블 변경
    if(ledStatus) {
      digitalWrite(LED_B, HIGH);
      LED_statusLabel = on;
      LED_buttonLabel = off;
    } else {
      digitalWrite(LED_B, LOW);
      LED_statusLabel = off;
      LED_buttonLabel = on;
    }

    // 물주기 버튼 레이블 변경
    if(waterFeed) {
      WATER_statusLabel = on;
      WATER_buttonLabel = off;
    } else {
      WATER_statusLabel = off;
      WATER_buttonLabel = on;
    }

    // FAN 켜기/끄기 및 버튼레이블 변경
    if(fanWork) {
      digitalWrite(FAN, HIGH);
      FAN_statusLabel = on;
      FAN_buttonLabel = off;
    } else {
      digitalWrite(FAN, LOW);
      FAN_statusLabel = off;
      FAN_buttonLabel = on;
    }

    // 자동 기능 활성화/ 비활성화 및 버튼 레이블 변경
    if(autoWork) {
     AUTO_statusLabel = on;
     AUTO_buttonLabel = off;

      //현재 외부 조도가 200lx 보다 낮거나 높으면 
      //상태값과 버튼 레이블을 변경하고
      //온실 내부의 LED를 켜고 끈다.
      if(lux_read <200) {
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

      //현재 온실 내부 온도가 27도 보다 낮거나 높으면 
      //상태값과 버튼 레이블을 변경하고
      //온실 내부의 FAN을 켜고 끈다.
      if(celsiustmp > 27) {
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
      
      //현재 토양 습도값이 450 보다 낮거나 높으면 
      //상태값과 버튼 레이블을 변경하고
      //서보모터를 제어하여 물을 준다.
      if(hum_read < 450) {
        waterFeed = true;
        WATER_statusLabel = on;
        WATER_buttonLabel = off;
        water_servo.write(1);
      }
      else {
        waterFeed = false;
        WATER_statusLabel = off;
        WATER_buttonLabel = on;
        water_servo.write(100);
      }
    } else { // 자동 : FALSE 일때 버튼 레이블
     AUTO_statusLabel = off;
     AUTO_buttonLabel = on; 
    }

    // 버퍼에 웹페이지를 데이터를 체워넣고
    // TCP 방식으로 브라우저에 뿌릴 것임.
    BufferFiller bfill = ether.tcpOffset();

    //아래는 브라우저에서 접속한 클라이언트들이 보게될 페이지다.
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
            "WATER: <a href=\"/?water=$S\"><input type=\"button\" value=\"Feed\"></a>"
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
         WATER_buttonLabel,
         
         lux_statusLabel, luxData,
         tmp_statusLabel, tmpData,
         hum_statusLabel, humData,
         AUTO_buttonLabel, AUTO_buttonLabel );

         //버튼 레이블과 상태값, 그리고 센서값들을 웹에 표현하는 방식이다. 

    ether.httpServerReply(bfill.position());  
    // 버퍼 채웠으니 보낸다.
  } 
  
  // 물 주는 서보가 내려간 이후 올라가는 부분을 구현한 것이다.
  // 서보가 내려가면 2초 뒤에 다시 올라가도록 하였다.
  // Delay를 써버리면 모든 작동들이 멈춰버리고 부자연스럽기에
  // 비동기방식으로 millis 를 사용하여
  // 웹페이지가 제어되거나 다른 작업이 수행되면서도 
  // 서보가 정상 작동하도록 구현하였다.

  // 클라이언트가 물주는 것을 요청하였고, 
  // 227번 라인에서 처리될 때 눌렀을 때 부터 시간을 측정하며 서보를 내리고
  // 이후 시간차가 2초이상 나면, 다시 서보를 올리는 방식이다.
  if(current_time - prev_time > 2000) {
        water_servo.write(100); 
        prev_time = current_time;
  }

  // 계속 환경을 측정한다.
  chk_ENV_Status( );
  
  // 안정성 확보.
  delay(1);
}
