#include "mqtt.hpp"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../../../src/shared/auth.hpp"

const char* mqtt_server = "192.168.3.137";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

Auth* authInstance = nullptr;

extern bool wifiCredentialsUpdated;

void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);

    if (String(topic) == "esp32/wifi") {
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            String ssid = doc["ssid"].as<String>();
            String password = doc["password"].as<String>();
            Serial.println("Received new WiFi credentials.");
            if (authInstance) {
                authInstance->setCredentials(ssid, password);
                wifiCredentialsUpdated = true;  // флаг для переподключения
            }
        } else {
            Serial.println("Failed to parse WiFi credentials");
        }
    } else if (String(topic) == "esp32/test") {
        Serial.println("Ping received!");
    }
}



void MQTT::setAuthInstance(Auth* auth) {
    authInstance = auth;
}

void MQTT::connect() {
    if (!authInstance || !authInstance->is_connected()) {
        Serial.println("[MQTT] WiFi not connected. Connect WiFi first.");
        return;
    }

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);

        if (client.connect(clientId.c_str())) {
            Serial.println("Connected to MQTT");
            client.subscribe("esp32/wifi");
            client.subscribe("esp32/test");
        } else {
            Serial.print("Failed to connect MQTT, state=");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

void MQTT::disconnect() {
    client.disconnect();
    Serial.println("Disconnected from MQTT.");
}

void MQTT::send_message(const char* message) {
    if (client.connected()) {
        client.publish("esp32/test", message);
    }
}

void MQTT::receive_message() {
    if (!client.connected()) {
        Serial.println("[MQTT] Client disconnected, trying to reconnect...");
        connect();
    }
    client.loop();
}

