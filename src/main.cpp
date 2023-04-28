#include <Arduino.h>
#include <freertos/FreeRTOS.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "freertos/task.h"

const uint AOUT_PIN = 25;  // ESP32 pin GIOP36 (ADC0) that connects to AOUT pin of moisture sensor

uint desert = 3700;    // The adc value measured at dryest state (just outside of pot)
uint aquarium = 1400;  // The adv value measured a couple minutes after drowning the poor plant in water

uint moistureLevel(uint sensorValue, uint min, uint max);

void setup() { Serial.begin(115200); }

void loop() {
  uint value = analogRead(AOUT_PIN);  // read the analog value from sensor

  Serial.printf("Moisture value: %d\n", value);
  Serial.printf("Moisture percentage: %d\n", moistureLevel(value, aquarium, desert));
  Serial.println("");
  delay(1000);
}

uint moistureLevel(uint sensorValue, uint min, uint max) {
  uint range = max - min;
  uint step = range / 100;
  uint percentage = 100 - ((sensorValue - min) / step);
  return percentage;
}