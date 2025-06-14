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

bool setupOLED()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    LOG_ERROR("¡Error al iniciar OLED! Verifica:");
    LOG_WARN("1. Conexiones SDA/SCL");
    LOG_WARN("2. Dirección I2C (0x3C vs 0x3D)");
    return false;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display(); // ¡Importante! Esta línea aplica los cambios.
  LOG_INFO("OLED inicializado correctamente.");
  return true;
}

void mostrarMensaje(const String &mensaje)
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(mensaje);
  display.display();
  delay(3000);
}

bool setupDS18B20()
{
  sensors.begin();
  if (sensors.getDeviceCount() == 0)
  {
    Serial.println("Sensor no detectado");
    return false;
  }
  return true;
}

bool connectWiFi()
{
  int intentos = 0;
  const int maxIntentos = 5;

  mostrarMensaje("Conectando a WiFi...");
  LOG_INFO("Conectando a WiFi: " + String(ssid));

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && intentos < maxIntentos)
  {
    delay(2000);
    intentos++;
    Serial.printf("[WARN] Intento WiFi %d/%d\n", intentos, maxIntentos);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("WiFi %d/%d\n", intentos, maxIntentos);
    display.display();
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    LOG_ERROR("WiFi no conectado. Modo offline.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Falló");
    display.print("Modo offline");
    display.display();
    return false; // Continúa en modo offline sin reiniciar
  }

  LOG_INFO("WiFi conectado. IP: " + WiFi.localIP().toString());
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("IP: " + WiFi.localIP().toString());
  display.display();

  if (WiFi.status() == WL_CONNECTED)
  {
    // Genera el topic usando la MAC y el base
    String macAddress = WiFi.macAddress();
    macAddress.replace(":", ""); // Elimina ':' para simplificar
    mqtt_topic = String(MQTT_TOPIC_BASE) + "/" + macAddress + "/Temperatura";

    LOG_INFO("Topic MQTT generado: ");
    LOG_INFO(mqtt_topic);
  }
  return true;
}

void checkWiFiConnection()
{
  if (millis() - lastWiFiCheck >= wifiCheckInterval)
  {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
      LOG_ERROR("WiFi desconectado. Intentando reconectar...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}

void connectToMQTT()
{
  if (client.connected())
    return;

  LOG_INFO("Intentando conexión MQTT...");

  String clientId = "ESP32-" + WiFi.macAddress(); // ID único basado en MAC
  if (client.connect(clientId.c_str()))
  {
    LOG_INFO("¡Conectado al broker MQTT!");
    LOG_INFO("ClientID: ");
    LOG_INFO(clientId);
  }
  else
  {
    LOG_ERROR("Fallo en conexión MQTT. Código de error:");
    LOG_ERROR(client.state()); // Imprime el código de error

    // Descripción del error
    switch (client.state())
    {
    case -4:
      LOG_ERROR("MQTT_CONNECTION_TIMEOUT");
      break;
    case -3:
      LOG_ERROR("MQTT_CONNECTION_LOST");
      break;
    case -2:
      LOG_ERROR("MQTT_CONNECT_FAILED");
      break;
    case -1:
      LOG_ERROR("MQTT_DISCONNECTED");
      break;
    case 0:
      LOG_ERROR("MQTT_CONNECTED");
      break;
    case 1:
      LOG_ERROR("MQTT_CONNECT_BAD_PROTOCOL");
      break;
    case 2:
      LOG_ERROR("MQTT_CONNECT_BAD_CLIENT_ID");
      break;
    case 3:
      LOG_ERROR("MQTT_CONNECT_UNAVAILABLE");
      break;
    case 4:
      LOG_ERROR("MQTT_CONNECT_BAD_CREDENTIALS");
      break;
    case 5:
      LOG_ERROR("MQTT_CONNECT_UNAUTHORIZED");
      break;
    default:
      LOG_ERROR("Error desconocido");
    }
  }
}

// Publicar JSON con temperatura y MAC
void publishTemperature()
{
  if (temperatura <= -50.0 || temperatura >= 125.0)
  { // Rango válido DS18B20
    LOG_ERROR("Temperatura fuera de rango: " + String(temperatura));
    mostrarMensaje("Error: Temp fuera de rango");
    delay(1000);
    return;
  }

  float tempRedondeada = round(temperatura * 10) / 10.0;
  if (tempRedondeada == lastPublishedTemperature)
  {
    LOG_INFO("Temperatura sin cambios. No se publica.");
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
    LOG_ERROR("Error al publicar JSON");
  }
}

void reinicioControlado(const String &motivo, unsigned int tiempoEspera = 3000)
{
  LOG_ERROR("Reiniciando ESP32. Motivo: " + motivo);

  // Mostrar motivo en OLED (si está disponible)
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Reiniciando:");
  display.println(motivo);
  display.display();

  delay(tiempoEspera); // Tiempo para que el usuario lea el mensaje
  ESP.restart();       // Reinicio físico
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // Tiempo para inicializar Serial

  if (!setupOLED())
  {
    reinicioControlado("Falló OLED");
  }
  mostrarMensaje("Iniciando...");

  if (!setupDS18B20())
  { // Verifica conexión del sensor
    reinicioControlado("Fallo sensor");
  }
  mostrarMensaje("Sensor DS18B20 OK");

  if (!connectWiFi())
  {
    reinicioControlado("Falló WiFi");
  }
  mostrarMensaje("WiFi conectado");

  client.setServer(servidor_mqtt, puerto_mqtt);
  delay(500);
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
    if (temperatura == -127.0)
    {
      LOG_ERROR("Error al leer el sensor DS18B20. Reinicio controlado.");
      reinicioControlado("Error en sensor DS18B20");
    }
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
