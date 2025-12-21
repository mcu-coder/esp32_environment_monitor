#include <WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
//DHT11
#include <Bonezegei_DHT11.h>
//OLED
#include <U8g2lib.h>
#include <Wire.h>
//JSON
#include <ArduinoJson.h>

// 设置wifi接入信息(请根据您的WiFi信息进行修改)
const char* ssid = "NET";
const char* password = "12345678";
const char* mqttServer = "iot-06z00axdhgfk24n.mqtt.iothub.aliyuncs.com";
// 如以上MQTT服务器无法正常连接，请前往以下页面寻找解决方案
// http://www.taichi-maker.com/public-mqtt-broker/

Ticker ticker;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
int count;  // Ticker计数用变量
int count_2;
// ****************************************************
 

//遗嘱相关信息
const char* willMsg = "esp8266 offline";  // 遗嘱主题信息
const int willQos = 0;                    // 遗嘱QoS
const int willRetain = false;             // 遗嘱保留

const int subQoS = 1;            // 客户端订阅主题时使用的QoS级别（截止2020-10-07，仅支持QoS = 1，不支持QoS = 2）
const bool cleanSession = true;  // 清除会话（如QoS>0必须要设为false）


// LED 配置
#define LED_BUILTIN 2
bool ledStatus = LOW;
#define LED_1 5
int led_1_state = 0;

// Beep 配置
#define BEEP 18

//DHT11
Bonezegei_DHT11 dht(4);
int humidity = 0;
float temperature = 0;

//阈值
int humidity_V = 70;
int temperature_V = 28;

#define SCL 22
#define SDA 21
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, /*reset=*/U8X8_PIN_NONE);
char str[50];  //拼接字符使用

void setup() {

  Serial.begin(9600);  // 启动串口通讯
  // Dht11
  dht.begin();
  
  // 连接MQTT服务器
  connectMQTTserver();
  // Ticker定时对象
  ticker.attach(1, tickerCount);
 
  digitalWrite(LED_BUILTIN, ledStatus);  // 启动后关闭板上LED
  digitalWrite(BEEP, ledStatus);  // 启动后关闭板上LED
}
void loop() {
  // 如果开发板未能成功连接服务器，则尝试连接服务器
  if (!mqttClient.connected()) {
    connectMQTTserver();
  } else {
    //1秒更新一次
    if (count_2 >= 1) {
        //大于阈值数据，蜂鸣器报警
      if (temperature> temperature_V || humidity>humidity_V){
        digitalWrite(BEEP, HIGH);  // 打开蜂鸣器。
      } else {
        digitalWrite(BEEP, LOW);  // 关闭蜂鸣器。
      }
    
      Serial.printf("Temperature: %0.1lf °C  Humidity:%d %\n", temperature, humidity);
 
      Read_Dht11();
      count_2 = 0;
    }
    //3秒发送一次
    if (count >= 3) {
      // 每隔3秒钟发布一次信息
      pubMQTTmsg();
      count = 0;
    }
  }

  // 处理信息以及心跳
  mqttClient.loop();

}

//计时器
void tickerCount() {
  count++;
  count_2++;
}
// 连接MQTT服务器并订阅信息
void connectMQTTserver() {
  // 根据ESP8266的MAC地址生成客户端ID（避免与其它ESP8266的客户端ID重名）
   
    subscribeTopic();  // 订阅指定主题

  } else {
    Serial.print("MQTT Server Connect Failed. Client State:");
    Serial.println(mqttClient.state());
    delay(5000);
  }
}

// 收到信息后的回调函数
void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message Received [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  //解析数据
  massage_parse_json((char*)payload);
  //回传数据
  pubMQTTmsg();
}

// 订阅指定主题
void subscribeTopic() {
  // 通过串口监视器输出是否成功订阅主题以及订阅的主题名称
  // 请注意subscribe函数第二个参数数字为QoS级别。这里为QoS = 1
  if (mqttClient.subscribe(subTopic, subQoS)) {
    Serial.println(subTopic);
  } else {
    Serial.print("Subscribe Fail...");
    connectMQTTserver();  // 则尝试连接服务器
  }
}

// 发布信息
void pubMQTTmsg() {
  char pubMessage[254];
  // 数据流
  MQTT_FillBuf(pubMessage);
 
}

// ESP8266连接wifi
void connectWifi() {

  WiFi.begin(ssid, password);

  //等待WiFi连接,成功连接后输出成功信息
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.println("");
}
// 读取DHT11数值
void Read_Dht11() {

  if (dht.getData()) {                   // get All data from DHT11
    temperature = dht.getTemperature();  // return temperature in celsius
    humidity = dht.getHumidity();        // return humidity
  }
}
// 数据封装
unsigned char MQTT_FillBuf(char* buf) {

  char text[256];
  memset(text, 0, sizeof(text));

  strcpy(buf, "{");
 


  memset(text, 0, sizeof(text));
  sprintf(text, "}");
  strcat(buf, text);

  return strlen(buf);
}

// 解析json数据
void massage_parse_json(char* message) {
  // 声明一个 JSON 文档对象
  StaticJsonDocument<200> doc;

  // 解析 JSON 数据
  DeserializationError error = deserializeJson(doc, (const char*)message);

  // 检查解析是否成功
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }
  // 从解析后的 JSON 文档中获取键值对
  int cmd = doc["cmd"];
  JsonObject data = doc["data"];
  switch (cmd) {
    case 1:
      humidity_V = data["humi_v"];
      temperature_V = data["temp_v"];
      break;
    case 2:
      // 实现LED闪烁
      led_1_state = data["led"];
      digitalWrite(LED_1, led_1_state);  // 则点亮LED。
      break;
  }
}
