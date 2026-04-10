#include <Arduino.h>
#include <Wire.h>

// =======================
// I2C 引脚宏定义（按你当前板子）
// =======================
#define IIC_POWER_PIN  45
#define PIN_I2C_SDA   6
#define PIN_I2C_SCL   7

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nI2C Scanner Start...");
  pinMode(IIC_POWER_PIN, OUTPUT);
  digitalWrite(IIC_POWER_PIN, HIGH);

  // 初始化 I2C
  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.begin();

  Serial.println("Scanning I2C bus...");
  
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("✅ Found I2C device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("❌ No I2C devices found.");
  } else {
    Serial.print("🎯 Total devices found: ");
    Serial.println(found);
  }
}

void loop() {
  // 空循环，不重复扫描
}
