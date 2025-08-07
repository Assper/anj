#include "messaging.hpp"
#include <Arduino.h>

void Messaging::connect() {
  Serial.println("Connecting to messaging service...");
}

void Messaging::disconnect() {
  Serial.println("Disconnecting from messaging service...");
}

void Messaging::send_message(const char* message) {
  Serial.println(message);
}

void Messaging::receive_message() {
  Serial.println("Receiving message...");
}
