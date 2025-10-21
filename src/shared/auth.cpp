#include "auth.hpp"
#include <ArduinoJson.h>
#include <WiFi.h>

Auth::Auth(const String& ssid, const String& password)
    : _ssid(ssid), _password(password) {}

void Auth::setCredentials(const String& ssid, const String& password) {
    Serial.printf("Setting new credentials: SSID=%s, PASS=%s\n", ssid.c_str(), password.c_str());
    _ssid = ssid;
    _password = password;
}

void Auth::connect_wifi() {
    Serial.printf("[WiFi] Connecting to SSID: %s\n", _ssid.c_str());
    Serial.printf("[WiFi] Using password: %s\n", _password.c_str());
WiFi.disconnect(true);  // true - удалить старую конфигурацию
delay(1000);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n[WiFi] Failed to connect!");
        print_wifi_status();
    }
}




void Auth::loop_wifi() {
    if (WiFi.status() != WL_CONNECTED && millis() - _lastReconnectAttempt > _reconnectInterval) {
        Serial.println("[WiFi] Lost connection or credentials changed, reconnecting...");
        _lastReconnectAttempt = millis();
        connect_wifi();
        Serial.print("[WiFi] After connect_wifi status: ");
        Serial.println(WiFi.status());
    }
}

bool Auth::is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

void Auth::disconnect_wifi() {
    WiFi.disconnect(true, true); // отключить и очистить
    Serial.println("[WiFi] Disconnected.");
    delay(1000); // пауза для стабилизации
}
void Auth::print_wifi_status() {
    wl_status_t status = WiFi.status();
    switch (status) {
        case WL_NO_SSID_AVAIL:
            Serial.println("[WiFi] No SSID available");
            break;
        case WL_CONNECT_FAILED:
            Serial.println("[WiFi] Connection failed (wrong password?)");
            break;
        case WL_IDLE_STATUS:
            Serial.println("[WiFi] Idle");
            break;
        case WL_DISCONNECTED:
            Serial.println("[WiFi] Disconnected");
            break;
        default:
            Serial.print("[WiFi] Unknown status: ");
            Serial.println(status);
            break;
    }
}


