#include <Wire.h>

#define EEPROM_I2C_ADDRESS 0x51  // Default I2C address for 24C02 (A0, A1, A2 = GND)

void setup() {
  Serial.begin(9600);
  Wire.begin();

  Serial.println("Reading data from 24C02 EEPROM...");
  delay(500);

  for (unsigned int address = 0; address < 256; address++) {
    byte data = readEEPROM(EEPROM_I2C_ADDRESS, address);
    
    // Print data in hexadecimal format
    if (address % 16 == 0) {
      Serial.println();  // new line every 16 bytes
      Serial.print("0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(": ");
    }

    if (data < 16) Serial.print("0");
    Serial.print(data, HEX);
    Serial.print(" ");
  }

  Serial.println("\n\nDone reading EEPROM.");
}

void loop() {
  // nothing here
}

byte readEEPROM(int deviceAddress, unsigned int memAddress) {
  byte data = 0;
  
  Wire.beginTransmission(deviceAddress);
  Wire.write((byte)memAddress);
  Wire.endTransmission();

  Wire.requestFrom(deviceAddress, 1);
  if (Wire.available()) data = Wire.read();

  return data;
}
