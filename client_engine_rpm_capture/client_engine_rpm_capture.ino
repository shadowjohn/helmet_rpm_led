/*
 * 轉速表，接高壓線圈輸入端綠線或黑線，或接轉速表訊號線，或凸台脈衝線圈訊號線
 * 這版接收到轉速表的訊號，會連上另一組 Arduino D1 mini 的 wifi
 * 成功連線後，會持續把轉速送出 udp 12345 port 至 server (192.168.1.254)
 * Author: 羽山 (https://3wa.tw)
 * Author: @FB 田峻墉
 * Release Date: 2024-07-15
 * D1 接至 PC817，為轉速訊號接入端
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

#define WIFI_SSID "3wa_helmet_server"
#define WIFI_PASSWORD "********"

const char* udpAddress = "192.168.1.254"; // 伺服端(安全帽)的 IP 地址
const int udpPort = 12345; // 接收端的端口
WiFiUDP udp;

Ticker wifiReconnectTimer;
#define ToPin D6    //凸台、或轉速訊號線

volatile unsigned long C = micros(); //本次偵測到凸台的時間
volatile unsigned long C_old = 0; //上一次偵測到凸台的時間
volatile unsigned long rpm = 0; //換算後的轉速
volatile unsigned long RPM_DELAY = 0; //每一次轉速，凸台與凸台經過的時間
volatile unsigned int isShowCount = 0; //每次加一，每經過 100 次才更新一次七段，以免眼睛跟不上

/*
轉速   60 轉 =  每分鐘    60 轉，每秒  1    轉，1轉 = 1          秒 = 1000.000 ms = 1000000us
轉速   100 轉 = 每分鐘   100 轉，每秒  1.67 轉，1轉 = 0.598802   秒 =  598.802 ms =  598802us
轉速   200 轉 = 每分鐘   200 轉，每秒  3.3  轉，1轉 = 0.300003   秒 =  300.003 ms =  300003us
轉速   600 轉 = 每分鐘   600 轉，每秒  10   轉，1轉 = 0.1        秒 =  100.000 ms =  100000us
轉速  1500 轉 = 每分鐘  1500 轉，每秒  25   轉，1轉 = 0.04       秒 =   40.000 ms =   40000us
轉速  6000 轉 = 每分鐘  6000 轉，每秒  60   轉，1轉 = 0.01666... 秒 =   16.667 ms =   16667us
轉速 14000 轉 = 每分鐘 14000 轉，每秒 233.3 轉，1轉 = 0.0042863. 秒 =    4.286 ms =    4286us
轉速 14060 轉 = 每分鐘 14060 轉，每秒 240   轉，1轉 = 0.0041667. 秒 =    4.167 ms =    4167us
轉速 16000 轉 = 每分鐘 16000 轉，每秒 266.6 轉，1轉 = 0.0037500. 秒 =    3.750 ms =    3750us 
*/
void ICACHE_RAM_ATTR countup() {      
  //新版的 Nodemcu 在使用 ISR 中斷，Function 要加上 ICACHE_RAM_ATTR
  //偵測到凸台RISING，就會觸發此 function countup  
  C = micros(); //記錄當下的時間
  // (1/(16000/60) * 1000 * 1000 = 3750
  // (1/(17000/60) * 1000 * 1000 = 3529  
  // 不可能有超過 17000rpm 的狀況
  RPM_DELAY = C - C_old; //現在的時間減去上一次觸發的時間  
  if(RPM_DELAY < 3500) {
    //超過 17000rpm 了
    return;
  }
  if(RPM_DELAY > 598802) {
    //低於 100rpm
    C_old = C;
    rpm = 0;
    return;
  }    
  //其他轉速，計算得出轉速度
  rpm = 60000000UL / RPM_DELAY;
  //把上一次凸台的時間改成現在時間
  C_old = C;  
}
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  Serial.print("IP address: ");
  Serial.println(event.ip.toString());  // 打印 IP 地址 
}
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");  
  wifiReconnectTimer.once(2, connectToWifi);
}
void setup() {  
  Serial.begin(115200); //注意包率設很高，要在監視器看的話要改一下
  Serial.println("Counting...");
  //宣告觸發腳位為 INPUT_PULLUP
  pinMode(ToPin, INPUT_PULLUP);    
  //註冊中斷觸發
  attachInterrupt(digitalPinToInterrupt(ToPin), countup, RISING); //RISING

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  connectToWifi(); 
}

void loop() {
  //七段顯示器不能一直刷數字，不然人類的眼睛會追不上
  //isShowCount 每一次都加1，計數100次才改變一次七段顯示器的內容，顯示完就歸零
  //如果覺得眼睛還是追不上，可以把 100 調大一些，如 150、200
  isShowCount++;  
  if (isShowCount > 100)
  {    
    isShowCount = 0;   
    if(micros() - C_old > 598802) {
      //2021-09-21 針對訊號源消失的處理
      //低於 100rpm      
      rpm = 0;      
    }    
    //七段最多顯示到 9999，所以超過 10000 都變 9999
    //rpm = (rpm>=10000)?9999:rpm;  
    //顯示在七段上
    //預設為 2 行程使用
    //如果為 4 行程引擎，要 x 2 倍
    //rpm *= 2;
    displayOnLed(rpm);   
  }  
}
void displayOnLed(int show_rpm)
{
  // 把轉速訊號傳去 server 顯示
  char buffer[10];
  itoa(show_rpm, buffer, 10); // 將轉速數據轉換為字串

  if(WiFi.status() == WL_CONNECTED){
    udp.beginPacket(udpAddress, udpPort);
    udp.write(buffer);
    udp.endPacket();
    Serial.println(show_rpm);        
  }
}
