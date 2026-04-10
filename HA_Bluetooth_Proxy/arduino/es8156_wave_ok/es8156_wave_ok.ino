#include <Arduino.h>
#include <Wire.h>
#include "driver/i2s.h"

// ===================== 你的硬件引脚（按你前面可用的配置）=====================
#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   16
#define ES8156_ADDR   0x08

#define PIN_I2S_BCLK  11
#define PIN_I2S_LRCK  12
#define PIN_I2S_DOUT  13
#define PIN_I2S_MCLK  10

// ===================== 原厂参数（从 51 代码迁移）=====================
// Format & Length
#define NORMAL_I2S      0x00
#define Format_Len16    0x03

// VDDA select
#define VDDA_3V3        0x00
#define VDDA_1V8        0x01

#define MCLK_PIN        0x00
#define SCLK_PIN        0x01

// --------------------- 这些宏对应原厂可调项 ---------------------
#define MSMode_MasterSelOn  0       // 0: Slave, 1: Master
#define Ratio               256     // MCLK/LRCK
#define Format              NORMAL_I2S
#define Format_Len          Format_Len16
#define SCLK_DIV            4       // SCLK = MCLK / SCLK_DIV
#define SCLK_INV            0       // 0: falling edge align, 1: rising edge
#define MCLK_SOURCE         MCLK_PIN
#define EQ7bandOn           0
#define VDDA_VOLTAGE        VDDA_3V3
#define DAC_Volume          191     // 191: 0dB (原厂注释)
#define DACHPModeOn         0       // 0: PA/LOUT(省功耗), 1: 耳机驱动

// 51 工程里提到 “状态机确认寄存器读回 0x60” 的地址是 0x0C（原厂宏）
// #define STATEconfirm 0x0C
static const uint8_t REG_STATE_CONFIRM = 0x0C;

// ===================== I2C helpers =====================
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

static void dump_chipid() {
  uint8_t fd=0, fe=0, ff=0;
  es8156_read(0xFD, fd);
  es8156_read(0xFE, fe);
  es8156_read(0xFF, ff);
  Serial0.printf("ChipID read: FD=0x%02X FE=0x%02X FF=0x%02X\n", fd, fe, ff);

  uint8_t st = 0;
  if (es8156_read(REG_STATE_CONFIRM, st)) {
    Serial0.printf("STATE_CONFIRM (0x%02X) = 0x%02X (原厂期望约 0x60)\n", REG_STATE_CONFIRM, st);
  }
}

// ===================== 原厂 Reset / Powerdown / Standby（按寄存器序列迁移）====================
static void ES8156_Reset() {
  // 原厂：I2CWRNBYTE_CODEC(0x00,0x1C); I2CWRNBYTE_CODEC(0x00,0x01);
  es8156_write(0x00, 0x1C);
  delay(2);
  es8156_write(0x00, 0x01); // Slave Mode
  delay(2);
}

static void ES8156_Powerdown() {
  es8156_write(0x14, 0x00);
  es8156_write(0x19, 0x02);
  es8156_write(0x22, 0x02);
  es8156_write(0x25, 0x81);
  es8156_write(0x18, 0x01);
  es8156_write(0x09, 0x02);
  es8156_write(0x09, 0x01);
  es8156_write(0x08, 0x00);
  delay(500);
  es8156_write(0x25, 0x87);
}

static void ES8156_Standby_NoPop() {
  es8156_write(0x14, 0x00);
  es8156_write(0x19, 0x02);
  es8156_write(0x25, 0xA1);
  es8156_write(0x18, 0x01);
  es8156_write(0x09, 0x02);
  es8156_write(0x09, 0x01);
  es8156_write(0x08, 0x00);
}

