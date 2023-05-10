// Configurations
#include "./Configuration/Blynk.h"
#include "./Configuration/Wifi.h"

// Libraries
#include <Blynk/BlynkApi.h>    // Used for Blynk.notify()
#include <BlynkSimpleEsp32.h>  // Part of Blynk by Volodymyr Shymanskyy
#include <WiFi.h>              // Part of WiFi Built-In by Arduino
#include <WiFiClient.h>        // Part of WiFi Built-In by Arduino
#include <math.h>

// Pins (ESP devboard with soil pcb connected using dupont wires)
const ushort AOUT_PIN = 36;  // ESP32 pin (ADC0) that connects to AOUT pin of moisture sensor

// Pins (Built-in to DiyMore Esp32 soil module)
const ushort DHT11_PIN = 22;     // DHT11 air humidity sensor
const ushort ADC_PIN_1 = 32;     // ADC 1
const ushort LED_BLUE_PIN = 33;  // built in blue led
const ushort ADC_PIN_2 = 34;     // ADC 2

// GPIO pins that are not connected (according to manual)
const ushort nc06 = 6;
const ushort nc07 = 7;
const ushort nc08 = 8;
const ushort nc11 = 11;
const ushort nc20 = 20;
const ushort nc24 = 24;
const ushort nc37 = 37;
const ushort nc38 = 38;

// Limits
const int wifiHandlerThreadStackSize = 10000;
const int blynkHandlerThreadStackSize = 10000;

// Counters
unsigned long long wifiReconnectCounter = 0;
unsigned long long blynkReconnectCounter = 0;

// Timeouts
int wifiConnectionTimeout = 10000;
int blynkConnectionTimeout = 10000;
int blynkConnectionStabilizerTimeout = 5000;
ushort cycleDelayInMilliSeconds = 100;

// Task Handles
TaskHandle_t wifiConnectionHandlerThreadFunctionHandle;
TaskHandle_t blynkConnectionHandlerThreadFunctionHandle;
TaskHandle_t moistureMeasurementThreadFunctionHandle;
TaskHandle_t waterNotifierThreadFunctionHandle;

void wifiConnectionHandlerThreadFunction(void* params);
void blynkConnectionHandlerThreadFunction(void* params);
void measureMoisture(void* params);
void waterNotifier(void* params);

uint moistureLevel(uint sensorValue, uint min, uint max);
void WaitForBlynk(int cycleDelayInMilliSeconds);

// Config
uint desert = 3700;    // The adc value measured at dryest state (just outside of pot)
uint aquarium = 1400;  // The adv value measured a couple minutes after drowning the poor plant in water

// Blynk vars (global)
uint minimumSoilMoisturePercentage;
uint currentSoiMoisturePercentage;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BLUE_PIN, OUTPUT);
  digitalWrite(LED_BLUE_PIN, HIGH);

  xTaskCreatePinnedToCore(wifiConnectionHandlerThreadFunction, "Wifi Connection Handling Thread", wifiHandlerThreadStackSize, NULL, 20, &wifiConnectionHandlerThreadFunctionHandle, 1);
  xTaskCreatePinnedToCore(blynkConnectionHandlerThreadFunction, "Blynk Connection Handling Thread", blynkHandlerThreadStackSize, NULL, 20, &blynkConnectionHandlerThreadFunctionHandle, 1);
  xTaskCreatePinnedToCore(measureMoisture, "Moisture Level Measurement Thread", 10000, NULL, 20, &moistureMeasurementThreadFunctionHandle, 0);
  xTaskCreatePinnedToCore(waterNotifier, "Water Notifier Thread", 10000, NULL, 20, &waterNotifierThreadFunctionHandle, 0);
}

void loop() { Blynk.run(); }

// ----------------------------------------------------------------------------
// FUNCTIONS
// ----------------------------------------------------------------------------

// Blynk Functions

BLYNK_CONNECTED() {  // Restore hardware pins according to current UI config
  Blynk.syncAll();
}

BLYNK_WRITE(V1)  // Button Widget is writing to pin V1
{
  minimumSoilMoisturePercentage = param.asInt();
  Serial.printf("Min moisture was set to: %d\n", minimumSoilMoisturePercentage);
}

// General functions

