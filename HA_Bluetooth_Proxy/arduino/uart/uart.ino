// ============================
//  XIAO-ESP32-S3 RS485 测试
//  UART2 (Serial2) 仅 TX/RX
// ============================

// UART2 引脚定义
#define RS485_TX 17
#define RS485_RX 18

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-XIAO-S3 RS485 (Auto Direction) Test Start...");

  // 初始化 UART2
  Serial2.begin(115200, SERIAL_8N1, RS485_RX, RS485_TX);
}

void loop() 
{
  // ====== 发送数据 ======
  Serial2.print("Hello from ESP32-XIAO-S3 via RS485!\r\n");
  Serial.println("[TX] Hello from ESP32-XIAO-S3");

  delay(500);

  // ====== 接收数据 ======
  while (Serial2.available()) {
    String data = Serial2.readString();
    Serial.print("[RX] ");
    Serial.println(data);
  }

  delay(1500);
}
