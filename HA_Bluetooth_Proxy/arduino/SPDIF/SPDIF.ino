#include <Arduino.h>

extern "C" {
  #include "spdif.h"
}

#define SAMPLE_RATE 44100   // 音频采样率，标准CD音质
#define FREQ        1000.0  // 生成的正弦波频率：1000Hz

const int samples_per_cycle = SAMPLE_RATE / FREQ; // 计算一个完整周期包含的样本数,一个完整正弦波周期需要约44个样本
int16_t pcm_buf[samples_per_cycle * 2];           // 乘以2是因为立体声（左右声道）

void setup() {
  Serial.begin(115200);
  Serial.println("SPDIF Test Start");

  spdif_init(SAMPLE_RATE);

  for (int i = 0; i < samples_per_cycle; i++) {
    float t = i / (float)SAMPLE_RATE;
    float s = sinf(2 * PI * FREQ * t);
    int16_t v = (int16_t)(s * 20000);

    pcm_buf[i * 2 + 0] = v;
    pcm_buf[i * 2 + 1] = v;
  }
}

void loop() {
  spdif_write(pcm_buf, sizeof(pcm_buf));
}
