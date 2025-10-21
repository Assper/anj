#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

#define EEPROM_SIZE 512

WebServer server(80);

// Статус подключения
enum DeviceStatus {
  STATUS_AP_MODE,
  STATUS_CONNECTING,
  STATUS_CONNECTED,
  STATUS_FAILED
};

DeviceStatus currentStatus = STATUS_AP_MODE;
String lastError = "";
unsigned long lastConnectionAttempt = 0;
unsigned long lastStatusCheck = 0;

// ===================================================================
// === Функции для работы с EEPROM ===
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
// === Обработчики веб-сервера ===
// ===================================================================

// Добавляем CORS заголовки для всех ответов
void setCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
  setCORSHeaders();
  server.send(204);
}

void handleConfigure() {
  setCORSHeaders();
  
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
    
    DynamicJsonDocument response(256);
    response["status"] = "success";
    response["message"] = "Credentials saved. Device will restart.";
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    
    server.send(200, "application/json", jsonResponse);
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid credentials\"}");
  }
}

// НОВИЙ HANDLER - зміна WiFi кредів без reset
void handleReconfigure() {
  setCORSHeaders();
  
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
    
    DynamicJsonDocument response(256);
    response["status"] = "success";
    response["message"] = "New credentials saved. Reconnecting...";
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    
    server.send(200, "application/json", jsonResponse);
    
    delay(500);
    // Відключаємось та підключаємось з новими кредами
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());
    currentStatus = STATUS_CONNECTING;
    lastConnectionAttempt = millis();
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid credentials\"}");
  }
}

