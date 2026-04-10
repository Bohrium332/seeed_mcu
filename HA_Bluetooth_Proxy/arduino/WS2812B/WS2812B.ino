#include <Adafruit_NeoPixel.h>

/* 只能使用Adafruit_NeoPixel这个库 */

#define LED_PIN     38     // GPIO38
#define NUM_LEDS    1      // 单灯
#define BRIGHTNESS  20     // 亮度 20~255

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

uint16_t hue = 0;

uint32_t ColorHSV(uint16_t hue) {
  return strip.ColorHSV(hue, 255, BRIGHTNESS);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("WS2812B (GPIO38) 测试启动...");

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();   // 清空

  // ======================
  // RGB 基础测试
  // ======================
  Serial.println("红色测试");
  strip.setPixelColor(0, strip.Color(BRIGHTNESS, 0, 0));
  strip.show();
  delay(1000);

  Serial.println("绿色测试");
  strip.setPixelColor(0, strip.Color(0, BRIGHTNESS, 0));
  strip.show();
  delay(1000);

  Serial.println("蓝色测试");
  strip.setPixelColor(0, strip.Color(0, 0, BRIGHTNESS));
  strip.show();
  delay(1000);

  Serial.println("开始彩虹效果...");
}

void loop() {
  uint32_t color = strip.ColorHSV(hue, 255, BRIGHTNESS);
  strip.setPixelColor(0, color);
  strip.show();

  hue += 256;   // 控制变色速度（越小变化越慢）
  if (hue >= 65535) hue = 0;

  delay(20);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.print("彩虹运行中 Hue = ");
    Serial.println(hue);
  }
}
