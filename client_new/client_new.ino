/* the chip will wake up once every X miniutes,  
*then read temperature and humidity readings from the sensor, 
*store the readings into a data array
*Go back to sleep again, turn the radio into sleep mode each time. 
*/

/*Ideally encode temperature and humidity values as one byte
anticipate temp to be in the range -40 to 60 degrees
one byte can represent values from 0 to 255, therefore resolution of ~0.4 (enough for application)
Therefore: round(T - (-40) / 0.39) will give a good approxiamation

Read temp then humidity

For humidity, limited to the range of 0 to 100
round(F/0.39)

2 bytes of data from humidity and temperature readings
take at twenty minute intervals => 6 bytes per hour, 144 per day
Send 3 packets per day of 48  bytes, 

ID    Time
2     15:30
2     18:30
3     19:15
...
*/

#include <SPI.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <DHT.h>
#include <RH_RF69.h>

#define DHTPIN 3
#define DHTTYPE DHT22
uint8_t temp;
uint8_t humidity;
DHT dht(DHTPIN, DHTTYPE);

float lowerTempLimit  = -40.0;
float increment = 0.392;

volatile byte sensorCount = 0;
volatile byte dataCount = 0; // increments with every reading of the sensor
#define SENSOR_INTERVAL 150 //wait period between sensor readings is this * 8 secs
#define SEND_INTERVAL 24  //number of data points before a packet is sent

#define LED 9
//#define SERIAL 
#define RADIO_INIT_FAIL 5
#define RADIO_FREQUENCY_ERROR 10

#define NODEID 2
RH_RF69 rf69;
uint8_t data[49];

void flash(int j) {
  for (int i = 0; i<j; i++) {
    digitalWrite(LED, HIGH);
    delay(50);
    digitalWrite(LED, LOW);
    delay(50);
  }
}

void readSensor() { 

  float t = dht.readTemperature();
  temp = (uint8_t)round((t-lowerTempLimit) / increment);
  memcpy(data+dataCount*2, &temp, 1);
  
  float h = dht.readHumidity();
  humidity = (uint8_t)round(h / increment);
  memcpy(data+dataCount*2+1, &humidity, 1);
  
}

void sendData() {
  rf69.send(data, sizeof(data));  
  rf69.waitPacketSent();  
}

void watchdogEnable () 
  {
    
  rf69.setIdleMode(RH_RF69_OPMODE_MODE_SLEEP);
  //rf69.setModeIdle();
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
    sensorCount++;
    wdt_disable();
}

void setup() {  
  
  #if SERIAL
  Serial.begin(9600);
  
  if (!rf69.init()) {
    Serial.println("init failed");
  }
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM, No encryption
  if (!rf69.setFrequency(915.0)) { 
    Serial.println("setFrequency failed");
  }
  
  rf69.setHeaderFrom(NODEID);
   
  #else
    if (!rf69.init()) {
      flash(RADIO_INIT_FAIL); 
  }
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM, No encryption
  if (!rf69.setFrequency(433.0)) { 
    flash(RADIO_FREQUENCY_ERROR);
  }
  
  rf69.setHeaderFrom(NODEID);
  rf69.setTxPower(14);
  
  #endif
}

void loop() {
  
   if(sensorCount == SENSOR_INTERVAL) { 
      //Serial.print("Reading from sensor\n");
      readSensor();
      //Serial.print("Sensor read\n");  
      sensorCount = 0;
      dataCount++;     
 } 

 if(dataCount == SEND_INTERVAL) {
        #ifdef DEBUG
        Serial.print("Sending Data\n");
        #endif
        sendData();
        #ifdef DEBUG
        Serial.print("Sent Data\n");
        #endif
        dataCount = 0; 
  } 
  
  //delay(100); 
  watchdogEnable();
 

}  
