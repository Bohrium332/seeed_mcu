#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include "Audio.h"

// ============================================================
// ESP32-S3 + PCM5122 network MP3 demo
//
// Based on the existing ES8156_Music example, but adapted for PCM5122.
// The MP3 URL follows the same flow: HTTP probe -> resolve redirect -> play.
//
// ============================================================

// ---------------- Pin Macros ----------------
#define PIN_CODEC_XSMT  3   
#define PIN_I2S_MCLK    4   
#define PIN_I2S_BCLK    5   
#define PIN_I2S_LRCK    6   
#define PIN_I2S_DOUT    7   
#define PIN_I2C_SDA     15  
#define PIN_I2C_SCL     16  

#define PCM5122_I2C_ADDR 0x4C

// ---------------- WiFi & URL ----------------
static const char *WIFI_SSID = "dudu";
static const char *WIFI_PASS = "poi55885";
static const char *URL_ORIG  = "http://music.163.com/song/media/outer/url?id=1980818176.mp3";
static const uint8_t AUDIO_VOLUME = 21;

// ---------------- ESP32-audioI2S ----------------
Audio audio;

static void printMemoryInfo() {
  Serial.printf("psramFound=%s, freeHeap=%u, freePsram=%u, psramSize=%u\n",
                psramFound() ? "true" : "false",
                ESP.getFreeHeap(),
                ESP.getFreePsram(),
                ESP.getPsramSize());
}

static void codecUnmuteByXSMT() {
  pinMode(PIN_CODEC_XSMT, OUTPUT);
  digitalWrite(PIN_CODEC_XSMT, HIGH);
}

static void scanI2C() {
  Serial.println("--- Scanning I2C Bus ---");
  int found = 0;
  for (int addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] Device found at 0x%02X%s\n",
                    addr,
                    addr == PCM5122_I2C_ADDR ? " <-- PCM5122?" : "");
      ++found;
    }
  }
  Serial.printf("--- Scan complete. Found %d device(s). ---\n", found);
}

static bool pingCodec() {
  Wire.beginTransmission(PCM5122_I2C_ADDR);
  return Wire.endTransmission() == 0;
}

static String resolveRedirectOnce(const char *url) {
  HTTPClient http;
  WiFiClient client;

  Serial.printf("HTTP probe: %s\n", url);

  if (!http.begin(client, url)) {
    Serial.println("[WARN] HTTP begin failed, use original URL");
    return String(url);
  }

  const int code = http.GET();
  Serial.printf("HTTP code = %d\n", code);

  String out = String(url);
  if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
    String loc = http.header("Location");
    if (loc.length()) {
      Serial.print("Redirect Location: ");
      Serial.println(loc);
      out = loc;
    } else {
      Serial.println("[WARN] Redirect but Location header empty, use original URL");
    }
  }

  http.end();
  return out;
}

// ---------------- ESP32-audioI2S callbacks ----------------
void audio_info(const char *info) {
  Serial.print("[audio_info] ");
  Serial.println(info);
}

void audio_showstreamtitle(const char *info) {
  Serial.print("[title] ");
  Serial.println(info);
}

void audio_bitrate(const char *info) {
  Serial.print("[bitrate] ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("[eof] ");
  Serial.println(info);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" PCM5122 + WiFi + ESP32-audioI2S ");
  Serial.println("=================================");
  Serial.printf("XSMT=%d, MCLK=%d, BCLK=%d, LRCK=%d, DOUT=%d, SDA=%d, SCL=%d\n",
                PIN_CODEC_XSMT, PIN_I2S_MCLK, PIN_I2S_BCLK, PIN_I2S_LRCK,
                PIN_I2S_DOUT, PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("PCM5122 I2C 7-bit address: 0x%02X\n", PCM5122_I2C_ADDR);
  printMemoryInfo();

  codecUnmuteByXSMT();
  delay(20);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  scanI2C();
  if (pingCodec()) {
    Serial.println("[OK] PCM5122 ACK on I2C.");
  } else {
    Serial.println("[WARN] PCM5122 did not ACK on I2C.");
    Serial.println("[WARN] Playback may still work if only I2S pins are correct.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("Connecting WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - t0 > 20000) {
      Serial.println("\n[FATAL] WiFi connect timeout.");
      while (true) {
        delay(1000);
      }
    }
  }

  Serial.println("\n[OK] WiFi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  if (!psramFound()) {
    Serial.println("[FATAL] PSRAM is not enabled.");
    Serial.println("[FATAL] In Arduino IDE set Tools -> PSRAM -> OPI PSRAM, then rebuild and upload.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Init Audio pinout (with MCLK)...");
  audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_MCLK);
  printMemoryInfo();
  audio.setVolume(AUDIO_VOLUME);
  Serial.printf("Audio volume: %u / 21\n", AUDIO_VOLUME);

  const String finalUrl = resolveRedirectOnce(URL_ORIG);
  Serial.print("Play URL: ");
  Serial.println(finalUrl);

  delay(100);
  audio.connecttohost(finalUrl.c_str());
}

void loop() {
  audio.loop();
}
