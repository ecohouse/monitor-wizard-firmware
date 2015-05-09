/* goal: the arduino will WAKE UP once every X miniutes,  and then 
READ temperature and humidity readings from the sensor,
STORE the readings as a char array into memory
Go back to SLEEP again

implementation:
have a read function which takes readings
every eight seconds, the interrupt vector will be called, increment a counter by one each time - once 
counter has gotten up to a certain number e.g. 40 for five minutes, then trigger the read sensors function
This way the device will still wake up every eight seconds, but it will go back to sleep straight after
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
#define SENSOR_INTERVAL 113 //wait period between sensor readings is this * 8 secs
#define SEND_INTERVAL 1 
#define LED 9

#define SERIAL FALSE
#define RADIO_INIT_FAIL 5
#define RADIO_FREQUENCY_ERROR 10
#define GOT_REPLY 3 
#define RECV_FAIL 6
#define NO_REPLY 9

RH_RF69 rf69;

DHT dht(DHTPIN, DHTTYPE);

uint8_t data[13]; //maximum user message of 28 octets
char bufferh[6];
char buffert[6];

void flash(int i) {
  for (int i = 0; i<i; i++) {
    digitalWrite(LED, HIGH);
    delay(50);
    digitalWrite(LED, LOW);
    delay(50);
  }
}

void printData() {
  //print all the data for debugging purposes
    for(int i = 0; i<5; i++) {
      Serial.print(F("Humidity: "));
      for(int j = 0; j<6; j++) {  
        Serial.print((char) data[12*i +j]);
      }
      Serial.print(F(" Temperature: "));
      for(int j = 6; j<12; j++) {  
        Serial.print((char)data[12*i + j]);
      }
      Serial.print(F("\n"));
    }
}

void readSensor() {     
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  dtostrf(h, 5, 2, bufferh); //converts the float into a char array
  dtostrf(t, 5, 2, buffert);
  memcpy(data+(dataCount*12), bufferh, 6); //each reading adds 12B, hence need to copy data 12 bytes along
                                            
  memcpy(data+(dataCount*12)+6, buffert, 6); // copy temperature into data array.
}

void sendData() {
  rf69.send(data, sizeof(data));  
  rf69.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  #if SERIAL
  if (rf69.waitAvailableTimeout(500))
  { 
    // Should be a reply message for us now   
    if (rf69.recv(buf, &len))
    {
      Serial.print("got reply: ");
      Serial.println((char*)buf);
    }
    else
    {
      Serial.println("recv failed");
    }
  }
  else
  {
    Serial.println("No reply, is rf69_server running?");
  }
  
  #else
     if (rf69.waitAvailableTimeout(500))
  { 
    // Should be a reply message for us now   
    if (rf69.recv(buf, &len))
    {
      flash(GOT_REPLY);
    }
    else
    {
      flash(RECV_FAIL);
    }
  }
  else
  {
    flash(NO_REPLY); 
  }
  
  #endif

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
    flash(RADIO_INIT_FAIL); 
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
        dataCount = 0; 
  } 
  watchdogEnable();
}
  
