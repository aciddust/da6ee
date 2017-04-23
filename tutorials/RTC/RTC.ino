/* Define the DIO pins used for the RTC module */
#define SCK_PIN 4
#define IO_PIN 3
#define RST_PIN 2
 
/* Include the DS1302 library */
#include <DS1302.h>
 
/* Initialise the DS1302 library */
DS1302 rtc(RST_PIN, IO_PIN, SCK_PIN);
 
void setup()
{
  /* Clear the 1302's halt flag */
  rtc.halt(false);
  /* And disable write protection */
  rtc.writeProtect(false);
  /* Initialise the serial port */
  Serial.begin(9600);
  
  /* Set the time and date */
  rtc.setDOW(THURSDAY);
  rtc.setTime(21,48,0);
  rtc.setDate(27,8,2015); 
}
 
/* Main program */
void loop()
{
  /* Read the time and date once every second */
  while(1)
  {
    Serial.print("It is ");
    Serial.print(rtc.getDOWStr());
    Serial.print(" ");
    Serial.print(rtc.getDateStr());
    Serial.print(" ");
    Serial.print("and the time is: ");
    Serial.println(rtc.getTimeStr());
 
    /* Wait before reading again */
    delay (1000);
  }
}
