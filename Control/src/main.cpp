#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// --- Constantes de Configuración ---
// Pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Sensor DHT
#define DHTPIN 4
#define DHTTYPE DHT22
const unsigned long SENSOR_READ_INTERVAL = 60000; // Intervalo de lectura del sensor (1 min)

// WiFi
const unsigned long WIFI_CHECK_INTERVAL = 300000; // Intervalo chequeo WiFi (5 min)
const unsigned long WIFI_CONNECT_TIMEOUT = 30000; // Timeout conexión WiFi (30s)

// MQTT
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // Intervalo reintento conexión MQTT (5s)

// Estados
const int CALDERA_ESTADO_ACTUAL = 0;
const int BOMBA_ESTADO_ACTUAL = 1;

// --- Objetos Globales ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// --- Variables Globales ---
float temperatura = 0.0;
float humedad = 0.0;
float lastPublishedTemperature = -1000.0;
String mqtt_topic = "";
unsigned long lastMeasurementTime = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastMQTTAttemptTime = 0;

// --- Funciones de Setup ---

void setupOLED()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println(F("Error al inicializar OLED"));
    while (true)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  Serial.println(F("OLED inicializado."));
}

void setupDHT()
{
  dht.begin();
  Serial.println(F("Sensor DHT22 inicializado."));
  delay(2000);
}

// --- Funciones de Conexión ---

void connectWiFi()
{
  Serial.print(F("Configurando IP estática... "));
  if (!WiFi.config(local_IP, gateway, subnet))
  {
    Serial.println(F("Fallo. Usando DHCP..."));
    // No es necesario hacer nada más, WiFi.begin() usará DHCP si config falla
  }
  else
  {
    Serial.println(F("OK."));
  }

  Serial.print(F("Conectando a WiFi: "));
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_CONNECT_TIMEOUT)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("WiFi conectado!"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.println(F("WiFi conectado!"));
    display.print(F("IP: "));
    display.println(WiFi.localIP());
    display.display();

    // Generamos el topic MQTT a partir de la MAC
    String macAddress = WiFi.macAddress();
    macAddress.replace(":", "");
    mqtt_topic = "ACS_Control/" + macAddress + "/Temperatura";
    Serial.print(F("Topic MQTT: "));
    Serial.println(mqtt_topic);
  }
  else
  {
    Serial.println(F("No se pudo conectar a Wi-Fi. Reiniciando..."));
    delay(1000);
    ESP.restart();
  }
}

void connectToMQTT()
{
  Serial.print(F("Intentando conectar al Broker MQTT: "));
  Serial.print(servidor_mqtt);
  String clienteID = "ESP-" + WiFi.macAddress();
  clienteID.replace(":", "");

  if (client.connect(clienteID.c_str()))
  {
    Serial.println(F(" Conectado!"));
  }
  else
  {
    Serial.print(F(" Fallo, rc="));
    Serial.print(client.state());
    Serial.println(F(". Reintentando en 5 segundos..."));
  }
}

// --- Funciones de Lógica Principal ---

/**
 * @brief Lee los datos del sensor DHT22.
 * @param temp Referencia a la variable donde guardar la temperatura.
 * @param hum Referencia a la variable donde guardar la humedad.
 * @return true si la lectura fue exitosa, false en caso de error.
 */
bool readSensorData(float &temp, float &hum)
{
  hum = dht.readHumidity();
  temp = dht.readTemperature();

  if (isnan(temp) || isnan(hum))
  {
    Serial.println(F("Error al leer del sensor DHT!"));
    return false;
  }
  else
  {
    return true;
  }
}

/**
 * @brief Publica los datos de temperatura y humedad por MQTT en formato JSON.
 * @param temp Temperatura a publicar.
 * @param hum Humedad a publicar.
 */
void publishSensorData(float temp, float hum)
{
  float tempRedondeada = round(temp * 10) / 10.0;
  float humRedondeada = round(hum * 10) / 10.0;

  // Comparar con la última temperatura publicada (solo publicar si cambia)
  if (tempRedondeada == lastPublishedTemperature && lastPublishedTemperature != -1000.0)
  {
    return;
  }
  lastPublishedTemperature = tempRedondeada; // Actualizar la última publicada

  JsonDocument doc; // Usar JsonDocument (tamaño dinámico en ESP32)
  doc["mac"] = WiFi.macAddress();
  doc["temperatura"] = tempRedondeada;
  doc["humedad"] = humRedondeada;

  char jsonBuffer[128]; // Ajustar tamaño si es necesario
  size_t n = serializeJson(doc, jsonBuffer);

  Serial.print(F("Publicando en MQTT: "));
  Serial.println(jsonBuffer);

  if (!client.publish(mqtt_topic.c_str(), jsonBuffer, n))
  { // Publicar con tamaño exacto
    Serial.println(F("Error al publicar JSON"));
  }
}

