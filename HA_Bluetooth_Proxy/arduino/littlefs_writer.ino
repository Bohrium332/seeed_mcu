#include <Arduino.h>
#include <LittleFS.h>

// 这是一个一次性程序，用于将嵌入的 MP3 数据写入 LittleFS
// 运行一次后就可以删除或注释掉

// 包含 MP3 数据（注意：这会占用大量内存，只能用于小文件）
// 如果 MP3 太大，请使用 upload_littlefs.sh 脚本上传

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== LittleFS 文件写入工具 ===\n");

  // 初始化 LittleFS (format=true 会格式化文件系统)
  Serial.println("正在挂载 LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("错误: LittleFS 挂载失败!");
    return;
  }
  Serial.println("✓ LittleFS 挂载成功");

  // 显示文件系统信息
  Serial.print("总空间: ");
  Serial.print(LittleFS.totalBytes());
  Serial.println(" bytes");
  Serial.print("已使用: ");
  Serial.print(LittleFS.usedBytes());
  Serial.println(" bytes");
  Serial.print("可用空间: ");
  Serial.print(LittleFS.totalBytes() - LittleFS.usedBytes());
  Serial.println(" bytes\n");

  // 列出所有现有文件
  Serial.println("现有文件:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("  ");
    Serial.print(file.name());
    Serial.print(" - ");
    Serial.print(file.size());
    Serial.println(" bytes");
    file = root.openNextFile();
  }

  Serial.println("\n=================================");
  Serial.println("提示: 由于 MP3 文件较大，");
  Serial.println("建议使用脚本上传方式：");
  Serial.println("  cd ~/Arduino/es8156");
  Serial.println("  ./upload_littlefs.sh");
  Serial.println("=================================\n");

  // 如果你的 MP3 很小（<100KB），可以使用以下方式
  // 将 song.h 中的数据写入文件
  /*
  #include "song_small.h"  // 小的 MP3 数据文件
  
  Serial.println("正在写入 song.mp3...");
  File f = LittleFS.open("/song.mp3", "w");
  if (!f) {
    Serial.println("错误: 无法创建文件");
    return;
  }
  
  size_t written = f.write(song_mp3_data, song_mp3_len);
  f.close();
  
  Serial.print("✓ 写入成功: ");
  Serial.print(written);
  Serial.println(" bytes");
  */
}

void loop() {
  delay(1000);
}
