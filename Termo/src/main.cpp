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
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("[ERROR] OLED no detectado. Verifica conexiones.");
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.println("Error OLED");
    display.display();
    while (true)
      ; // Bloquea la ejecución
  }
  display.clearDisplay();
}

void connectWiFi()
{
  int intentos = 0;
  const int maxIntentos = 5;

  display.clearDisplay();
  display.println("Conectando WiFi...");
  display.display();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && intentos < maxIntentos)
  {
    delay(2000);
    intentos++;
    Serial.printf("[WARN] Intento WiFi %d/%d\n", intentos, maxIntentos);

    display.clearDisplay();
    display.printf("WiFi %d/%d\n", intentos, maxIntentos);
    display.display();
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[ERROR] WiFi no conectado. Modo offline.");
    display.clearDisplay();
    display.println("WiFi Falló");
    display.println("Modo offline");
    display.display();
    return; // Continúa en modo offline sin reiniciar
  }

  Serial.println("[INFO] WiFi conectado. IP: " + WiFi.localIP().toString());
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
  if (client.connected())
    return;

  Serial.println("[INFO] Intentando conexión MQTT...");
  String clientId = "ESP32-" + WiFi.macAddress();

  if (client.connect(clientId.c_str()))
  {
    Serial.println("[INFO] MQTT conectado.");
    display.println("MQTT OK");
    display.display();
  }
  else
  {
    String error;
    switch (client.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      error = "Timeout";
      break;
    case MQTT_CONNECTION_LOST:
      error = "Conexión perdida";
      break;
    case MQTT_CONNECT_FAILED:
      error = "Falló";
      break;
    default:
      error = "Código: " + String(client.state());
    }
    Serial.printf("[ERROR] MQTT: %s\n", error.c_str());

    display.clearDisplay();
    display.println("MQTT Error:");
    display.println(error);
    display.display();
  }
}

// Publicar JSON con temperatura y MAC
void publishTemperature()
{
  if (temperatura <= -50.0 || temperatura >= 125.0)
  { // Rango válido DS18B20
    Serial.println("[ERROR] Temperatura fuera de rango: " + String(temperatura));
    display.println("Error Sensor");
    display.display();
    return;
  }

  float tempRedondeada = round(temperatura * 10) / 10.0;
  if (tempRedondeada == lastPublishedTemperature)
  {
    Serial.println("[INFO] Temperatura sin cambios. No se publica.");
    return;
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
  delay(1000); // Tiempo para inicializar Serial

  setupOLED();

  // Verificar sensor DS18B20
  sensors.begin();
  if (sensors.getDeviceCount() == 0)
  {
    Serial.println("[ERROR] Sensor DS18B20 no encontrado.");
    display.println("Error: Sin sensor");
    display.display();
    while (true)
      ; // Bloquea la ejecución
  }
  Serial.println("[INFO] Sensor DS18B20 inicializado.");

  connectWiFi();
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
