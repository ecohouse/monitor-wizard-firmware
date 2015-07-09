#include <SPI.h>
#include <RH_RF69.h>

// Singleton instance of the radio driver
RH_RF69 rf69;
float increment = 0.392;
float lowerTempLimit = -40.0;
uint8_t interrupt_pin = 7;

float decodeTemp(uint8_t temp) {
  return (float)(temp * increment + lowerTempLimit);
}

float decodeHumidity(uint8_t humidity) {
  return (float)(humidity * increment);
}

void debugResults(uint8_t buf[]) {
      Serial.print("got message from Node ");
      Serial.print(rf69.headerFrom());
      Serial.print("\n");
      
      for (int i = 0; i<sizeof(buf); i++) {
        if(i%2 == 0) { //humidity at odd positions
          Serial.print("Temperature: ");
          Serial.print(decodeTemp(buf[i]));
          Serial.print(" ");
        } else {
          Serial.print("Humidity: ");
          Serial.print(decodeHumidity(buf[i]));
          Serial.print("\n");
        }
      }
      
      Serial.print("\n");
}

void setup() 
{
  Serial.begin(9600);
  if (!rf69.init())
    Serial.println("init failed");
    
  if (!rf69.setFrequency(915.0))
    Serial.println("setFrequency failed");
}

void loop()
{
  if (rf69.available())
  {
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf69.recv(buf, &len))
    {
    
      //set interrupt pin on Rpi high
      noInterrupts();
      digitalWrite(interrupt_pin, 1);
      delay(1000);
      
      Serial.print("MSG");
      delay(1000);
      
      Serial.print((char)rf69.headerFrom());
      Serial.print((char*)buf);
      digitalWrite(interrupt_pin, 0);
      interrupts();
    
    }
    else
    {
      Serial.println("recv failed");
    }
  }
}

