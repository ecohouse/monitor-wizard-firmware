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
#include <RH_NRF905.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include "DHT.h"

const byte LED = 13; //use an led to debug when on the barebones microcontroller
volatile byte sensorCount = 0;// increments after every 8 secs, 
//once count gets up to a certain value then the sensors are read
volatile byte dataCount = 0; // increments with every reading of the sensor

#define DHTPIN 2
#define DHTTYPE DHT22
#define SENSOR_COUNT_THRESHOLD 1 //wait period between sensor readings is this * 8 secs
#define DATA_COUNT_THRESHOLD 1 //send when there are two sets of readings:   therefore a transmission every half hour

// Singleton instance of the radio driver
RH_NRF905 nrf905; 

DHT dht(DHTPIN, DHTTYPE);

uint8_t data[13]; //maximum user message of 28 octets
char bufferh[6];
char buffert[6];

void flash() {
  for (int i = 0; i<10; i++) {
    digitalWrite(LED, HIGH);
    delay(50);
    digitalWrite(LED, LOW);
    delay(50);
  }
}

void printData() {
  //print all the data
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
 
  nrf905.setRF(RH_NRF905::TransmitPower10dBm); //set transmit power to 6dBm
  // Send a message to nrf905_server
  nrf905.send(data, sizeof(data));
  
  Serial.println("Sending to nrf905_server");
  
 nrf905.waitPacketSent();
  
  // Now wait for a reply
  uint8_t buf[RH_NRF905_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (nrf905.waitAvailableTimeout(500))
  { 
    // Should be a reply message for us now   
    if (nrf905.recv(buf, &len))
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
    Serial.println("No reply, is nrf905_server running?");
  }
 // delay(400);
}
    
ISR(WDT_vect) {
    if(sensorCount == SENSOR_COUNT_THRESHOLD) { // keep it to 8 secs for now
      readSensor();
      sensorCount = 0;
      dataCount++;
    } else {
      sensorCount++;
    }
    wdt_disable();
}

void setup() {  
  
  Serial.begin(9600);
  
  while (!Serial) 
    ; // wait for serial port to connect. Needed for Leonardo only
  if (!nrf905.init()) {
    Serial.println(F("init failed"));
  } else {
    Serial.println(F("init successful"));
  }
  // Defaults after init are 433.2 MHz (channel 108), -10dBm
  
  dht.begin();
  Serial.print(F("Initialised \n"));
}

void loop() {
  
  delay(1000);
  
  byte ADCSRA_old = ADCSRA;
	
//disable ADC
  ADCSRA = 0;
  
  MCUSR &= ~(1 << WDRF); //resets the status flag
  WDTCSR |= (1 << WDCE) | (1 << WDE); //enables configuration of watchdog resgister
  WDTCSR = (1 << WDP0) | (1 << WDP3); // i = 9 0b1001 => sets interval to eight secs
  WDTCSR |= (1 << WDIE); //enables the interrupt mode
  wdt_reset(); // this is a loop => therefore the register should reset each time
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();
  sleep_enable(); //prepares the sleep mode
  
  //turns off brown-out enable
  MCUCR = (1 << BODS) | (1 << BODSE);
  MCUCR = (1 << BODS);
  interrupts(); //enable interrupts
  sleep_mode(); // triggers the sleep mode
  
  sleep_disable(); //precaution in case the Mcu doesn't wake up

  ADCSRA = ADCSRA_old;	//reenable ADC
	//add some sort of delay here so that sleep mode is actually disabled?

  delay(500);
  
 if(dataCount == DATA_COUNT_THRESHOLD) {
        sendData(); //latency is too high to place into ISR vector
        dataCount = 0; 
  } 

}
  
