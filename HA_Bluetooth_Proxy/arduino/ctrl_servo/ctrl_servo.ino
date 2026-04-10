#include <ESP32Servo.h>

// 创建舵机对象
Servo myservo1;
Servo myservo2;

// 定义引脚 (XIAO ESP32C3 的引脚定义)
// 建议使用 D0, D1, D2 等通用引脚
const int servoPin1 = D0;
const int servoPin2 = D1;

void setup() {
  // 分配硬件定时器 (ESP32特有步骤，确保PWM更稳定)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // 设置频率为 50Hz (标准舵机频率)
  myservo1.setPeriodHertz(50);
  myservo2.setPeriodHertz(50);

  // 连接舵机并设置脉宽范围
  // 标准舵机通常是 500us - 2500us 对应 0 - 180度
  // 如果你的舵机转不到180度，可以微调这两个数值
  myservo1.attach(servoPin1, 500, 2500);
  myservo2.attach(servoPin2, 500, 2500);

  Serial.begin(115200);
  Serial.println("System Ready: Servos attached.");
}

void loop() {
  Serial.println("Moving to 180 degrees...");
  
  // 转动到 180 度
  myservo1.write(170);
  myservo2.write(170);

  // 停 3 秒
  delay(3000);

  Serial.println("Moving back to 0 degrees...");
  
  // 转回到 0 度
  myservo1.write(10);
  myservo2.write(10);

  // 这里建议也加一个延时，否则回到0度后会瞬间开始下一次循环往180度转
  // 如果你想让它在0度也停3秒，就用 delay(3000);
  delay(3000); 
}