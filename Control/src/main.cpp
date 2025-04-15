#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Configuración SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Configuración del sensor DHT22
#define DHTPIN 4          // Pin digital 4
#define DHTTYPE DHT22     // Define el tipo de sensor DHT
DHT dht(DHTPIN, DHTTYPE); // Crea el objeto DHT

// Variables
float temperatura = 0.0;
float humedad = 0.0;
float lastPublishedTemperature = -1000.0;

unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 300000; // Verificar wifi cada 5 min

unsigned long lastMeasurementTime = 0;
const unsigned long measurementInterval = 60000; // 1 minuto

// Conexión del cliente MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Configuración OLED (sin cambios)
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

// Función de configuración para el DHT22
void setupDHT()
{
  dht.begin(); // Inicia el sensor DHT
  Serial.println("Sensor DHT22 inicializado.");
  // Pequeña espera inicial para estabilizar el sensor DHT
  delay(2000);
}

// Conexión wifi
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

// Conexión al servidor MQTT
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
  // Redondear temperatura y humedad a 1 decimal
  float tempRedondeada = round(temperatura * 10) / 10.0;
  float humRedondeada = round(humedad * 10) / 10.0;

  // Comparar con la última temperatura publicada
  if (tempRedondeada == lastPublishedTemperature && lastPublishedTemperature != -1000.0) // Evitar no publicar la primera vez
  {
    Serial.println("Temperatura sin cambios. No se publica.");
    return; // Salir si la temperatura no cambió
  }

  // Actualizar la última temperatura publicada
  lastPublishedTemperature = tempRedondeada;

  // Crear documento JSON
  JsonDocument doc;
  doc["mac"] = WiFi.macAddress();      // Dirección MAC
  doc["temperatura"] = tempRedondeada; // Temperatura con 1 decimal
  doc["humedad"] = humRedondeada;      // Humedad con 1 decimal

  // Serializar JSON a una cadena
  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);

  Serial.print("Preparando para publicar JSON: ");
  Serial.println(jsonBuffer);

  // Publicar el JSON
  if (client.publish(mqtt_topic.c_str(), jsonBuffer))
  {
    Serial.print("JSON publicado en ");
    Serial.print(mqtt_topic);
    Serial.print(": ");
    Serial.println(jsonBuffer);
  }
  else
  {
    Serial.println("Error al publicar JSON");
  }
}

// Comprueba el estado de la conexión WiFi y reconecta si es necesario
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

// Muestra temperatura y humedad en pantalla
void updateDisplay(float temperatura, float humedad, int caldera, int bomba)
{
  display.fillRect(0, 0, SCREEN_WIDTH, 32, SSD1306_BLACK);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  display.print("T: ");
  display.print(temperatura, 1); // Mostrar 1 decimal
  display.println(" C");
  display.print("H: ");
  display.print(humedad, 1); // Mostrar 1 decimal
  display.println(" %");
  display.setCursor(15, 45);
  display.print("C:");

  display.drawCircle(45, 51, 7, SSD1306_WHITE);
  display.drawCircle(95, 51, 7, SSD1306_WHITE);

  caldera == 0 ? display.drawCircle(45, 51, 4, SSD1306_WHITE) : display.fillCircle(45, 51, 4, SSD1306_WHITE);

  display.setCursor(65, 45);
  display.print("B:");
  bomba == 0 ? display.drawCircle(95, 51, 4, SSD1306_WHITE) : display.fillCircle(95, 51, 4, SSD1306_WHITE);

  display.display();
}

void setup()
{
  Serial.begin(115200);

  setupOLED();                                  // Configura OLED
  setupDHT();                                   // DHT Cambio: Llama a la nueva función de setup del DHT
  connectWiFi();                                // Conecta al WiFi
  client.setServer(servidor_mqtt, puerto_mqtt); // Configura MQTT
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
  client.loop(); // Necesario para mantener la conexión MQTT y procesar mensajes entrantes

  // Medir y publicar temperatura cada 'measurementInterval'
  unsigned long currentTime = millis();
  if (currentTime - lastMeasurementTime >= measurementInterval)
  {
    lastMeasurementTime = currentTime;

    // Leer temperatura y humedad del DHT22
    humedad = dht.readHumidity();        // Lee la humedad primero
    temperatura = dht.readTemperature(); // Lee la temperatura

    // Comprobar si las lecturas son válidas (no NaN - Not a Number)
    if (isnan(temperatura) || isnan(humedad))
    {
      Serial.println("Error al leer del sensor DHT!");
      lastPublishedTemperature = -1000.0;
    }
    else
    {
      Serial.print("Humedad: ");
      Serial.print(humedad);
      Serial.print(" %\t");
      Serial.print("Temperatura: ");
      Serial.print(temperatura);
      Serial.println(" *C");

      // Actualizar pantalla (pasando 1 decimal)
      updateDisplay(round(temperatura * 10) / 10.0, round(humedad * 10) / 10.0, 0, 1); // Asumiendo caldera=0, bomba=1 como en tu código original

      // Publicar por MQTT si la temperatura ha cambiado
      publishTemperature();
    }
  }
}