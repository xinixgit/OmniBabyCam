#include <ESP8266WiFi.h>
#include <WifiClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "MqttHandler.h"
#include "CommunicationManager.h"
#include "SensorHandler.h"
#include "I2CAddressScanner.h"

#define TEN_MIN 600000

Config config;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
MqttHandler *mqttHandler;
TemperatureSensorCommunicationManager *communicationManager;
TemperatureSensor *sysTempSensor;
SensorHandler *sensorHandler;

// functions declaration
void connectToWifi();
void initSensors();
void onWifiConnect(const WiFiEventStationModeGotIP &);
void onWifiDisconnect(const WiFiEventStationModeDisconnected &);

void setup()
{
  Serial.begin(9600);

  mqttHandler = new MqttHandler(&config.mqtt_config);
  communicationManager = new TemperatureSensorCommunicationManager(mqttHandler);
  initSensors();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
}

void loop()
{
  connectToWifi();
  delay(500);

  mqttHandler->connect();
  delay(500);

  sensorHandler->publishAll();
  delay(500);

  mqttHandler->disconnect();

  WiFi.mode(WIFI_OFF);

  WiFi.forceSleepBegin();

  delay(TEN_MIN);

  WiFi.forceSleepWake();
  delay(500);
}

void connectToWifi()
{
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to Wi-Fi network ");
  WiFi.begin(config.wifi_ssid, config.wifi_password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  Serial.println("Connected to Wi-Fi.");
  Serial.println(WiFi.localIP());
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
  Serial.println("Disconnected from Wi-Fi.");
}

void initSensors()
{
  TemperatureSensorConfig tempSensorConfig = TemperatureSensorConfig([](String payload)
                                                                     { communicationManager->publishTemperature(payload); });
  tempSensorConfig.type = AHT21;
  TemperatureSensor *tempSensor = initTemperatureSensor(tempSensorConfig);
  sysTempSensor = tempSensor;

  AirQualitySensorConfig aqiSensorConfig = AirQualitySensorConfig([](String payload)
                                                                  { communicationManager->publishAQI(payload); });
  aqiSensorConfig.nominalTemperature = tempSensor->readTemperature();
  aqiSensorConfig.nominalHumidity = tempSensor->readHumidity();
  AirQualitySensor *aqiSensor = initAirQualitySensor(aqiSensorConfig);
  sensorHandler = new SensorHandler(std::list<Sensor *>{tempSensor, aqiSensor});
}