#include <WiFi.h>

const char* ssid = "dudu";   // WiFi 名称
const char* psw = "poi55885"; 

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("正在连接 WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);

  // 连接无密码 WiFi
  WiFi.begin(ssid, psw);

  // 等待连接
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // 连接成功
  Serial.println();
  Serial.println("✅ WiFi 已连接！");
  Serial.print("📶 本机 IP 地址：");
  Serial.println(WiFi.localIP());
}

void loop() {
  
}
