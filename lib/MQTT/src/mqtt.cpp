#include "mqtt.hpp"
#include <WiFi.h>
#include <PubSubClient.h>

// MQTT Broker details
const char* mqtt_server = "192.168.3.37"; // Replace with your computer's IP address
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/test";

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}

void MQTT::connect() {
  // Connect to Wi-Fi
  // Note: You'll need to add your Wi-Fi credentials here
  WiFi.begin("Assper", "gkzt3zFyYX");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void MQTT::disconnect() {
  client.disconnect();
  Serial.println("Disconnected from MQTT.");
}

void MQTT::send_message(const char *message) {
  if (client.connected()) {
    client.publish(mqtt_topic, message);
  }
}

void MQTT::receive_message() {
  if (client.connected()) {
    client.loop();
  }
}
