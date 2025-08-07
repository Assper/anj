#include <Arduino.h>
#include "mqtt.hpp"

MQTT messaging;

void setup()
{
  Serial.begin(115200);
  delay(1000); // Wait for serial to initialize
  messaging = MQTT();
  messaging.connect();
}

void loop()
{
  messaging.receive_message();
  messaging.send_message("ping from esp32...");
  delay(1000);
}
