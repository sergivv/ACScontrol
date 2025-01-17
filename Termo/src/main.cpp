#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "config.h"

// Configuración SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Pin del DS18B20
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Conexión del cliente MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Variables de medición
float temperatura = 0.0;
float lastPublishedTemperature = -1000.0;

unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 300000; // Verificar wifi cada 5 min

unsigned long lastMeasurementTime = 0;
const unsigned long measurementInterval = 60000;

void setupOLED()
{
  // Inicializa la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("Error al inicializar OLED");
    while (true)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void connectWiFi()
{
  if (!WiFi.config(local_IP, gateway, subnet))
  {
    Serial.println("Fallo en la configuración de IP estática. Usando DHCP...");
  }

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) // 30s timeout
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    display.clearDisplay();
    display.println("WiFi conectado!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();

    // Generamos el topic a partir de la mac
    String macAddress = WiFi.macAddress();                     // Obtiene la MAC
    mqtt_topic = "ACS_Control/" + macAddress + "/Temperatura"; // Crea el topic
    Serial.print("Topic MQTT: ");
    Serial.println(mqtt_topic);
  }
  else
  {
    Serial.println("No se pudo conectar a Wi-Fi. Reiniciando...");
    ESP.restart();
  }
}

void checkWiFiConnection()
{
  if (millis() - lastWiFiCheck >= wifiCheckInterval)
  {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi desconectado. Intentando reconectar...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}

void connectToMQTT()
{
  String clienteGenerado = "ESP-" + WiFi.macAddress().substring(12);
  char cliente[10];
  clienteGenerado.toCharArray(cliente, 10);
  static unsigned long lastAttemptTime = 0;
  if (millis() - lastAttemptTime > 5000)
  { // Reintentar cada 5 segundos
    lastAttemptTime = millis();
    if (client.connect(cliente))
    {
      Serial.println("Conectado al broker MQTT!");
    }
    else
    {
      Serial.print("Error MQTT: ");
      Serial.println(client.state());
    }
  }
}

// Publicar JSON con temperatura y MAC
void publishTemperature()
{
  // Redondear temperatura a 1 decimal
  float tempRedondeada = round(temperatura * 10) / 10.0;

  // Comparar con la última temperatura publicada
  if (tempRedondeada == lastPublishedTemperature) {
    Serial.println("Temperatura sin cambios. No se publica.");
    return; // Salir si la temperatura no cambió
  }

  // Actualizar la última temperatura publicada
  lastPublishedTemperature = tempRedondeada;

  // Crear documento JSON
  JsonDocument doc;
  doc["mac"] = WiFi.macAddress();      // Dirección MAC
  doc["temperatura"] = tempRedondeada; // Temperatura con 1 decimal

  // Serializar JSON a una cadena
  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);

  // Publicar el JSON
  if (client.publish(mqtt_topic.c_str(), jsonBuffer))
  {
    Serial.print("JSON publicado: ");
    Serial.println(jsonBuffer);
  }
  else
  {
    Serial.println("Error al publicar JSON");
  }
}

void setup()
{
  Serial.begin(115200);

  setupOLED();
  connectWiFi();
  sensors.begin();
  client.setServer(servidor_mqtt, puerto_mqtt);
}

void loop()
{
  // Verificar y reconectar Wi-Fi
  checkWiFiConnection();

  // Reconecta MQTT si es necesario
  if (!client.connected())
  {
    connectToMQTT();
  }
  client.loop();

  // Medir y publicar temperatura cada 1 minuto
  unsigned long currentTime = millis();
  if (currentTime - lastMeasurementTime >= measurementInterval)
  {
    lastMeasurementTime = currentTime;

    sensors.requestTemperatures();
    temperatura = sensors.getTempCByIndex(0); // Obtiene la temperatura en grados Celsius
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1); // Temperatura con un decimal
    Serial.println(" ºC");

    // Muestra la temperatura en la pantalla
    display.clearDisplay();
    display.setTextSize(4);
    display.setCursor(0, 0);
    display.print(temperatura, 1);
    display.println("C");
    display.display();

    // Publicar en MQTT y desconecta el cliente.
    publishTemperature();
  }
}
