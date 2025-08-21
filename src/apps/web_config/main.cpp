#include "../../../lib/MQTT/src/mqtt.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "../../shared/auth.hpp"

// Web server for configuration
WebServer server(80);
DNSServer dnsServer;
const char* ap_ssid = "ESP32-Config";
const char* ap_password = "12345678";

// Your existing instances
Auth auth("", ""); // Start with empty credentials
MQTT mqtt;
bool wifiCredentialsUpdated = false;
bool configMode = true;

// Function declarations
void startAPMode();
void connectToWiFi();
void handleRoot();
void handleSave();
void handleStatus();
void handleMQTT();

const char* config_html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 WiFi Config</title>
    <style>
        body { font-family: Arial; margin: 40px; background: #f0f0f0; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; }
        input { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
        button { width: 100%; padding: 12px; background: #007cba; color: white; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #005a8b; }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <div class="container">
        <h2>ESP32 WiFi Configuration</h2>
        <form action="/save" method="POST">
            <label>WiFi Network Name (SSID):</label>
            <input type="text" name="ssid" placeholder="Enter WiFi name" required>

            <label>WiFi Password:</label>
            <input type="password" name="password" placeholder="Enter WiFi password">

            <button type="submit">Connect to WiFi</button>
        </form>
        <p style="color: #666; font-size: 12px;">
            After connecting, the ESP32 will join your WiFi network and start normal operation.
        </p>
    </div>
</body>
</html>
)";

void connectToWiFi() {
    Serial.println("[MAIN] Attempting WiFi connection...");

    // Stop AP mode components
    if (configMode) {
        server.stop();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        delay(1000);
    }

    // Try to connect
    auth.connect_wifi();

    if (auth.is_connected()) {
        Serial.println("[MAIN] WiFi connected! Starting normal operation...");
        configMode = false;

        // Setup MQTT
        mqtt.setAuthInstance(&auth);
        mqtt.connect();

    } else {
        Serial.println("[MAIN] WiFi connection failed. Returning to AP mode...");
        delay(2000);
        startAPMode();
    }
}

void startAPMode() {
    Serial.println("[WEB] Starting Access Point mode...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);

    Serial.println("[WEB] Access Point started:");
    Serial.println("Network: " + String(ap_ssid));
    Serial.println("Password: " + String(ap_password));
    Serial.println("IP: " + WiFi.softAPIP().toString());
    Serial.println("Open your browser and go to: http://" + WiFi.softAPIP().toString());

    // Start DNS server for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/status", handleStatus);
    server.on("/mqtt", HTTP_POST, handleMQTT);
    server.onNotFound(handleRoot); // Redirect any request to config page

    server.begin();
    Serial.println("[WEB] Web server started");

    configMode = true;
}

void handleRoot() {
    server.send(200, "text/html", config_html);
}

void handleSave() {
    if (server.hasArg("ssid")) {
        String ssid = server.arg("ssid");
        String password = server.hasArg("password") ? server.arg("password") : "";

        Serial.println("[WEB] New WiFi credentials received:");
        Serial.println("SSID: " + ssid);
        Serial.println("Password: [HIDDEN]");

        server.send(200, "text/html",
            "<html><body style='font-family:Arial;margin:40px;background:#f0f0f0;'>"
            "<div style='max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px;text-align:center;'>"
            "<h2>Connecting to WiFi...</h2>"
            "<p>ESP32 is now trying to connect to your WiFi network.</p>"
            "<p>This may take up to 30 seconds.</p>"
            "<p style='color:#666;font-size:12px;'>Check the serial monitor for connection status.</p>"
            "</div></body></html>");

        // Set new credentials
        auth.setCredentials(ssid, password);

        // Try to connect
        delay(2000);
        connectToWiFi();
    } else {
        server.send(400, "text/html", "Missing SSID parameter");
    }
}

void handleStatus() {
    DynamicJsonDocument doc(256);
    doc["wifi_connected"] = auth.is_connected();
    doc["config_mode"] = configMode;
    doc["ip_address"] = WiFi.localIP().toString();

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleMQTT() {
    if (server.hasArg("plain") == false) {
        server.send(400, "text/plain", "Body not received");
        return;
    }

    String message = "Message received";
    String body = server.arg("plain");
    server.send(200, "text/plain", message);

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    String topic = doc["topic"];
    String mqttMessage = doc["message"];

    if (topic == "esp32/test") {
        mqtt.send_message(mqttMessage.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("=== ESP32 Starting ===");
    Serial.printf("Flash size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Always start in configuration mode since we have empty credentials
    Serial.println("[MAIN] Starting in configuration mode...");
    startAPMode();
}

void loop() {
    delay(100); // Prevent watchdog reset
    yield();

    if (configMode) {
        // Handle web configuration
        dnsServer.processNextRequest();
        server.handleClient();

        // Show status every 30 seconds
        static unsigned long lastStatus = 0;
        if (millis() - lastStatus > 30000) {
            lastStatus = millis();
            Serial.println("[WEB] Waiting for configuration... Connect to: " + String(ap_ssid));
        }

    } else {
        // Normal operation mode

        // Check WiFi connection
        auth.loop_wifi();

        if (auth.is_connected()) {
            // Handle MQTT
            mqtt.receive_message();

            // Handle credential updates from MQTT
            if (wifiCredentialsUpdated) {
                wifiCredentialsUpdated = false;
                Serial.println("[MAIN] Credentials updated via MQTT, reconnecting...");
                connectToWiFi();
            }

            // Your main application code here
            static unsigned long lastMessage = 0;
            if (millis() - lastMessage > 10000) {
                lastMessage = millis();
                mqtt.send_message("Hello from ESP32!");
                Serial.println("[MAIN] Sent test message");
            }

        } else {
            Serial.println("[MAIN] WiFi connection lost. Starting AP mode...");
            startAPMode();
        }
    }
}
