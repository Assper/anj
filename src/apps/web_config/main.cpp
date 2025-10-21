#include "../../../lib/MQTT/src/mqtt.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <esp_mac.h> // Явно подключим для esp_read_mac
#include "../../shared/auth.hpp"

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---
WebServer server(80);
DNSServer dnsServer;
const char* ap_password = "12345678"; // Пароль к точке доступа
String deviceName = ""; // Будем хранить уникальное имя устройства здесь

// --- Экземпляры твоих классов ---
Auth auth("", ""); 
MQTT mqtt;
bool wifiCredentialsUpdated = false;
bool configMode = true;

// --- ОБЪЯВЛЕНИЕ ФУНКЦИЙ ---
void startAPMode();
void connectToWiFi();
void handleCaptivePortal(); // ИЗМЕНЕНО: Новая функция для редиректа
void handleStatus();
void handleMQTT();
// handleRoot и handleSave удалены

// --- РЕАЛИЗАЦИЯ ФУНКЦИЙ ---

// Генерирует уникальное имя на основе MAC-адреса
String generateDeviceName() {
    uint8_t mac[6];
    char macStr[18] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return "Hub-Device-" + String(macStr);
}

void connectToWiFi() {
    Serial.println("[MAIN] Attempting WiFi connection...");

    if (configMode) {
        server.stop();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        delay(1000);
    }

    auth.connect_wifi();

    if (auth.is_connected()) {
        Serial.println("[MAIN] WiFi connected! Starting normal operation...");
        configMode = false;
        mqtt.setAuthInstance(&auth);
        mqtt.connect();
    } else {
        Serial.println("[MAIN] WiFi connection failed. Returning to AP mode...");
        delay(2000);
        startAPMode();
    }
}

// ⭐️ ИЗМЕНЕНО: Эта функция теперь отвечает за редирект в приложение
void handleCaptivePortal() {
    // Формируем специальную ссылку (deep link) для твоего приложения
    String redirectUrl = "anj-iot://configure?device_name=" + deviceName;
    
    // Отправляем HTTP-ответ 302, который говорит браузеру перейти по новой ссылке
    server.sendHeader("Location", redirectUrl, true);
    server.send(302, "text/plain", ""); // Тело ответа может быть пустым
    Serial.println("[WEB] Captive Portal: Redirecting to mobile app via " + redirectUrl);
}


void startAPMode() {
    Serial.println("[WEB] Starting Access Point mode...");
    
    // ⭐️ ИЗМЕНЕНО: Генерируем имя один раз и сохраняем в глобальную переменную
    deviceName = generateDeviceName();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(deviceName.c_str(), ap_password); // Используем .c_str() для String

    Serial.println("[WEB] Access Point started:");
    Serial.println("Network: " + deviceName);
    Serial.println("Password: " + String(ap_password));
    Serial.println("IP: " + WiFi.softAPIP().toString());
    
    dnsServer.start(53, "*", WiFi.softAPIP());

    // ⭐️ ИЗМЕНЕНО: Настраиваем сервер на редирект
    server.on("/", handleCaptivePortal);            // При заходе на главную страницу
    server.on("/status", handleStatus);             // Оставим для отладки
    server.on("/mqtt", HTTP_POST, handleMQTT);      // Оставим для отладки
    server.onNotFound(handleCaptivePortal);         // Для всех остальных запросов (это ключ к работе Captive Portal)
    // server.on("/save", ...) удален

    server.begin();
    Serial.println("[WEB] Web server started");

    configMode = true;
}

// ⭐️ УДАЛЕНО: handleRoot() и config_html больше не нужны
// ⭐️ УДАЛЕНО: handleSave() больше не нужен

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
    String body = server.arg("plain");
    server.send(200, "text/plain", "Message received");
    
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, body)) return;

    String topic = doc["topic"];
    String mqttMessage = doc["message"];

    if (topic == "esp32/test") {
        mqtt.send_message(mqttMessage.c_str());
    }
}


// --- ОСНОВНЫЕ ФУНКЦИИ ---
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== ESP32 Starting ===");
    startAPMode();
}

void loop() {
    delay(100); 
    yield();

    if (configMode) {
        dnsServer.processNextRequest();
        server.handleClient();
        
        static unsigned long lastStatus = 0;
        if (millis() - lastStatus > 30000) {
            lastStatus = millis();
            // ⭐️ ИЗМЕНЕНО: Используем глобальную переменную deviceName
            Serial.println("[WEB] Waiting for configuration... Connect to: " + deviceName);
        }

    } else {
        auth.loop_wifi();
        if (auth.is_connected()) {
            mqtt.receive_message();

            if (wifiCredentialsUpdated) {
                wifiCredentialsUpdated = false;
                Serial.println("[MAIN] Credentials updated via MQTT, reconnecting...");
                connectToWiFi();
            }

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