void handleRoot() {
  setCORSHeaders();
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Config</title>";
  html += "<style>body{font-family:Arial;margin:20px;} input{width:100%;padding:10px;margin:5px 0;} button{width:100%;padding:15px;background:#4CAF50;color:white;border:none;cursor:pointer;}</style>";
  html += "</head><body>";
  html += "<h1>ESP32 WiFi Config</h1>";
  html += "<p>Status: Configuration Mode</p>";
  html += "<form id='wifiForm'>";
  html += "<input type='text' id='ssid' placeholder='WiFi SSID' required><br>";
  html += "<input type='password' id='password' placeholder='WiFi Password' required><br>";
  html += "<button type='submit'>Connect</button>";
  html += "</form>";
  html += "<div id='result'></div>";
  html += "<script>";
  html += "document.getElementById('wifiForm').addEventListener('submit', function(e) {";
  html += "  e.preventDefault();";
  html += "  var ssid = document.getElementById('ssid').value;";
  html += "  var password = document.getElementById('password').value;";
  html += "  fetch('/configure-wifi', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/json'},";
  html += "    body: JSON.stringify({ssid: ssid, password: password})";
  html += "  }).then(r => r.json()).then(d => {";
  html += "    document.getElementById('result').innerHTML = '<p style=\"color:green\">' + d.message + '</p>';";
  html += "  }).catch(e => {";
  html += "    document.getElementById('result').innerHTML = '<p style=\"color:red\">Error: ' + e + '</p>';";
  html += "  });";
  html += "});";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Обработчик статуса - работает в обоих режимах
void handleStatus() {
  setCORSHeaders();
  
  DynamicJsonDocument doc(512);
  
  switch(currentStatus) {
    case STATUS_AP_MODE:
      doc["status"] = "ap_mode";
      doc["message"] = "Device in configuration mode";
      doc["ip"] = WiFi.softAPIP().toString();
      doc["ssid"] = "Hub-device-config";
      doc["mode"] = "AP";
      break;
      
    case STATUS_CONNECTING:
      doc["status"] = "connecting";
      doc["message"] = "Attempting to connect to WiFi";
      doc["ssid"] = readSSID();
      doc["attempt_time"] = millis() - lastConnectionAttempt;
      doc["mode"] = "STA";
      break;
      
    case STATUS_CONNECTED:
      doc["status"] = "connected";
      doc["message"] = "Connected to WiFi";
      doc["ip"] = WiFi.localIP().toString();
      doc["ssid"] = WiFi.SSID();
      doc["rssi"] = WiFi.RSSI();
      doc["mode"] = "STA";
      break;
      
    case STATUS_FAILED:
      doc["status"] = "failed";
      doc["message"] = "Failed to connect";
      doc["error"] = lastError;
      doc["ssid"] = readSSID();
      doc["mode"] = "AP";
      break;
  }
  
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

// ⭐️ ПОКРАЩЕНИЙ - Обработчик пинга з більш детальною інформацією
void handlePing() {
  setCORSHeaders();
  
  DynamicJsonDocument doc(384);
  doc["status"] = "ok";
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  doc["connected"] = isConnected;
  
  if (isConnected) {
    doc["ip"] = WiFi.localIP().toString();
    doc["mode"] = "STA";
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["gateway"] = WiFi.gatewayIP().toString();
  } else {
    doc["ip"] = WiFi.softAPIP().toString();
    doc["mode"] = "AP";
    doc["ssid"] = "Hub-device-config";
  }
  
  // Додаємо MAC адрес
  doc["mac"] = WiFi.macAddress();
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

// Сброс настроек
void handleReset() {
  setCORSHeaders();
  
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
  
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Credentials cleared. Restarting...\"}");
  delay(1000);
  ESP.restart();
}

// ===================================================================
// === Функции запуска режимов ===
// ===================================================================

void startAPMode() {
  Serial.println("Starting AP Mode...");
  currentStatus = STATUS_AP_MODE;
  
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  delay(100);
  
  // Генеруємо унікальний IP базуючись на MAC адресі
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  // Генеруємо IP в діапазоні 192.168.100.X - 192.168.100.255
  uint8_t lastOctet = mac[5];
  if (lastOctet < 2) lastOctet = 2; // Уникаємо .0 та .1
  
  IPAddress localIP(192, 168, 100, lastOctet);
  IPAddress gateway(192, 168, 100, lastOctet);
  IPAddress subnet(255, 255, 255, 0);
  
  Serial.print("Device MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  Serial.print("Generated unique IP: 192.168.100.");
  Serial.println(lastOctet);
  
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP("Hub-device-config", "password");
  
  delay(500);
  
  IPAddress IP = WiFi.softAPIP();
  
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("Connect to 'Hub-device-config' WiFi network");
  Serial.println("Password: password");

  // Регистрируем все обработчики
  server.on("/", handleRoot);
  server.on("/configure-wifi", HTTP_POST, handleConfigure);
  server.on("/configure-wifi", HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/ping", HTTP_OPTIONS, handleOptions);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/reset", HTTP_OPTIONS, handleOptions);
  
  server.begin();
  Serial.println("HTTP server started in AP mode");
}

void startSTAMode() {
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  currentStatus = STATUS_CONNECTED;

  // В режиме клиента регистрируем обработчики
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/ping", HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.on("/reconfigure-wifi", HTTP_POST, handleReconfigure);
  server.on("/reconfigure-wifi", HTTP_OPTIONS, handleOptions);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/reset", HTTP_OPTIONS, handleOptions);
  
  server.begin();
  Serial.println("HTTP server started in STA mode");
}

// ===================================================================
// === Setup и Loop ===
// ===================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 WiFi Configuration v1.0");
  Serial.println("=================================\n");

  String ssid = readSSID();
  String password = readPassword();

  if (ssid.length() > 0) {
    // Есть сохраненные credentials - пытаемся подключиться
    Serial.println("Found saved credentials:");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.println("Attempting to connect...");
    
    currentStatus = STATUS_CONNECTING;
    lastConnectionAttempt = millis();
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      // Успешно подключились
      Serial.println("");
      startSTAMode();
    } else {
      // Не удалось подключиться - переходим в режим AP
      Serial.println("\nFailed to connect to saved WiFi.");
      Serial.println("Starting configuration server...");
      currentStatus = STATUS_FAILED;
      lastError = "Connection timeout - check SSID/password";
      startAPMode();
    }
  } else {
    // Нет сохраненных credentials - запускаем режим AP
    Serial.println("No saved WiFi credentials found.");
    Serial.println("Starting configuration mode...");
    startAPMode();
  }
  
  Serial.println("\nSetup complete!");
  Serial.println("=================================\n");
}

void loop() {
  server.handleClient();
  
  // Мониторинг підключення - перевірка кожні 5 секунд
  if (millis() - lastStatusCheck > 5000) {
    lastStatusCheck = millis();
    
    if (currentStatus == STATUS_CONNECTED && WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Attempting to reconnect...");
      currentStatus = STATUS_CONNECTING;
      lastConnectionAttempt = millis();
      WiFi.reconnect();
    }
  }
  
  // Автоматическое переподключение
  if (currentStatus == STATUS_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      currentStatus = STATUS_CONNECTED;
      Serial.println("Reconnected successfully!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      
      // Перезапускаємо server після реконекту
      server.begin();
    } else if (millis() - lastConnectionAttempt > 30000) {
      // Если не удалось подключиться за 30 секунд
      currentStatus = STATUS_FAILED;
      lastError = "Reconnection timeout";
      Serial.println("Reconnection failed. Switching to AP mode...");
      startAPMode();
    }
  }
  
  delay(10);
}
