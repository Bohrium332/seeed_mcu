#include <SPI.h>
#include <ETH.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"
#include "WiFi.h"

// W5500 以太网配置
#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_PHY_CS   6
#define ETH_PHY_IRQ  5
#define ETH_PHY_RST  -1

// SPI 引脚配置
#define ETH_SPI_SCK  7
#define ETH_SPI_MISO 8
#define ETH_SPI_MOSI 9

// I2C 引脚配置 (ES8156音频芯片)
#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   16

// I2C1 引脚配置 (Grove I2C总线)
#define PIN_I2C1_SDA  1
#define PIN_I2C1_SCL  2
#define PIN_I2C1_EN   45   // I2C1使能引脚,使用前需拉高

// I2S 引脚配置
#define PIN_I2S_BCLK  11
#define PIN_I2S_LRCK  12
#define PIN_I2S_DOUT  13
#define PIN_I2S_MCLK  10

// ES8156 I2C 地址
#define ES8156_ADDR   0x08

// WS2812B 配置
#define LED_PIN     38
#define NUM_LEDS    1
#define BRIGHTNESS  255    // 亮度拉满

// RS232 引脚定义
#define RS232_TX 17
#define RS232_RX 18

// RS485 引脚定义
#define RS485_TX 4
#define RS485_RX 3

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// 以太网连接状态
static bool eth_connected = false;

// ===================== ES8156 原厂参数配置 =====================
// Format & Length
#define NORMAL_I2S      0x00
#define Format_Len16    0x03

// VDDA select
#define VDDA_3V3        0x00
#define VDDA_1V8        0x01

#define MCLK_PIN        0x00
#define SCLK_PIN        0x01

// ES8156 原厂配置参数
#define MSMode_MasterSelOn  0       // 0: Slave, 1: Master
#define Ratio               256     // MCLK/LRCK
#define Format              NORMAL_I2S
#define Format_Len          Format_Len16
#define SCLK_DIV            4       // SCLK = MCLK / SCLK_DIV
#define SCLK_INV            0       // 0: falling edge align, 1: rising edge
#define MCLK_SOURCE         MCLK_PIN
#define EQ7bandOn           0
#define VDDA_VOLTAGE        VDDA_3V3
#define DAC_Volume          191     // 191: 0dB 
#define DACHPModeOn         0       

static const uint8_t REG_STATE_CONFIRM = 0x0C;

// =======================
// ES8156 I2C 读写函数
// =======================
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

// =======================
// ES8156 初始化序列
// =======================
static void ES8156_Reset() {
  es8156_write(0x00, 0x1C);
  delay(2);
  es8156_write(0x00, 0x01); // Slave Mode
  delay(2);
}

static void ES8156_DAC() {
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

  // Ratio 分支配置
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

  // 原厂启动序列
  es8156_write(0x00, 0x02);
  es8156_write(0x00, 0x03);
  es8156_write(0x25, 0x20);

  Serial0.println("[OK] ES8156_DAC init done (factory config)");
}

// =======================
// I2S 初始化 (配置: 48kHz, Ratio=256)
// =======================
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const uint32_t SAMPLE_RATE = 48000;   // 对应 Ratio=256 => MCLK≈12.288MHz
static const uint32_t TONE_HZ     = 1000;

// 正弦波查找表
static int16_t sine_table[1024];
static uint32_t phase = 0;
static uint32_t phase_inc = 0;

static void build_sine_table() {
  for (int i = 0; i < 1024; i++) {
    float x = sinf(2.0f * PI * (float)i / 1024.0f);
    sine_table[i] = (int16_t)(x * 12000); // 振幅避免削顶
  }
  phase_inc = (uint32_t)((double)TONE_HZ * (double)UINT32_MAX / (double)SAMPLE_RATE);
}

static bool i2s_init_start_clock()
{
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

  // 写一点 0，让 BCLK/LRCK/MCLK 立刻跑起来（满足"先 IIS 后 IIC"的要求）
  int16_t zeros[256 * 2] = {0}; // 256 frames * stereo
  size_t written = 0;
  i2s_write(I2S_PORT, zeros, sizeof(zeros), &written, 50 / portTICK_PERIOD_MS);

  Serial0.println("[OK] I2S clocks started (先时钟后I2C配置)");
  return true;
}

// =======================
// 1kHz 正弦波测试音 (RTOS 任务)
// =======================
void audioTask(void *parameter)
{
  // 使用查找表生成正弦波
  static int16_t pcm[256 * 2]; // 256 frames * stereo
  
  size_t bytes_written;
  while (true) {
    // 生成一批音频数据
    for (int i = 0; i < 256; i++) {
      uint16_t idx = (uint16_t)(phase >> 22); // 32-bit phase -> top 10 bits => 0..1023
      int16_t s = sine_table[idx];
      pcm[i * 2 + 0] = s; // L
      pcm[i * 2 + 1] = s; // R
      phase += phase_inc;
    }
    
    i2s_write(I2S_PORT, pcm, sizeof(pcm), &bytes_written, portMAX_DELAY);
    Serial1.print("Hello I am RS232!\r\n");
    Serial2.print("Hello I am RS485!\r\n");
  }
}

