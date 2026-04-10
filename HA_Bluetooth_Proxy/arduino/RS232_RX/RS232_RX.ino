// UART2 引脚定义
#define RS232_TX 47
#define RS232_RX 48

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("ESP32-XIAO-S3 RS232_2 Receive Test Start...");

  // UART2 初始化（用于接收）
  Serial2.begin(115200, SERIAL_8N1, RS232_RX, RS232_TX);

  Serial.println("Ready to receive data...");
}

void loop() 
{
  // 只要 UART2 有数据就读取
  if (Serial2.available()) 
  {
    String incoming = Serial2.readStringUntil('\n');  // 读一行
    Serial.print("[RX] Received: ");
    Serial.println(incoming);
  }
}
