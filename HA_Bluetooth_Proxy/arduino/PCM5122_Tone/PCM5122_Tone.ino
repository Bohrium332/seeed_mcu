#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>

// ============================================================
// ESP32-S3 -> PCM5122 Arduino demo
//
// Schematic notes:
// 1. PCM5122 MODE1/MODE2 are strapped for I2C control mode.
// 2. The schematic marks the codec address as 0x98, which is the 8-bit form.
//    Arduino Wire uses the 7-bit form: 0x4C.
// 3. The audio-related GPIOs below are inferred from the schematic PDF.
//    If your final board differs, only change the macros in this block.
// ============================================================

// ---------------- Pin Macros ----------------
#define PIN_CODEC_XSMT  3   
#define PIN_I2S_MCLK    4   
#define PIN_I2S_BCK     5   
#define PIN_I2S_LRCK    6   
#define PIN_I2S_DOUT    7   

#define PIN_I2C_SDA     15  
#define PIN_I2C_SCL     16  

#define PCM5122_I2C_ADDR 0x4C

// ---------------- Audio Params ----------------
static constexpr uint32_t kSampleRate = 48000;
static constexpr uint16_t kToneHz = 1000;
static constexpr size_t kFramesPerBuffer = 256;
static constexpr float kAmplitude = 0.20f;

static int16_t audioBuffer[kFramesPerBuffer * 2];
static float phaseAcc = 0.0f;

static bool pingCodec() {
  Wire.beginTransmission(PCM5122_I2C_ADDR);
  return Wire.endTransmission() == 0;
}

static void scanI2C() {
  Serial.println("Scanning I2C bus...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found device at 0x%02X%s\n",
                    addr,
                    addr == PCM5122_I2C_ADDR ? "  <-- PCM5122?" : "");
      ++found;
    }
  }
  Serial.printf("I2C scan done, %u device(s) found.\n", found);
}

static void codecUnmuteByXSMT() {
  pinMode(PIN_CODEC_XSMT, OUTPUT);
  digitalWrite(PIN_CODEC_XSMT, HIGH);
}

static bool initI2S() {
  const i2s_config_t i2sConfig = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = static_cast<int>(kSampleRate),
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = static_cast<int>(kFramesPerBuffer),
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = static_cast<int>(kSampleRate * 256),
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_16BIT
  };

  const i2s_pin_config_t pinConfig = {
    .mck_io_num = PIN_I2S_MCLK,
    .bck_io_num = PIN_I2S_BCK,
    .ws_io_num = PIN_I2S_LRCK,
    .data_out_num = PIN_I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("i2s_driver_install failed: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_NUM_0, &pinConfig);
  if (err != ESP_OK) {
    Serial.printf("i2s_set_pin failed: %d\n", err);
    return false;
  }

  err = i2s_set_clk(I2S_NUM_0, kSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  if (err != ESP_OK) {
    Serial.printf("i2s_set_clk failed: %d\n", err);
    return false;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static void fillSineBuffer() {
  const float phaseStep = (2.0f * PI * kToneHz) / static_cast<float>(kSampleRate);

  for (size_t i = 0; i < kFramesPerBuffer; ++i) {
    const int16_t sample = static_cast<int16_t>(sinf(phaseAcc) * 32767.0f * kAmplitude);
    audioBuffer[i * 2 + 0] = sample;  // left
    audioBuffer[i * 2 + 1] = sample;  // right

    phaseAcc += phaseStep;
    if (phaseAcc >= 2.0f * PI) {
      phaseAcc -= 2.0f * PI;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=== ESP32-S3 PCM5122 tone demo ===");
  Serial.printf("XSMT=%d, MCLK=%d, BCK=%d, LRCK=%d, DOUT=%d, SDA=%d, SCL=%d\n",
                PIN_CODEC_XSMT, PIN_I2S_MCLK, PIN_I2S_BCK, PIN_I2S_LRCK,
                PIN_I2S_DOUT, PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.println("PCM5122 I2C 7-bit address: 0x4C");

  codecUnmuteByXSMT();
  delay(20);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  scanI2C();
  if (pingCodec()) {
    Serial.println("PCM5122 ACK OK on I2C.");
  } else {
    Serial.println("PCM5122 did not ACK on I2C.");
    Serial.println("If audio still plays, your I2S pins are fine and only SDA/SCL need adjustment.");
  }

  if (!initI2S()) {
    Serial.println("I2S init failed, stop.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("I2S started, outputting 1 kHz sine wave...");
}

void loop() {
  fillSineBuffer();

  size_t bytesWritten = 0;
  esp_err_t err = i2s_write(I2S_NUM_0,
                            audioBuffer,
                            sizeof(audioBuffer),
                            &bytesWritten,
                            portMAX_DELAY);
  if (err != ESP_OK) {
    Serial.printf("i2s_write failed: %d\n", err);
    delay(100);
  }
}
