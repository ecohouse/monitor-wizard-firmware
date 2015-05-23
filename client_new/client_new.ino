/* the chip will wake up once every X miniutes,  
*then read temperature and humidity readings from the sensor, 
*store the readings into a data array
*Go back to sleep again, turn the radio into sleep mode each time. 
*/

/*Ideally encode temperature and humidity values as one byte
anticipate temp to be in the range -40 to 60 degrees
one byte can represent values from 0 to 511, therefore resolution of ~0.195 (enough for application)
Therefore: round(T - (-40) / 0.195) will give a good approxiamation

Read temp then humidity

For humidity, limited to the range of 0 to 100
round(F/0.195)

2 bytes of data from humidity and temperature readings
take at twenty minute intervals => 6 bytes per hour, 144 per day
Send 3 packets per day of 48  bytes, each will carry a different ID to denote which packet it is

/*Knowing what time that packet is sent:
in the day it is
The receiver is connected to the internet and can add a timestamp to each received packet, 

ID   Packet   Time
2    0             15:30
2    1             18:30

Assume that the data is read in between at regular injtervals, such that 

*/

#include <SPI.h>
#include <RH_RF69.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include "DHT.h"

volatile byte sensorCount = 0;
volatile byte dataCount = 0; // increments with every reading of the sensor

#define DHTPIN 3
#define DHTTYPE DHT22
float lowerTempLimit  = -40.0;
float increment = 0.195;

#define SENSOR_INTERVAL 1 //wait period between sensor readings is this * 8 secs
#define SEND_INTERVAL 1 //number of data points before a packet is sent

#define LED 9
#define SERIAL 1
#define RADIO_INIT_FAIL 5
#define RADIO_FREQUENCY_ERROR 10

RH_RF69 rf69;

DHT dht(DHTPIN, DHTTYPE);

uint8_t data[48];
uint8_t temp;
uint8_t humidity;

void flash(int i) {
  for (int i = 0; i<i; i++) {
    digitalWrite(LED, HIGH);
    delay(50);
    digitalWrite(LED, LOW);
    delay(50);
  }
}

void readSensor() {     
  float t = dht.readTemperature();
  temp = round((t+lowerTempLimit) / increment);
  memcpy(&data[dataCount*2], &temp, 1);
  
  float h = dht.readHumidity();
  humidity = round(h / increment);
  memcpy(&data[dataCount*2+1], &humidity, 1);
  
}

void sendData() {
  rf69.send(data, sizeof(data));  
  //to extend: would simply do this with the other data arrays, possibly having the checking code in another function/possibly not have it at all  
  rf69.waitPacketSent();  
}

void watchdogEnable () 
  {
  // clear various "reset" flags
  MCUSR = 0;     
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // set interrupt mode and an interval 
  WDTCSR = bit (WDIE) | bit (WDP3) | bit (WDP0);    // set WDIE, and requested delay
  wdt_reset();  // pat the dog

  // disable ADC
  byte old_ADCSRA = ADCSRA;
  ADCSRA = 0;  
  
  // turn off various modules
  byte old_PRR = PRR;
  PRR = 0xFF; 

  // timed sequence coming up
  noInterrupts ();
  
  // ready to sleep
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  sleep_enable();

  // turn off brown-out enable in software
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS); 
  interrupts ();
  sleep_cpu ();  

  // cancel sleep as a precaution
  sleep_disable();
  PRR = old_PRR;
  ADCSRA = old_ADCSRA;
  
  } // end of myWatchdogEnable
    
ISR(WDT_vect) {
    wdt_disable();
}

void setup() {  
  
  #if SERIAL
  Serial.begin(9600);
  
  if (!rf69.init()) {
    Serial.println("init failed");
  }
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM, No encryption
  if (!rf69.setFrequency(433.0)) { 
    Serial.println("setFrequency failed");
  }
  
  #else
    if (!rf69.init()) {
      flash(RADIO_INIT_FAIL); 
  }
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM, No encryption
  if (!rf69.setFrequency(433.0)) { 
    flash(RADIO_FREQUENCY_ERROR);
  }
  #endif
}

void loop() {
 if(sensorCount == SENSOR_INTERVAL) { 
      readSensor();
      sensorCount = 0;
      dataCount++;     
 } else {
      sensorCount++;
 }

 if(dataCount == SEND_INTERVAL) {
        sendData(); 
        //delay(500) /*may need delay as watchdog has been set*/
        dataCount = 0; 
  } 
  watchdogEnable();
}
  
