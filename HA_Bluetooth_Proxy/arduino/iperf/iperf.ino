/*
 * Hardware requirements：
 * ESP32-S3 + W5500
 * iperf TCP server
 *
 * PC Command Client ：
 *   iperf -c <ESP32_IP> -t 20 -i 2 -w 16k
 */

#include <SPI.h>
#include <ETH.h>
#include <WiFi.h>

static bool eth_connected = false;

#ifndef ETH_PHY_CS
#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_PHY_CS   6
#define ETH_PHY_IRQ  5
#define ETH_PHY_RST  -1
#endif

#define ETH_SPI_SCK  7
#define ETH_SPI_MISO 8
#define ETH_SPI_MOSI 9

WiFiServer iperfServer(5001);

void onEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial0.println("ETH Started");
      ETH.setHostname("esp32-iperf");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial0.println("ETH Connected");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial0.print("ETH IP: ");
      Serial0.print(ETH.localIP());
      Serial0.print("  Speed: ");
      Serial0.print(ETH.linkSpeed());
      Serial0.print("Mbps ");
      Serial0.println(ETH.fullDuplex() ? "FULL" : "HALF");
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

void handleIperfClient()
{
  WiFiClient client = iperfServer.available();
  static uint8_t buf[1460];

  if (client) {
    Serial0.println("iperf client connected");

    uint32_t totalBytes = 0;
    uint32_t startMs = millis();

    while (client.connected()) {
      int len = client.available();
      if (len > 0) {
        int r = client.read(buf, min(len, (int)sizeof(buf)));
        totalBytes += r;
      }
      delay(0); // feed watchdog
    }

    uint32_t durMs = millis() - startMs;
    float mbps = (totalBytes * 8.0f) / (durMs / 1000.0f) / 1e6;

    Serial0.printf(
      "iperf finished: %u bytes, %u ms, %.2f Mbps\n",
      totalBytes, durMs, mbps
    );

    client.stop();
    Serial0.println("iperf client disconnected");
  }
}

void setup()
{
  delay(500);
  Serial0.begin(115200);
  Serial0.println("\nESP32 W5500 iperf server");

  Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(
    ETH_PHY_TYPE,
    ETH_PHY_ADDR,
    ETH_PHY_CS,
    ETH_PHY_IRQ,
    ETH_PHY_RST,
    SPI
  );

  Serial0.print("Waiting for ETH");
  while (!eth_connected) {
    delay(500);
    Serial0.print(".");
  }
  Serial0.println();

  iperfServer.begin();
  Serial0.println("iperf TCP server started on port 5001");
}

void loop()
{
  if (eth_connected) {
    handleIperfClient();
  }
}
