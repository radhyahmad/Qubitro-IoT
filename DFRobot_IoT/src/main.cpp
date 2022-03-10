#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"
#include "SparkFunCCS811.h"
#include "ArduinoJson.h"
#include "QubitroMqttClient.h"
#include "configuration.h"

static void initialize_sensor();
static void wifi_init();
static void qubitro_init();
static void publish_data();

WiFiClient wifiClient;
QubitroMqttClient mqttClient(wifiClient);

Adafruit_BME680 bme680;

#define CSS811_ADDR 0x5A
CCS811 ccs811(CSS811_ADDR);

uint32_t u32_previous_millis = 0;
#define INTERVAL (uint32_t)15000

void setup()
{
  Serial.begin(9600);
  initialize_sensor();
  wifi_init();
  qubitro_init();
}       
 
void loop() 
{
  mqttClient.poll();
  uint32_t u32_now = millis();
  if (u32_now - u32_previous_millis >= INTERVAL)
  {
    u32_previous_millis = u32_now;
    publish_data();
  }
  
}

static void initialize_sensor()
{
  if (!bme680.begin())
   {
     Serial.println("Could not find a valid BME680!");
     while(1);
   }

  bme680.setTemperatureOversampling(BME680_OS_8X);
  bme680.setHumidityOversampling(BME680_OS_2X);
  
  if (ccs811.begin() == false)
  {
    Serial.print("CCS811 Error");
    while(1);
  }
}
 
static void wifi_init()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(10);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.print("Connecting to WiFi");
  
  while (true)
  {
    delay(1000);
    Serial.print(".");
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("");
      Serial.println("WiFi Connected.");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("RSSI: ");
      Serial.println(WiFi.RSSI());
      break;
    } 
  }
}

static void qubitro_init()
{
  mqttClient.setId(DEVICE_ID);
  mqttClient.setDeviceIdToken(DEVICE_ID, DEVICE_TOKEN);
  Serial.println("Connecting to Qubitro...");

  if (!mqttClient.connect(SECRET_BROKER, PORT))
  {
    Serial.print("Connection failed. Error code");
    Serial.println(mqttClient.connectError());
    Serial.println("Visit docs.qubitro.com");
  }

  Serial.println("Connected to Qubitro.");
  mqttClient.subscribe(DEVICE_ID);
  
}

static void publish_data()
{
   if (!bme680.performReading()) 
   {
     Serial.println("Failed to perform reading");
     return;
   }

   uint32_t endTime = bme680.beginReading();

   if (endTime == (uint32_t)0)
   {
     Serial.println("Failed to begin reading.");
     return;
   }

   if (!bme680.endReading())
   {
     Serial.println("Failed to complete reading.");
     return;
   }

   if (ccs811.dataAvailable())
   {
     ccs811.readAlgorithmResults();
   } 

    static char payload[256];
    StaticJsonDocument<256>doc;

    doc["temperature"] = bme680.temperature;
    doc["humidity"] = bme680.humidity;
    doc["eco2"] = ccs811.getCO2();
    doc["tvoc"] = ccs811.getTVOC();
  
    serializeJsonPretty(doc, payload);
    mqttClient.beginMessage(DEVICE_ID);
    mqttClient.print(payload);
    mqttClient.endMessage();
}