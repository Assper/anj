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
  // Fade in (0 to 255)
  for (int brightness = 0; brightness <= 255; brightness++)
  {
    analogWrite(LED_BUILTIN, brightness);
    delay(10);
  }

  // Fade out (255 to 0)
  for (int brightness = 255; brightness >= 0; brightness--)
  {
    analogWrite(LED_BUILTIN, brightness);
    delay(10);
  }
}