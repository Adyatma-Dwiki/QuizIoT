#include <Arduino.h>
#include <Ticker.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "BH1750.h"
#include "DHTesp.h"

#define DHTPIN 19
#define DHTTYPE DHTesp::DHT11
#define G_PIN 25
#define Y_PIN 26
#define R_PIN 27
#define TEST_PIN 14
#define MQTT_BROKER "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH "esp32/data"
#define MQTT_TOPIC_PUBLISH1 "esp32/humidity"
#define MQTT_TOPIC_PUBLISH2 "esp32/temperature"
#define MQTT_TOPIC_PUBLISH3 "esp32/light"
#define MQTT_TOPIC_SUBSCRIBE "esp32/cmd"

const char* LED3 = "null";

unsigned long lastTempPublish = 0;
unsigned long lastHumidPublish = 0;
unsigned long lastLuxPublish = 0;
const int Temp_Publish_interval = 7000;
const int Humid_Publish_interval = 5000;
const int Lux_Publish_interval = 4000;

const char *ssid = "aneh";
const char *password = "12345678";
char g_szDeviceId[30];
void onPublishMessage();
void WifiConnect();
boolean mqttConnect();
// ------------------------ MQTT ------------------------
WiFiClient espClient;
PubSubClient mqtt(espClient);

Ticker timerPublish, ledOff;
int nMsgCount = 0;
DHTesp dht;
BH1750 lightMeter;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  WifiConnect();
  mqttConnect();
  timerPublish.attach_ms(3000, onPublishMessage);
  pinMode(G_PIN, OUTPUT);
  pinMode(Y_PIN, OUTPUT);
  pinMode(R_PIN, OUTPUT);
  // SENSOR ------------------------
  dht.setup(DHTPIN, DHTTYPE);
  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);

  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
  // lightMeter.setMTreg(69);  // not needed, only mentioning it

  // Serial.println(F("BH1750 Test begin"));
  //
}

void loop()
{
  // put your main code here, to run repeatedly:
  delay(5000);
  mqtt.loop();
}
bool isLEDOn = false;

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();
  if (strcmp(topic, MQTT_TOPIC_SUBSCRIBE) == 0)
  {
    payload[len] = '\0';
    Serial.printf("LED3: %s\n", payload);
    LED3 = (char *)payload;
    if (strcmp(LED3, "ON") == 0)
    {
      // Ubah status LED menjadi menyala
      isLEDOn = true;
       digitalWrite(Y_PIN, HIGH);
    }
    else if (strcmp(LED3, "OFF") == 0)
    {
      // Ubah status LED menjadi mati
      isLEDOn = false;
      digitalWrite(Y_PIN, LOW);
    }
  }
}

void WifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}

boolean mqttConnect()
{
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(mqttCallback);
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);
  // Connect to MQTT Broker
  // Or, if you want to authenticate MQTT:
  boolean status = mqtt.connect(g_szDeviceId);
  if (status == false)
  {
    Serial.print(" fail, rc=");
    Serial.print(mqtt.state());
    return false;
  }
  Serial.println(" success");
  mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
  Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
  onPublishMessage();
  return mqtt.connected();
}
void onPublishMessage()
{
  delay(5000);
  char szTopic[50];
  char szData[10];
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  float lux = lightMeter.readLightLevel();
  if (dht.getStatus() == DHTesp::ERROR_NONE)
  {
  //  Serial.printf("Temperature: %.2f C, Humidity: %.2f %%, light: %.2f\n",
    //              temperature, humidity, lux);
    if (millis() - lastTempPublish >= 7000) { // publish temperature data every 7 seconds
      lastTempPublish = millis();
      sprintf(szTopic, "%s/temperature", MQTT_TOPIC_PUBLISH);
      sprintf(szData, "%.2f", temperature);
      mqtt.publish(szTopic, szData);
      Serial.printf("Publish message Temperature: %s\n", szData);
    }
    
    if (millis() - lastHumidPublish >= 5000) { // publish humidity data every 5 seconds
      lastHumidPublish = millis();
      sprintf(szTopic, "%s/humidity", MQTT_TOPIC_PUBLISH);
      sprintf(szData, "%.2f", humidity);
      mqtt.publish(szTopic, szData);
      Serial.printf("Publish message Humidity: %s\n", szData);
    }
  }
  
  if (millis() - lastLuxPublish >= 4000) { // publish lux data every 4 seconds
    lastLuxPublish = millis();
    sprintf(szTopic, "%s/light", MQTT_TOPIC_PUBLISH);
    sprintf(szData, "%.2f", lux);
    mqtt.publish(szTopic, szData);
    Serial.printf("Publish message Light: %s\n", szData);
  }
  
  // Check lux level and set MTreg accordingly
  if (lux < 0) {
    Serial.println(F("Error condition detected"));
  } else {
    if (lux >= 400) {
      // reduce measurement time - needed in direct sun light
      digitalWrite(R_PIN, HIGH);
      digitalWrite(G_PIN, LOW);
      if (lightMeter.setMTreg(32)) {
        Serial.println(F("The Door is Open"));
      } else {
        Serial.println(F("Error, Sensor is not detected"));
      }
    } else {
      if (lux < 400) {
        // typical light environment
        digitalWrite(R_PIN, LOW);
        digitalWrite(G_PIN, HIGH);
        if (lightMeter.setMTreg(69)) {
          Serial.println(F("The Door Is Close"));
        } else {
          Serial.println(F("Error, Sensor is not detected"));
        }
      } 
    }
  }
}