// 以太网事件处理函数
void onEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  switch (event) {

    case ARDUINO_EVENT_ETH_START:
      Serial0.println("ETH Started");
      ETH.setHostname("esp32-ethernet");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial0.println("ETH Connected");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial0.print("Got an IP Address for ETH MAC: ");
      Serial0.print(ETH.macAddress());
      Serial0.print(", IPv4: ");
      Serial0.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial0.print(", FULL_DUPLEX");
      }
      Serial0.print(", ");
      Serial0.print(ETH.linkSpeed());
      Serial0.println("Mbps");
      eth_connected = true;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial0.println("ETH Disconnected");
      eth_connected = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      Serial0.println("ETH Stopped");
      eth_connected = false;
      break;

    default:
      break;
  }
}

// 测试网络连接
void testClient(const char * host, uint16_t port)
{
  Serial0.print("\nConnecting to ");
  Serial0.print(host);
  Serial0.print(":");
  Serial0.println(port);

  NetworkClient client;
  if (!client.connect(host, port)) {
    Serial0.println("connection failed");
    return;
  }
  client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) {
    Serial0.write(client.read());
  }

  Serial0.println("closing connection\n");
  client.stop();
}

void setup() {
  Serial0.begin(115200);
  delay(1000);
  Serial1.begin(115200, SERIAL_8N1, RS232_RX, RS232_TX);
  delay(1000);
  Serial2.begin(115200, SERIAL_8N1, RS485_RX, RS485_TX);
  delay(1000);
  Serial0.println("Serial All Started");

  // ========== I2C0 初始化 (ES8156音频) ==========
  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.begin();

  // 扫描 I2C0 设备
  Serial0.println("Scanning I2C0 (ES8156)...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial0.print("  Found I2C0 device at 0x");
      Serial0.println(addr, HEX);
    }
  }

  // ========== I2C1 初始化 (Grove 总线) ==========
  // 1. 先拉高使能引脚
  pinMode(PIN_I2C1_EN, OUTPUT);
  digitalWrite(PIN_I2C1_EN, HIGH);
  delay(10); // 等待使能生效
  
  // 2. 初始化I2C1
  Wire1.setPins(PIN_I2C1_SDA, PIN_I2C1_SCL);
  Wire1.begin();
  Wire1.setClock(100000); // 100kHz
  
  // 3. 扫描I2C1设备
  Serial0.println("Scanning I2C1 (GPIO1/GPIO2, EN=GPIO45)...");
  bool found_i2c1 = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() == 0) {
      Serial0.print("  Found I2C1 device at 0x");
      Serial0.println(addr, HEX);
      found_i2c1 = true;
    }
  }
  if (!found_i2c1) {
    Serial0.println("  No I2C1 devices found");
  }

  // ========== ES8156 + I2S 初始化 (原厂顺序:先I2S时钟,后I2C配置) ==========
  Serial0.println("=== Starting ES8156 Audio Init (Factory Method) ===");
  
  // 1. 先启动 I2S 时钟
  if (!i2s_init_start_clock()) {
    Serial0.println("[ERR] I2S init failed.");
  }
  
  // 2. 构建正弦波查找表
  build_sine_table();
  
  // 3. I2S时钟运行后,再进行I2C配置
  ES8156_Reset();
  ES8156_DAC();
  
  Serial0.println("Audio ready (factory config)!");

  // 创建音频播放任务 (运行在核心0，优先级1，堆栈4096字节)
  xTaskCreatePinnedToCore(
    audioTask,      // 任务函数
    "AudioTask",    // 任务名称
    4096,           // 堆栈大小
    NULL,           // 参数
    1,              // 优先级
    NULL,           // 任务句柄
    0               // 运行在核心0 (loop运行在核心1)
  );
  Serial0.println("Audio task started on Core 0!");

  // ========== WS2812B 初始化 ==========
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  // 设置全白灯光 (R=255, G=255, B=255)
  strip.setPixelColor(0, strip.Color(255, 255, 255));
  strip.show();
  Serial0.println("WS2812B 全白灯光已开启!");

  // ========== WiFi AP 热点配置 ==========
  WiFi.mode(WIFI_MODE_AP);

  bool apStatus = WiFi.softAP("ESP32_AP_TEST", "12345678", 1, 0, 4);
  /* 19.5dBm发射 */
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  if (apStatus) 
  {
    Serial0.println("热点创建成功!");
    Serial0.print("热点IP地址: ");
    Serial0.println(WiFi.softAPIP());
  }
  else 
  {
    Serial0.println("热点创建失败!");
  }

  // ========== W5500 以太网配置 ==========
  Serial0.println("Registering event handler for ETH events...");
  Network.onEvent(onEvent);

  Serial0.println("Starting ETH interface...");
  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);

  Serial0.println("Waiting for Ethernet connection...");
  // 等待以太网连接
  int timeout = 20;
  while (!eth_connected && timeout > 0) 
  {
    delay(500);
    Serial0.print(".");
    timeout--;
  }
  
  if (eth_connected) {
    Serial0.println("\nEthernet connected!");
  } else {
    Serial0.println("\nEthernet connection timeout, continuing without ETH...");
  }
}

void loop() {

  if (eth_connected) 
  {
    testClient("baidu.com", 80);
  } else {
    Serial0.println("以太网未连接");
  }
  
  // delay(100);
}
