#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT31.h>
#include "config.h"

// Configuración SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Inicialización SHT3x
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Variables
float temperatura = 0.0;
float humedad = 0.0;

unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 300000; // Verificar wifi cada 5 min

// Configuración OLED
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

void setupSHT3x()
{
  // Iniciar comunicación I2C
  if (!sht31.begin(0x44))
  { // 0x44 es la dirección I2C predeterminada del SHT30
    Serial.println("¡No se encontró el sensor SHT30!");
    while (1)
      ; // Detener el programa
  }
  Serial.println("Sensor SHT30 inicializado.");
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
  }
  else
  {
    Serial.println("No se pudo conectar a Wi-Fi. Reiniciando...");
    ESP.restart();
  }
}

// Comprueba el estado de la conexión WiFi
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

void setup()
{
  Serial.begin(115200);

  setupOLED();
  connectWiFi();
  setupSHT3x();
}

void loop()
{
  // Verificar y reconectar Wi-Fi
  checkWiFiConnection();

  temperatura = sht31.readTemperature();
  humedad = sht31.readHumidity();
  Serial.println(temperatura);
  Serial.println(humedad);

  if (!isnan(temperatura) && !isnan(humedad))
  {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Temperatura: ");
    display.print(temperatura);
    display.setCursor(0, 15);
    display.print("Humedad: ");
    display.print(humedad);
    display.display();
  }

  delay(10000);
}
