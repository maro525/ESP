extern "C"{
#include <spi.h>
#include <spi_register.h>
}

#include <Arduino.h>

// loopを回しやすくするライブラリ
#include <Ticker.h>

Ticker ticker;

int led = 5;

void setup() {
  Serial.begin(115200);
  Serial.print("\n");

  pinMode(led, OUTPUT);

  spi_init(HSPI);

  ticker.attach_ms(1000, timer);
}

void loop() {
}

void timer() {

  uint32 val0 = check(0);
  uint32 val1 = check(1);

  Serial.print(" Sencor1:");
  Serial.print(val0);
  Serial.print(" Sencor2:");
  Serial.print(val1);
  Serial.print("\n");

  if (val0 >= val1) {
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);
  }
}

uint32 check(int channel) {
  uint8 cmd = (0b11 << 3) | channel;

  const uint32 COMMAND_LENGTH = 5;
  const uint32 RESPONSE_LENGTH = 12;

  uint32 retval = spi_transaction(HSPI, 0, 0, 0, 0, COMMAND_LENGTH, cmd, RESPONSE_LENGTH, 0);

  retval = retval & 0x3FF;
  return retval;
}
