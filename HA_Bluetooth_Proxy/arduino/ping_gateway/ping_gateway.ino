/*
 * 实验内容：
 * ESP32 + W5500
 * 持续 ping 网关 192.168.4.2
 * 打印 RTT / 丢包率
 * 下载库 ：https://github.com/marian-craciunescu/ESP32Ping
 */

#include <SPI.h>
#include <ETH.h>
#include <WiFi.h>
#include <ESP32Ping.h>

static bool eth_connected = false;

// ================= Ethernet PHY 配置 =================
#ifndef ETH_PHY_CS
#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_PHY_CS   6
#define ETH_PHY_IRQ  5
#define ETH_PHY_RST  -1
#endif

// SPI pins
#define ETH_SPI_SCK  7
#define ETH_SPI_MISO 8
#define ETH_SPI_MOSI 9

// ================= Ping 参数 =================
IPAddress gatewayIP(192, 168, 4, 2);
const uint32_t PING_INTERVAL_MS = 1000;

// 统计变量
uint32_t ping_sent = 0;
uint32_t ping_recv = 0;

// ================= Ethernet 事件回调 =================
void onEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial0.println("ETH Started");
      ETH.setHostname("esp32-w5500-ping");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial0.println("ETH Connected");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial0.print("ETH MAC: ");
      Serial0.print(ETH.macAddress());
      Serial0.print(" IP: ");
      Serial0.println(ETH.localIP());
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

// ================= Setup =================
void setup()
{
  delay(500);
  Serial0.begin(115200);
  Serial0.println("\nESP32 W5500 Ping Test");

  Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);

  Serial0.print("Waiting for Ethernet");
  while (!eth_connected) {
    delay(500);
    Serial0.print(".");
  }
  Serial0.println("\nEthernet ready!");
}

// ================= Loop =================
void loop()
{
  static uint32_t last_ping = 0;

  if (!eth_connected) {
    delay(1000);
    return;
  }

  if (millis() - last_ping >= PING_INTERVAL_MS) {
    last_ping = millis();
    ping_sent++;

    Serial0.println("-----------------------------------");
    Serial0.print("Ping ");
    Serial0.print(gatewayIP);
    Serial0.print("  ");

    uint32_t start = millis();
    bool success = Ping.ping(gatewayIP, 1); // ping 1 次
    uint32_t elapsed = millis() - start;

    if (success) {
      ping_recv++;
      Serial0.print("Reply time=");
      Serial0.print(elapsed);
      Serial0.println(" ms");
    } else {
      Serial0.println("Request timeout");
    }

    float loss = 100.0 * (ping_sent - ping_recv) / ping_sent;

    Serial0.print("发送次数=");
    Serial0.print(ping_sent);
    Serial0.print("  收到次数=");
    Serial0.print(ping_recv);
    Serial0.print("  丢包率=");
    Serial0.print(loss, 1);
    Serial0.println("%");
  }
}
