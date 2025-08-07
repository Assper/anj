#include <Arduino.h>
#include "auth.hpp"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Auth auth;
  auth.scan_bluetooth();
  auth.connect_bluetooth();
}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
