/*
 * 伺服端，啟動後會架一組 wifi server，連上的 client 會透過 udp 12345 丟轉速訊號來
 * Author: 羽山 (https://3wa.tw)
 * Author: @FB 田峻墉
 * Release Date: 2024-09-21
 * D1 接至 PC817，為轉速訊號接入端
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>

#define LED_PIN    D1 //接 Pixel LED
#define NUMPIXELS 25 // 25顆 LED
Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// 熱點名稱和密碼
const char* ssid = "3wa_helmet_server";
const char* password = "********";

// 指定熱點的 IP 地址、網關和子網掩碼
IPAddress local_ip(192,168,1,254);
IPAddress gateway(192,168,1,254); // 通常將網關設置為熱點的 IP 地址
IPAddress subnet(255,255,255,0);

// 創建一個 UDP 物件
WiFiUDP udp;
const int udpPort = 12345;

// 轉速範圍對應顏色
uint32_t getRPMColor(int rpm) {
  if (rpm <= 4000) {
    return strip.Color(0, 255, 0); // 綠色
  } else if (rpm <= 6000) {
    return strip.Color(255, 255, 0); // 黃色
  } else if (rpm <= 7500) {
    return strip.Color(255, 165, 0); // 橙色
  } else if (rpm < 10000) {
    return strip.Color(255, 0, 0); // 紅色
  } else {
    static bool blinkState = false;
    blinkState = !blinkState;
    return blinkState ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0); // 紅色閃爍
  }
}
uint32_t blendColors(uint32_t color1, uint32_t color2, int blend) {
  uint8_t r1 = (color1 >> 16) & 0xFF;
  uint8_t g1 = (color1 >> 8) & 0xFF;
  uint8_t b1 = color1 & 0xFF;

  uint8_t r2 = (color2 >> 16) & 0xFF;
  uint8_t g2 = (color2 >> 8) & 0xFF;
  uint8_t b2 = color2 & 0xFF;

  uint8_t r = (r1 * (255 - blend) + r2 * blend) / 255;
  uint8_t g = (g1 * (255 - blend) + g2 * blend) / 255;
  uint8_t b = (b1 * (255 - blend) + b2 * blend) / 255;

  return strip.Color(r, g, b);
}
void displayOnLed(int show_rpm)
{
  // 將轉速顯示在NeoPixel LED
  int led_count = map(show_rpm, 0, 9999, 0, NUMPIXELS);
  for (int i = 0; i < NUMPIXELS; i++) {
    if (i < led_count) {
      uint32_t color1, color2;
      int range_start, range_end;

      if (show_rpm <= 6000) {
        color1 = strip.Color(0, 255, 0); // 綠色
        color2 = strip.Color(0, 255, 0); // 綠色
        range_start = 0;
        range_end = 6000;
      } else if (show_rpm <= 8000) {
        color1 = strip.Color(0, 255, 0); // 綠色
        color2 = strip.Color(255, 255, 0); // 黃色
        range_start = 6000;
        range_end = 8000;
      } else if (show_rpm <= 9500) {
        color1 = strip.Color(255, 255, 0); // 黃色
        color2 = strip.Color(255, 165, 0); // 橙色
        range_start = 8000;
        range_end = 9500;
      } else {
        color1 = strip.Color(255, 165, 0); // 橙色
        color2 = strip.Color(255, 0, 0); // 紅色
        range_start = 9500;
        range_end = 10000;
      }

      int local_rpm = map(show_rpm, range_start, range_end, 0, 255);
      uint32_t color = blendColors(color1, color2, local_rpm);
      strip.setPixelColor(i, color);
    } else {
      strip.setPixelColor(i, 0); // 關閉LED
    }
  }
  strip.show();
}
void setup() {
  // 初始化串口通訊
  Serial.begin(115200);
  Serial.println();
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  strip.setBrightness(20);
  
  // 配置熱點的 IP 地址、網關和子網掩碼
  WiFi.softAPConfig(local_ip, gateway, subnet);

  // 啟動 WiFi 熱點
  WiFi.softAP(ssid, password); 

  // 打印熱點的 IP 地址
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // 打印熱點啟動信息
  Serial.println("WiFi AP started");

  // 啟動 UDP
  udp.begin(udpPort);

  // 打印熱點啟動信息
  Serial.println("WiFi AP started");
}

void loop() {  
  // 監聽 UDP 端口 12345
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0; // Null-terminate the string
    }
    Serial.printf("Received UDP packet: %s\n", incomingPacket);
    int rpm = atoi(incomingPacket);
    displayOnLed(rpm);
    
    // 應該不用 回應收到的數據
    //udp.beginPacket(udp.remoteIP(), udp.remotePort());
    //udp.write("Message received");
    //udp.endPacket();
  }
}
