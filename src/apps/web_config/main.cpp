#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// EEPROM size for storing credentials
#define EEPROM_SIZE 512

WebServer server(80);

// ===================================================================
// === Функции для работы с EEPROM (памятью) ===
// ===================================================================

void saveCredentials(const String& ssid, const String& password) {
  EEPROM.begin(EEPROM_SIZE);
  // Clear EEPROM
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  // Write SSID
  for (int i = 0; i < ssid.length(); i++) {
    EEPROM.write(i, ssid[i]);
  }
  // Write Password starting at address 100
  for (int i = 0; i < password.length(); i++) {
    EEPROM.write(100 + i, password[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

String readSSID() {
  EEPROM.begin(EEPROM_SIZE);
  String ssid = "";
  for (int i = 0; i < 100; i++) {
    char c = EEPROM.read(i);
    if (c == 0) break;
    ssid += c;
  }
  EEPROM.end();
  return ssid;
}

String readPassword() {
  EEPROM.begin(EEPROM_SIZE);
  String password = "";
  for (int i = 100; i < 200; i++) {
    char c = EEPROM.read(i);
    if (c == 0) break;
    password += c;
  }
  EEPROM.end();
  return password;
}

// ===================================================================
// === Функции-обработчики для веб-сервера ===
// ===================================================================

// Обработчик для страницы настройки (в режиме AP)
void handleConfigure() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body not received");
    return;
  }
  String body = server.arg("plain");
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, body);

  String ssid = doc["ssid"];
  String password = doc["password"];

  if (ssid.length() > 0 && password.length() > 0) {
    saveCredentials(ssid, password);
    server.send(200, "text/plain", "Credentials saved. Device will restart.");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Invalid credentials");
  }
}

// Простой HTML для корневой страницы (в режиме AP)
void handleRoot() {
  server.send(200, "text/html", "<h1>ESP32 Web Config</h1><p>POST to /configure-wifi with JSON body {'ssid':'your_ssid','password':'your_password'}</p>");
}

// Обработчик для пинга (в режиме STA - клиент)
void handlePing() {
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}


// ===================================================================
// === Функции запуска режимов ===
// ===================================================================

void startAPMode() {
  Serial.println("Starting AP Mode...");
  WiFi.softAP("Hub-device-config", "password");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/configure-wifi", HTTP_POST, handleConfigure);
  server.begin();
}

void startSTAMode() {
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // В режиме клиента нам нужен только /ping
    server.on("/ping", HTTP_GET, handlePing);
    server.begin();
}


// ===================================================================
// === Главные функции setup и loop ===
// ===================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  String ssid = readSSID();
  String password = readPassword();

  if (ssid.length() > 0) {
    Serial.println("Found saved credentials. Trying to connect to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      // Успешно подключились, запускаем сервер в режиме клиента
      startSTAMode();
    } else {
      // Не удалось подключиться, запускаем режим настройки
      Serial.println("\nFailed to connect. Starting configuration server.");
      startAPMode();
    }
  } else {
    // Сохраненных данных нет, запускаем режим настройки
    Serial.println("No credentials found. Starting configuration server.");
    startAPMode();
  }
}

void loop() {
  server.handleClient();
}
