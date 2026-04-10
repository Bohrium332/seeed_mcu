// UART2 引脚定义
#define RS232_TX 17
#define RS232_RX 18

void setup() {
  Serial0.begin(115200);
  delay(1000);
  
  Serial0.println("ESP32-XIAO-S3 RS232_1 (Auto Direction) Test Start...");

  // 初始化 UART2
  Serial2.begin(115200, SERIAL_8N1, RS232_RX, RS232_TX);
}
 
void loop() 
{
  // ====== 发送数据 ======
  Serial2.print("Hello I AM RS232_1!\r\n");
  Serial0.println("[TX] Hello I AM RS232_1!");

}