/**
 * @brief Actualiza la pantalla OLED con los datos.
 * @param temp Temperatura a mostrar.
 * @param hum Humedad a mostrar.
 * @param caldera Estado de la caldera.
 * @param bomba Estado de la bomba.
 */
void updateDisplay(float temp, float hum, int caldera, int bomba)
{
  display.clearDisplay(); // Limpiar toda la pantalla para evitar artefactos
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  display.print(F("T: "));
  display.print(temp, 1); // Mostrar 1 decimal
  display.print(F(" "));
  display.cp437(true); // Habilitar página de códigos 437 para símbolo de grado
  display.write(167);  // Código para el símbolo de grado '°'
  display.print(F("C"));

  display.setCursor(0, 18);
  display.print(F("H: "));
  display.print(hum, 1); // Mostrar 1 decimal
  display.println(F(" %"));

  // Indicadores Caldera/Bomba
  display.setTextSize(1); // Tamaño más pequeño para etiquetas
  display.setCursor(15, 45);
  display.print(F("C:"));
  display.drawCircle(45, 51, 7, SSD1306_WHITE); // Círculo exterior
  if (caldera != 0)
  {
    display.fillCircle(45, 51, 4, SSD1306_WHITE); // Relleno si está ON
  }
  else
  {
    display.drawCircle(45, 51, 4, SSD1306_WHITE); // Contorno si está OFF
  }

  display.setCursor(65, 45);
  display.print(F("B:"));
  display.drawCircle(95, 51, 7, SSD1306_WHITE); // Círculo exterior
  if (bomba != 0)
  {
    display.fillCircle(95, 51, 4, SSD1306_WHITE);
  }
  else
  {
    display.drawCircle(95, 51, 4, SSD1306_WHITE);
  }

  display.display();
}

// --- Funciones de Mantenimiento / Bucle Principal ---

/**
 * @brief Verifica la conexión WiFi y reconecta si es necesario.
 */
void handleWiFi()
{
  if (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println(F("WiFi desconectado. Intentando reconectar..."));
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("Reconectando WiFi.."));
      display.display();
      WiFi.disconnect();
      delay(100);
      WiFi.reconnect();
      lastWiFiCheck = millis();
    }
  }
}

/**
 * @brief Gestiona la conexión MQTT (conecta si está desconectado y ejecuta loop).
 */
void handleMQTT()
{
  if (!client.connected())
  {
    if (millis() - lastMQTTAttemptTime > MQTT_RECONNECT_INTERVAL)
    {
      lastMQTTAttemptTime = millis();
      connectToMQTT(); // Intentar conectar
    }
  }
  else
  {
    // Si está conectado, procesar mensajes entrantes/salientes y mantener conexión viva
    client.loop();
  }
}

// --- Setup ---
void setup()
{
  Serial.begin(115200);
  Serial.println(F("\n\nInicializando ESP32..."));

  setupOLED();
  setupDHT();
  connectWiFi(); // Conectar a WiFi y generar topic MQTT

  // Configurar servidor MQTT
  client.setServer(servidor_mqtt, puerto_mqtt);

  Serial.println(F("Setup completado."));
  lastMeasurementTime = millis(); // Iniciar timers
  lastWiFiCheck = millis();
  lastMQTTAttemptTime = millis() - MQTT_RECONNECT_INTERVAL; // Forzar intento MQTT inicial
}

// --- Loop ---
void loop()
{
  handleWiFi();
  handleMQTT();

  // Ejecutar tareas basadas en tiempo
  unsigned long currentTime = millis();

  // Tarea: Leer sensor y publicar datos
  if (currentTime - lastMeasurementTime >= SENSOR_READ_INTERVAL)
  {
    lastMeasurementTime = currentTime;

    float currentTemp, currentHum; // Variables locales para la lectura
    if (readSensorData(currentTemp, currentHum))
    {
      // Si la lectura es válida, actualizar variables globales
      temperatura = currentTemp;
      humedad = currentHum;

      // Actualizar pantalla
      updateDisplay(temperatura, humedad, CALDERA_ESTADO_ACTUAL, BOMBA_ESTADO_ACTUAL);

      // Intentar publicar por MQTT (solo si está conectado)
      if (client.connected())
      {
        publishSensorData(temperatura, humedad);
      }
      else
      {
        Serial.println(F("MQTT desconectado. No se publican datos."));
        // Si no se publicó, forzar publicación la próxima vez que conecte
        lastPublishedTemperature = -1000.0;
      }
    }
    else
    {
      // Error al leer el sensor, ya se mostró mensaje en readSensorData()
      // Opcional: Mostrar error en OLED
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Error DHT!");
      display.display();
      // Forzar publicación la próxima vez que haya lectura válida
      lastPublishedTemperature = -1000.0;
    }
  }

  delay(10);
}