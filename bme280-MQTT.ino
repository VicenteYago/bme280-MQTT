

//https://randomnerdtutorials.com/esp8266-nodemcu-mqtt-publish-bme280-arduino/
//https://arduinojson.org/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <myConfig.h>

#define MQTT_HOST IPAddress(192, 168, 12, 103)
#define MQTT_PORT 1883

#define SEALEVELPRESSURE_HPA (1013.25)

// MQTT Topics
#define TOPIC_BME280_A "BME280_A/values"

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
Adafruit_BME280 bme; // I2C

// Variables to hold sensor readings
float temp;
float hum;
float pres;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 1000*60*1;   // Interval at which to publish sensor readings

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void bmetoJson(char *buffer, int epochtime, float temp, float hum, float pres){
  StaticJsonDocument<170> doc;
  doc["sensor"] = "esp8266_A/bme280";
  doc["timestamp"] = epochtime; 
  JsonArray data = doc.createNestedArray("data"); 
  JsonObject data_0 = data.createNestedObject();
  data_0["temperature"] = temp;
  data_0["humidity"] = hum;
  data_0["pressure"] = pres;
  serializeJson(doc, buffer,170);
}


void setup() {
  Serial.begin(9600);
  Serial.println();
  
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  connectToWifi();

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // If your broker requires authentication (username and password), set them below
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);

  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

}

void loop() {
  
  timeClient.update();
  unsigned long currentMillis = millis();
  unsigned long epochTime = timeClient.getEpochTime();
  
  if (currentMillis - previousMillis >= interval) {
    // Save the last time a new reading was published
    previousMillis = currentMillis;
    // New BME280 sensor readings
    temp = bme.readTemperature();
    hum  = bme.readHumidity();
    pres = bme.readPressure()/100.0F;
    
    char buffer[170];
    bmetoJson(buffer,epochTime, temp, hum, pres);
    uint16_t packetIdPub1 = mqttClient.publish(TOPIC_BME280_A, 1, true, buffer); 
                               
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", TOPIC_BME280_A, packetIdPub1);
    Serial.printf("Message: %s \n", buffer);
  }
}
