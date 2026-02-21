/*
 * Horno ISA — Firmware ESP32
 * Cliente HTTPS + FreeRTOS (Dual Core)
 * 
 * Núcleo 0: Comunicación con Vercel (no bloqueante)
 * Núcleo 1: Control térmico PI + Dimmer TRIAC
 * 
 * INSTRUCCIONES:
 * 1. Sube el proyecto web a Vercel
 * 2. Copia tu URL (ej: https://horno-isa.vercel.app)
 * 3. Pégala en la variable serverURL abajo
 * 4. Ajusta ssid y password de tu WiFi
 * 5. Compila y sube al ESP32 desde Arduino IDE / PlatformIO
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>

// --- Credenciales y Nube ---
const char* ssid = "TU_RED_WIFI";
const char* password = "TU_PASSWORD";
// ⚠️ Reemplaza con tu URL de Vercel
const String serverURL = "https://TU-PROYECTO.vercel.app/api/sync";

// --- Configuración OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Hardware (Pines) ---
const int MAX6675_SCK = 19;
const int MAX6675_CS = 5;
const int MAX6675_SO = 23;
MAX6675 tc(MAX6675_SCK, MAX6675_CS, MAX6675_SO);

const int ZC_PIN = 34;  // Zero-Cross detector
const int DIM_PIN = 18; // TRIAC gate

// --- Variables Globales ---
volatile int power = 0;
volatile bool zc_detected = false;

float currentTemp = 0.0;
float filteredTemp = 0.0;
float targetTemp = 0.0;
bool systemActive = false;
String systemStatus = "CONECTANDO";

// Variables PI
float Kp = 12.0;
float Ki = 0.5;
float errorIntegral = 0.0;
unsigned long lastControlTime = 0;

// Hilo de Red
TaskHandle_t TaskNetwork;

// --- Interrupción de Cruce por Cero ---
void IRAM_ATTR zc_sns() {
  zc_detected = true;
  digitalWrite(DIM_PIN, LOW);
}

// =========================================================
// TAREA NÚCLEO 0: Comunicación con Vercel (No bloqueante)
// =========================================================
void networkTask(void * pvParameters) {
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure();
  HTTPClient http;

  for(;;) {
    if(WiFi.status() == WL_CONNECTED) {
      http.begin(*client, serverURL);
      http.addHeader("Content-Type", "application/json");

      String json = "{\"currentTemp\":";
      json += String(filteredTemp, 1);
      json += ",\"power\":";
      json += String(power);
      json += ",\"status\":\"";
      json += systemStatus;
      json += "\"}";

      int httpResponseCode = http.POST(json);

      if (httpResponseCode == 200) {
        String response = http.getString();
        int idx = response.indexOf("targetTemp\":");
        if(idx > 0) {
          float newTarget = response.substring(idx + 12, response.indexOf("}")).toFloat();
          if (newTarget != targetTemp) {
            targetTemp = newTarget;
            errorIntegral = 0.0;
            if (targetTemp > 10.0) systemActive = true;
            else systemActive = false;
          }
        }
      }
      http.end();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// =========================================================
// TAREA NÚCLEO 1: Setup y Control Térmico Físico
// =========================================================
void setup() {
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Conectando WiFi...");
  display.display();

  pinMode(ZC_PIN, INPUT);
  pinMode(DIM_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(ZC_PIN), zc_sns, RISING);
  filteredTemp = tc.readCelsius();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi Listo!");
  display.display();
  delay(1000);

  // Iniciar tarea de red en Core 0
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 10000, NULL, 1, &TaskNetwork, 0);

  Serial.println("\nt_s, targetC, tempC, power_percent");
  lastControlTime = millis();
}

void loop() {
  // --- Disparo del Dimmer ---
  if (zc_detected) {
    if (power > 0) {
      int delayTime = map(power, 0, 100, 8000, 100);
      delayMicroseconds(delayTime);
      digitalWrite(DIM_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(DIM_PIN, LOW);
    }
    zc_detected = false;
  }

  // --- Lazo de Control PI (cada 1 segundo) ---
  unsigned long now = millis();
  if (now - lastControlTime >= 1000) {
    float dt = (now - lastControlTime) / 1000.0;
    lastControlTime = now;

    float rawTemp = tc.readCelsius();
    if (isnan(rawTemp) || rawTemp == 0.0) {
      power = 0;
      systemActive = false;
      systemStatus = "ERR SENSOR";
    } else {
      filteredTemp = (0.8 * filteredTemp) + (0.2 * rawTemp);

      if (systemActive) {
        float error = targetTemp - filteredTemp;
        float pTerm = Kp * error;

        if (abs(error) < 10.0) errorIntegral += (error * dt);
        else errorIntegral = 0.0;

        if (errorIntegral > 40.0) errorIntegral = 40.0;
        if (errorIntegral < -40.0) errorIntegral = -40.0;

        float output = pTerm + (Ki * errorIntegral);

        if (output > 100.0) power = 100;
        else if (output < 0.0) power = 0;
        else power = (int)output;

        if (power == 100 && error > 5.0) systemStatus = "CALENTANDO";
        else if (power == 0 && error < -1.0) systemStatus = "ENFRIANDO";
        else systemStatus = "ESTABILIZANDO";
      } else {
        power = 0;
        if (systemStatus != "ERR SENSOR") systemStatus = "APAGADO";
      }
    }

    // Actualizar OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Vercel: ONLINE");
    display.setCursor(0, 15);
    display.print("Obj: "); display.print(targetTemp, 1); display.println(" C");
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.print(filteredTemp, 1); display.println(" C");
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print(systemStatus); display.print(" | P:"); display.print(power); display.println("%");
    display.display();

    // Salida Serial CSV
    Serial.print(now / 1000.0); Serial.print(",");
    Serial.print(targetTemp); Serial.print(",");
    Serial.print(filteredTemp); Serial.print(",");
    Serial.println(power);
  }
}
