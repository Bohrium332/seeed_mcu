#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include "Audio.h"

// ================== 硬件定义 ==================
#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   16

#define PIN_I2S_BCLK  11
#define PIN_I2S_LRCK  12
#define PIN_I2S_DOUT  13
#define PIN_I2S_MCLK  10

#define ES8156_ADDR   0x08

// ================== WiFi & URL ==================
static const char* WIFI_SSID = "dudu";
static const char* WIFI_PASS = "poi55885";
static const char* URL_ORIG  = "http://music.163.com/song/media/outer/url?id=1980818176.mp3";

// ================== ESP32-audioI2S ==================
Audio audio;

// ================== I2C 读写 ==================
static bool es8156_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8156_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission(true) == 0);
}

static bool es8156_read(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(ES8156_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ES8156_ADDR, 1) != 1) return false;
  val = Wire.read();
  return true;
}

static void es8156_dump_chipid() {
  uint8_t fd=0, fe=0, ff=0;
  bool okFD = es8156_read(0xFD, fd);
  bool okFE = es8156_read(0xFE, fe);
  bool okFF = es8156_read(0xFF, ff);
  Serial0.printf("ChipID: FD=%s 0x%02X, FE=%s 0x%02X, FF=%s 0x%02X\r\n",
                 okFD?"OK":"NG", fd, okFE?"OK":"NG", fe, okFF?"OK":"NG", ff);
}

// ================== ES8156：最小初始化（更适合“播放网络音频”，避免强绑 Ratio） ==================
// 这份初始化思路接近你 Step2 已验证可出声的路径，且不会强制写 Ratio/分频，
// 让 ESP32-audioI2S 根据 MP3 采样率动态配置 I2S 更稳。
static bool es8156_init_minimal() {
  Serial0.println("=== ES8156 init (minimal for streaming) ===");
  es8156_dump_chipid();

  struct RegVal { uint8_t r; uint8_t v; uint16_t dly; };
  const RegVal cfg[] = {
    // reset / power
    {0x00, 0x1C, 10},
    {0x00, 0x03, 10},

    // clock / fmt
    {0x02, 0x04, 0},   // 典型：Slave + normal clk
    {0x08, 0x3F, 0},   // analog / dac power相关（你验证可用）
    {0x11, 0x00, 0},   // I2S normal + 16bit（与库输出匹配）

    // unmute + volume
    {0x13, 0x00, 0},   // DAC unmute
    {0x14, 0xE0, 0},   // 音量（你当前程序用的值）

    // output path（你验证可用）
    {0x20, 0x2A, 0},
    {0x21, 0x3C, 0},
    {0x22, 0x08, 0},
  };

  for (auto &it : cfg) {
    if (!es8156_write(it.r, it.v)) {
      Serial0.printf("[ERR] I2C write fail: R%02X <= 0x%02X\r\n", it.r, it.v);
      return false;
    }
    Serial0.printf("W R%02X = 0x%02X\r\n", it.r, it.v);
    if (it.dly) delay(it.dly);
  }

  Serial0.println("[OK] ES8156 minimal init done.");
  return true;
}

// ================== ES8156：在 I2S 时钟起来后“再重置/补写一次” ==================
// 原厂常见建议：如果先 IIC 配了芯片、后面 IIS 时钟才来，可能需要再 Reset 一次。
static void es8156_reinit_after_i2s_clock() {
  Serial0.println("=== ES8156 re-init after I2S clocks ===");
  // reset
  es8156_write(0x00, 0x1C);
  delay(2);
  es8156_write(0x00, 0x03);
  delay(2);

  // 只补关键寄存器（别写 Ratio，避免锁死采样率）
  es8156_write(0x02, 0x04);
  es8156_write(0x08, 0x3F);
  es8156_write(0x11, 0x00);
  es8156_write(0x13, 0x00);
  es8156_write(0x14, 0xE0);
  es8156_write(0x20, 0x2A);
  es8156_write(0x21, 0x3C);
  es8156_write(0x22, 0x08);

  Serial0.println("[OK] ES8156 re-init done.");
}

// ================== 处理 302：拿到 Location ==================
static String resolve_redirect_once(const char* url) {
  HTTPClient http;
  WiFiClient client;

  Serial0.printf("HTTP probe: %s\r\n", url);

  if (!http.begin(client, url)) {
    Serial0.println("[WARN] HTTP begin failed, use original URL");
    return String(url);
  }

  int code = http.GET();
  Serial0.printf("HTTP code = %d\r\n", code);

  String out = String(url);
  if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
    String loc = http.header("Location");
    if (loc.length()) {
      Serial0.print("Redirect Location: ");
      Serial0.println(loc);
      out = loc;
    } else {
      Serial0.println("[WARN] Redirect but Location header empty, use original URL");
    }
  }

  http.end();
  return out;
}

// ================== ESP32-audioI2S 回调 ==================
void audio_info(const char *info){
  Serial0.print("[audio_info] ");
  Serial0.println(info);
}
void audio_showstreamtitle(const char *info){
  Serial0.print("[title] ");
  Serial0.println(info);
}
void audio_bitrate(const char *info){
  Serial0.print("[bitrate] ");
  Serial0.println(info);
}
void audio_eof_mp3(const char *info){
  Serial0.print("[eof] ");
  Serial0.println(info);
}

// ================== Arduino 标准入口 ==================
void setup() {
  Serial0.begin(115200);
  delay(200);

  Serial0.println("=================================");
  Serial0.println(" ES8156 + WiFi + ESP32-audioI2S  ");
  Serial0.println("=================================");

  // I2C init
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  // 扫描 I2C
  Serial0.println("--- Scanning I2C Bus ---");
  int found = 0;
  for (int addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial0.printf("[I2C] Device found at 0x%02X%s\r\n",
                     addr, (addr == ES8156_ADDR) ? " <-- ES8156" : "");
      found++;
    }
  }
  Serial0.printf("--- Scan complete. Found %d device(s). ---\r\n", found);

  // 先做一次 ES8156 最小初始化（此时 I2S 还没起来也没关系）
  if (!es8156_init_minimal()) {
    Serial0.println("[FATAL] ES8156 init failed. Stop.");
    while (1) delay(1000);
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial0.printf("Connecting WiFi: %s\r\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial0.print(".");
    if (millis() - t0 > 20000) {
      Serial0.println("\r\n[FATAL] WiFi connect timeout.");
      while (1) delay(1000);
    }
  }
  Serial0.println("\r\n[OK] WiFi connected.");
  Serial0.print("IP: ");
  Serial0.println(WiFi.localIP());

  // 让库配置 I2S 引脚（此处会把 I2S 时钟真正跑起来）
  Serial0.println("Init Audio pinout (with MCLK)...");
  audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_MCLK);

  // 等 I2S 稳定一下，再按原厂建议对 ES8156 再 reset/补写一次关键寄存器
  delay(50);
  es8156_reinit_after_i2s_clock();

  // 音量：0~21（库音量）
  audio.setVolume(6);

  // 解析一次 302
  String finalUrl = resolve_redirect_once(URL_ORIG);
  Serial0.print("Play URL: ");
  Serial0.println(finalUrl);

  delay(100);
  audio.connecttohost(finalUrl.c_str());
}

void loop() {
  audio.loop();
}
