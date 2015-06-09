#include <SPI.h>
#include <RH_RF69.h>

// Singleton instance of the radio driver
RH_RF69 rf69;
float increment = 0.392;
float lowerTempLimit = -40.0;

float decodeTemp(uint8_t temp) {
  return (float)(temp * increment + lowerTempLimit);
}

float decodeHumidity(uint8_t humidity) {
  return (float)(humidity * increment);
}

void setup() 
{
  Serial.begin(9600);
  if (!rf69.init())
    Serial.println("init failed");
    
  if (!rf69.setFrequency(433.0))
    Serial.println("setFrequency failed");
}

void loop()
{
  if (rf69.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf69.recv(buf, &len))
    {
      Serial.print("got message from Node ");
      Serial.print(rf69.headerFrom());
      Serial.print("\n");
      
      for (int i = 0; i<len; i++) {
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
    else
    {
      Serial.println("recv failed");
    }
  }
}

