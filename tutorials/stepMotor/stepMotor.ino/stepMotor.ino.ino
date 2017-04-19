#include <Stepper.h>

//2,048 = 360도
//1,536 = 270도
//1,024 = 180도
//  512 =  90도
//  256 =  45도
//  171 =  30도(*)
//  128 =  23도(*)

const int stepsPerRevolution = 64; 
Stepper myStepper(stepsPerRevolution, 11,9,10,8); 
void setup() {
  myStepper.setSpeed(500);
}
void loop() {
  
  // 시계 반대 방향으로 한바퀴 회전
  for(int i=0; i<32; i++) {  // 64 * 32 = 2048 한바퀴
    myStepper.step(stepsPerRevolution);
  }
  delay(500);

  // 시계 방향으로 한바퀴 회전
  for(int i=0; i<32; i++) {
    myStepper.step(-stepsPerRevolution);
  }
  delay(500);
}
