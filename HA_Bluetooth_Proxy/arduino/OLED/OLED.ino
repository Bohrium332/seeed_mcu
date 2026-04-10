#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#define IIC_POWER_PIN  45
#define SDA_PIN 1
#define SCL_PIN 2



// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/ SCL_PIN, /* data=*/ SDA_PIN, /* reset=*/ U8X8_PIN_NONE);  // 高速 I2C

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL_PIN, /* data=*/ SDA_PIN, /* reset=*/ U8X8_PIN_NONE);    // 低速 I2C

void setup(void) {
  pinMode(IIC_POWER_PIN, OUTPUT);
  digitalWrite(IIC_POWER_PIN, HIGH);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
}

void loop(void) {
  u8g2.clearBuffer();					// 清除内部内存
  u8g2.setFont(u8g2_font_ncenB08_tr);	// 选择合适的字体
  u8g2.drawStr(0,10,"Hello World!");	// 将内容写入内部内存
  u8g2.sendBuffer();					// 将内部内存传输到显示屏
  delay(1000);  
}
