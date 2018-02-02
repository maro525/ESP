#include <Arduino.h>

extern "C" {
#include "user_interface.h"
}

int adc_key_in = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  adc_key_in = system_adc_read();
  String(adc_key_in);
  Serial.println(value);
  delay(1000);
}