// ===================== 原厂 ES8156_DAC（按寄存器写入顺序迁移）====================
static void ES8156_DAC() {
  // I2CWRNBYTE_CODEC(0x02, (MCLK_SOURCE<<7) + (SCLK_INV<<4) + (EQ7bandOn<<3) + 0x04 + MSMode_MasterSelOn);
  es8156_write(0x02, (MCLK_SOURCE << 7) | (SCLK_INV << 4) | (EQ7bandOn << 3) | 0x04 | (MSMode_MasterSelOn & 0x01));
  es8156_write(0x13, 0x00);

  if (DACHPModeOn == 0) { // PA/LOUT
    es8156_write(0x20, 0x2A);
    es8156_write(0x21, 0x3C);
    es8156_write(0x22, 0x02);
    es8156_write(0x24, 0x07);
    es8156_write(0x23, 0x40 + (0x30 * VDDA_VOLTAGE));
  } else { // 耳机
    es8156_write(0x20, 0x16);
    es8156_write(0x21, 0x3F);
    es8156_write(0x22, 0x0A);
    es8156_write(0x24, 0x01);
    es8156_write(0x23, 0xCA + (0x30 * VDDA_VOLTAGE));
  }

  es8156_write(0x0A, 0x01);
  es8156_write(0x0B, 0x01);
  es8156_write(0x11, Format + (Format_Len << 4));
  es8156_write(0x14, DAC_Volume);

  // Ratio 分支（完全照搬逻辑）
  if (Ratio == 1536) { es8156_write(0x01, 0x26 - (0x03 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 1024) { es8156_write(0x01, 0x24 - (0x02 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 768)  { es8156_write(0x01, 0x23 + (0x40 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 512)  { es8156_write(0x01, 0x22 - (0x01 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 400)  { es8156_write(0x01, 0x21 + (0x40 * EQ7bandOn)); es8156_write(0x09, 0x00); es8156_write(0x10, 0x64); }
  if (Ratio == 384)  { es8156_write(0x01, 0x63 + (0x40 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 256)  { es8156_write(0x01, 0x21 + (0x40 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 128)  { es8156_write(0x01, 0x61 + (0x40 * EQ7bandOn)); es8156_write(0x09, 0x00); }
  if (Ratio == 64)   { es8156_write(0x01, 0xA1);                        es8156_write(0x09, 0x02); }

  // LRCK/SCLK 分频
  es8156_write(0x03, (uint8_t)(Ratio >> 8));
  es8156_write(0x04, (uint8_t)(Ratio & 0xFF));
  es8156_write(0x05, (uint8_t)SCLK_DIV);

  es8156_write(0x0D, 0x14);
  es8156_write(0x18, 0x00);
  es8156_write(0x08, 0x3F);

  // 原厂最后的启动序列
  es8156_write(0x00, 0x02);
  es8156_write(0x00, 0x03);
  es8156_write(0x25, 0x20);

  Serial0.println("[OK] ES8156_DAC init done.");
}

// ===================== ESP32S3 I2S 初始化（先出时钟）====================
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const uint32_t SAMPLE_RATE = 48000;   // 对应 Ratio=256 => MCLK≈12.288MHz
static const uint32_t TONE_HZ     = 1000;

// 简单 1k 正弦（lookup table）
static int16_t sine_table[1024];
static uint32_t phase = 0;
static uint32_t phase_inc = 0;

static void build_sine_table() {
  for (int i = 0; i < 1024; i++) {
    float x = sinf(2.0f * PI * (float)i / 1024.0f);
    sine_table[i] = (int16_t)(x * 12000); // 振幅别太大，避免削顶爆音
  }
  phase_inc = (uint32_t)((double)TONE_HZ * (double)UINT32_MAX / (double)SAMPLE_RATE);
}

static bool i2s_init_start_clock() {
  i2s_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = true;
  cfg.tx_desc_auto_clear = true;

  // 固定 MCLK=fs*256，匹配 Ratio=256
  cfg.fixed_mclk = SAMPLE_RATE * 256;

  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    Serial0.printf("i2s_driver_install failed: %d\n", err);
    return false;
  }

  i2s_pin_config_t pin_cfg;
  memset(&pin_cfg, 0, sizeof(pin_cfg));
  pin_cfg.bck_io_num = PIN_I2S_BCLK;
  pin_cfg.ws_io_num = PIN_I2S_LRCK;
  pin_cfg.data_out_num = PIN_I2S_DOUT;
  pin_cfg.data_in_num = I2S_PIN_NO_CHANGE;
  pin_cfg.mck_io_num = PIN_I2S_MCLK;

  err = i2s_set_pin(I2S_PORT, &pin_cfg);
  if (err != ESP_OK) {
    Serial0.printf("i2s_set_pin failed: %d\n", err);
    return false;
  }

  err = i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  if (err != ESP_OK) {
    Serial0.printf("i2s_set_clk failed: %d\n", err);
    return false;
  }

  // 写一点 0，让 BCLK/LRCK/MCLK 立刻跑起来（满足“先 IIS 后 IIC”的要求）
  int16_t zeros[256 * 2] = {0}; // 256 frames * stereo
  size_t written = 0;
  i2s_write(I2S_PORT, zeros, sizeof(zeros), &written, 50 / portTICK_PERIOD_MS);

  Serial0.println("[OK] I2S clocks started.");
  return true;
}

// ===================== Arduino setup/loop =====================
void setup() {
  Serial0.begin(115200);
  delay(200);

  Serial0.println("=== ES8156 Arduino Test (ported from 51 ref) ===");

  // 1) 先启动 I2S（先有时钟）
  if (!i2s_init_start_clock()) {
    Serial0.println("[ERR] I2S init failed.");
    while (1) delay(1000);
  }

  // 2) 再启动 I2C
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  // 3) 读下 ChipID / 状态寄存器
  dump_chipid();

  // 4) 按原厂建议：有 IIS 时钟后做一次 Reset（避免“先 IIC 后 IIS”导致异常）
  ES8156_Reset();

  // 5) 配置 DAC
  ES8156_DAC();

  dump_chipid();

  build_sine_table();
  Serial0.println("[OK] Start output 1kHz sine via I2S...");
}

void loop() {
  // 连续输出 1kHz 正弦（给你一个“听得见”的验证）
  static int16_t pcm[256 * 2]; // 256 frames * stereo
  for (int i = 0; i < 256; i++) {
    uint16_t idx = (uint16_t)(phase >> 22); // 32-bit phase -> top 10 bits => 0..1023
    int16_t s = sine_table[idx];
    pcm[i * 2 + 0] = s; // L
    pcm[i * 2 + 1] = s; // R
    phase += phase_inc;
  }

  size_t written = 0;
  i2s_write(I2S_PORT, pcm, sizeof(pcm), &written, portMAX_DELAY);

  // 你想测试待机/关断的话，可以取消注释：
  /*
  static uint32_t t0 = millis();
  if (millis() - t0 > 8000) {
    Serial0.println("[TEST] Standby no-pop...");
    ES8156_Standby_NoPop();
    delay(2000);
    Serial0.println("[TEST] Wake + re-init...");
    ES8156_Reset();
    ES8156_DAC();
    t0 = millis();
  }
  */
}
