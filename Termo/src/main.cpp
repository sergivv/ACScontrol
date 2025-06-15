#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "config.h"

// Configuración OLED
Adafruit_SSD1306 display(128, 32, &Wire);

// Sensor DS18B20
OneWire oneWire(4);
DallasTemperature sensors(&oneWire);

// Cliente MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Variables globales
float temperatura = 0.0;
float temp_min = 45.0, temp_max = 65.0;
float lastPublishedTemp = NAN;
String estacion = "invierno";
unsigned long lastMeasurementTime = 0;
const unsigned long measurementInterval = 60000;

bool setupOLED()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("Error OLED");
    return false;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display();
  return true;
}

void mostrarMensaje(String msg)
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

bool connectWiFi()
{
  WiFi.begin(ssid, password);
  mostrarMensaje("Conectando WiFi...");

  for (int i = 0; i < 10; i++)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      mostrarMensaje("WiFi OK\r\nIP: " + WiFi.localIP().toString());

      // Configurar topics MQTT
      String mac = WiFi.macAddress();
      // mac.replace(":", "");
      mqtt_topic = String(MQTT_TOPIC_BASE) + "/" + mac + "/Temperatura";
      topic_request = "ACS_Control/" + mac + "/ConfigRequest";
      topic_response = "ACS_Control/" + mac + "/ConfigResponse";
      topic_update = "ACS_Control/" + mac + "/ConfigUpdate";

      return true;
    }
    delay(1000);
  }
  return false;
}

void callbackMQTT(char *topic, byte *payload, unsigned int length)
{
  String topicStr(topic);
  String message;

  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  if (topicStr == topic_response or topicStr == topic_update)
  {
    LOG_INFO("Mensaje recibido en topic: " + topicStr);
    LOG_INFO("Mensaje: " + message);
  }
  {
    JsonDocument doc;
    deserializeJson(doc, message);

    temp_min = doc["temp_min"] | temp_min;
    temp_max = doc["temp_max"] | temp_max;
    estacion = doc["estacion"] | estacion;

    LOG_INFO("Config actualizada:");
    LOG_INFO("Min:" + String(temp_min) + " Max:" + String(temp_max) + " Estacion: " + estacion);
  }
}

void reconnectMQTT()
{
  if (!client.connected())
  {
    String clientId = "ESP32-" + WiFi.macAddress();
    if (client.connect(clientId.c_str()))
    {
      client.subscribe(topic_response.c_str());
      client.subscribe(topic_update.c_str());
    }
  }
}

void publishTemperature()
{
  // Redondear a 1 decimal para evitar fluctuaciones mínimas
  float temperaturaRedondeada = round(temperatura * 10) / 10.0;

  // Verificar si la temperatura ha cambiado significativamente
  if (isnan(lastPublishedTemp) || abs(temperaturaRedondeada - lastPublishedTemp) >= 0.2)
  {
    // Crear documento JSON
    JsonDocument doc;
    doc["mac"] = WiFi.macAddress();
    doc["temperatura"] = temperaturaRedondeada;

    // Serializar y publicar
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);

    if (client.publish(mqtt_topic.c_str(), jsonBuffer))
    {
      Serial.printf("[MQTT] Temp publicada: %.1f°C (Anterior: %.1f°C)\r\n", temperaturaRedondeada, lastPublishedTemp);
      lastPublishedTemp = temperaturaRedondeada; // Actualizar último valor publicado
    }
    else
    {
      Serial.println("[ERROR] Fallo al publicar temperatura");
    }
  }
  else
  {
    Serial.printf("[DEBUG] Temp no cambió: %.1f°C\r\n", temperaturaRedondeada);
  }
}

void setup()
{
  Serial.begin(115200);

  if (!setupOLED())
    ESP.restart();
  if (!connectWiFi())
    ESP.restart();

  sensors.begin();

  client.setServer(servidor_mqtt, puerto_mqtt);
  client.setCallback(callbackMQTT);
  reconnectMQTT();

  // Solicitar configuración inicial
  client.publish(topic_request.c_str(), "1");
}

void loop()
{
  client.loop();

  if (millis() - lastMeasurementTime >= measurementInterval)
  {
    lastMeasurementTime = millis();

    sensors.requestTemperatures();
    temperatura = sensors.getTempCByIndex(0);

    // Mostrar en OLED
    display.clearDisplay();
    display.setTextSize(4);
    display.setCursor(0, 0);
    display.print(temperatura, 1);
    display.println("C");
    display.display();

    publishTemperature();
  }
}