void measureMoisture(void* params) {
  WaitForBlynk(10000);
  // Blynk.notify("Soil sensor up and running."); // TODO: find substitute
  while (true) {
    uint value = analogRead(ADC_PIN_1);  // read the analog value from sensor
    currentSoiMoisturePercentage = moistureLevel(value, aquarium, desert);
    Serial.printf("Moisture value: %d\n", value);
    Serial.printf("Moisture percentage: %d\n", currentSoiMoisturePercentage);
    Serial.println("");
    Blynk.virtualWrite(V0, currentSoiMoisturePercentage);
    delay(1000);
  }
}

void waterNotifier(void* params) {
  while (true) {
    if (currentSoiMoisturePercentage <= minimumSoilMoisturePercentage) {
      // Notify user here to water the plant
      // Blynk.notify("Water the plant. Current moisture: %d%", currentSoiMoisturePercentage); // TODO: find substitute
    }
    while (currentSoiMoisturePercentage <= minimumSoilMoisturePercentage) {
      delay(1000);
    }
    delay(1000);
  }
}

uint moistureLevel(uint sensorValue, uint min, uint max) {
  if (sensorValue < min) return 0;
  uint range = max - min;
  uint step = range / 100;
  uint percentage = 100 - ((sensorValue - min) / step);
  return percentage;
}

// Connection functions

void WaitForWifi(uint cycleDelayInMilliSeconds) {
  while (WiFi.status() != WL_CONNECTED) {
    delay(cycleDelayInMilliSeconds);
  }
}

void WaitForBlynk(int cycleDelayInMilliSeconds) {
  while (!Blynk.connected()) {
    delay(cycleDelayInMilliSeconds);
  }
}

void wifiConnectionHandlerThreadFunction(void* params) {
  uint time;
  while (true) {
    if (!WiFi.isConnected()) {
      try {
        Serial.printf("Connecting to Wifi: %s\n", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PW);  // initial begin as workaround to some espressif library bug
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PW);
        WiFi.setHostname("Desklight (ESP32, Blynk)");
        time = 0;
        while (WiFi.status() != WL_CONNECTED) {
          if (time >= wifiConnectionTimeout || WiFi.isConnected()) break;
          delay(cycleDelayInMilliSeconds);
          time += cycleDelayInMilliSeconds;
        }
      } catch (const std::exception e) {
        Serial.printf("Error occured: %s\n", e.what());
      }
      if (WiFi.isConnected()) {
        Serial.printf("Connected to Wifi: %s\n", WIFI_SSID);
        wifiReconnectCounter = 0;
      }
    }
    delay(1000);
    Serial.printf("Wifi Connection Handler Thread current stack size: %d , current Time: %d\n", wifiHandlerThreadStackSize - uxTaskGetStackHighWaterMark(NULL), xTaskGetTickCount());
  };
}

void blynkConnectionHandlerThreadFunction(void* params) {
  uint time;
  while (true) {
    if (!Blynk.connected()) {
      Serial.printf("Connecting to Blynk: %s\n", BLYNK_USE_LOCAL_SERVER == true ? BLYNK_SERVER : "Blynk Cloud Server");
      if (BLYNK_USE_LOCAL_SERVER)
        Blynk.config(BLYNK_AUTH, BLYNK_SERVER, BLYNK_PORT);
      else
        Blynk.config(BLYNK_AUTH);
      Serial.printf("Pre- Blynk.connect()\n");
      try {
        Blynk.connect(10000);  // Connects using the chosen Blynk.config
      } catch (...) {
        Serial.printf("Blynk.connect() timed out\n");
      }
      Serial.printf("Post- Blynk.connect()\n");
      uint time = 0;
      while (!Blynk.connected()) {
        if (time >= blynkConnectionTimeout || Blynk.connected()) break;
        delay(cycleDelayInMilliSeconds);
        time += cycleDelayInMilliSeconds;
      }
      if (Blynk.connected()) {
        Serial.printf("Connected to Blynk: %s\n", BLYNK_USE_LOCAL_SERVER ? BLYNK_SERVER : "Blynk Cloud Server");
        delay(blynkConnectionStabilizerTimeout);
      }
    }
    delay(1000);
    Serial.printf("Blynk Connection Handler Thread current stack size: %d , current Time: %d\n", blynkHandlerThreadStackSize - uxTaskGetStackHighWaterMark(NULL), xTaskGetTickCount());
  }
}

// TODO: needs own thread
void flashLed(uint durationInMs) {
  pinMode(LED_BLUE_PIN, OUTPUT);
  digitalWrite(LED_BLUE_PIN, HIGH);
  delay(durationInMs);
  digitalWrite(LED_BLUE_PIN, LOW);
}