#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

String ssid = "";
String password = "";

enum SystemState {
  STATE_WIFI_CONNECTING,
  STATE_WIFI_CONNECTED,
  STATE_BT_WAITING,
  STATE_BT_RECEIVING
};

SystemState currentState = STATE_WIFI_CONNECTING;
unsigned long lastStateChange = 0;
unsigned long lastWiFiCheck = 0;
bool btInitialized = false;

void stopBluetooth() {
  if (btInitialized) {
    Serial.println("[BT] Stopping Bluetooth...");
    SerialBT.end();
    btInitialized = false;
    delay(1000); // Give time to properly stop
  }
}

void startBluetooth() {
  if (!btInitialized) {
    Serial.println("[BT] Starting Bluetooth...");
    WiFi.mode(WIFI_OFF); // Ensure WiFi is off
    delay(500);
    
    if (SerialBT.begin("ESP32_Config")) {
      Serial.println("[BT] Bluetooth started successfully");
      btInitialized = true;
      currentState = STATE_BT_WAITING;
      lastStateChange = millis();
    } else {
      Serial.println("[BT] Failed to start Bluetooth");
      delay(2000);
    }
  }
}

bool connectWiFi() {
  if (ssid.length() == 0) {
    Serial.println("[WiFi] No SSID provided");
    return false;
  }

  stopBluetooth(); // Ensure Bluetooth is stopped
  
  Serial.println("[WiFi] Starting WiFi connection...");
  Serial.print("[WiFi] SSID: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  currentState = STATE_WIFI_CONNECTING;
  lastStateChange = millis();
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
    
    // Feed watchdog
    yield();
    
    // Check for timeout
    if (millis() - start > 15000) {
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[WiFi] Connected successfully!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    currentState = STATE_WIFI_CONNECTED;
    lastStateChange = millis();
    return true;
  } else {
    Serial.println();
    Serial.println("[WiFi] Connection failed");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Give serial monitor time to connect
  
  Serial.println();
  Serial.println("=== ESP32 Starting ===");
  Serial.println("Free heap: " + String(ESP.getFreeHeap()));
  
  // Set default credentials (change these to your router)
  ssid = "Router";
  password = "defaultPassword";
  
  // Try to connect to WiFi first
  if (!connectWiFi()) {
    Serial.println("[MAIN] WiFi connection failed, starting Bluetooth...");
    startBluetooth();
  }
  
  Serial.println("=== Setup Complete ===");
}

void loop() {
  // Essential: Feed the watchdog timer
  yield();
  delay(50); // Minimum delay to prevent tight loops
  
  unsigned long now = millis();
  
  switch (currentState) {
    case STATE_WIFI_CONNECTING:
      // This state is handled in connectWiFi() function
      // If we're here, connection failed
      Serial.println("[MAIN] WiFi connection timeout, switching to Bluetooth");
      startBluetooth();
      break;
      
    case STATE_WIFI_CONNECTED:
      // Check WiFi connection every 5 seconds
      if (now - lastWiFiCheck > 5000) {
        lastWiFiCheck = now;
        
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("[WiFi] Connection lost!");
          Serial.println("Free heap: " + String(ESP.getFreeHeap()));
          
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          delay(1000);
          
          startBluetooth();
        } else {
          // WiFi is connected - do your main work here
          Serial.println("[MAIN] WiFi OK - Running main code...");
          
          // YOUR MQTT CODE GOES HERE
          // Example: mqtt.loop(); mqtt.publish(); etc.
          
          delay(1000); // Adjust based on your needs
        }
      }
      break;
      
    case STATE_BT_WAITING:
      if (!btInitialized) {
        Serial.println("[BT] Bluetooth not initialized, restarting...");
        startBluetooth();
        break;
      }
      
      if (SerialBT.hasClient()) {
        Serial.println("[BT] Client connected");
        SerialBT.println("Connected! Send WiFi credentials as JSON: {\"ssid\":\"YourWiFi\",\"password\":\"YourPassword\"}");
        currentState = STATE_BT_RECEIVING;
        lastStateChange = now;
      } else {
        // Print waiting message every 10 seconds
        if (now - lastStateChange > 10000) {
          Serial.println("[BT] Still waiting for client...");
          lastStateChange = now;
        }
      }
      break;
      
    case STATE_BT_RECEIVING:
      if (!SerialBT.hasClient()) {
        Serial.println("[BT] Client disconnected");
        currentState = STATE_BT_WAITING;
        lastStateChange = now;
        break;
      }
      
      if (SerialBT.available()) {
        String receivedData = "";
        
        // Read all available data
        while (SerialBT.available()) {
          char c = SerialBT.read();
          if (c == '\n' || c == '\r') {
            break;
          }
          receivedData += c;
          delay(1); // Small delay between characters
        }
        
        receivedData.trim();
        
        if (receivedData.length() > 0) {
          Serial.println("[BT] Received: " + receivedData);
          
          // Parse JSON
          DynamicJsonDocument doc(512);
          DeserializationError error = deserializeJson(doc, receivedData);
          
          if (error) {
            Serial.println("[BT] JSON parse error: " + String(error.c_str()));
            SerialBT.println("ERROR: Invalid JSON format");
            SerialBT.println("Expected: {\"ssid\":\"YourWiFi\",\"password\":\"YourPassword\"}");
          } else {
            if (doc.containsKey("ssid") && doc.containsKey("password")) {
              String newSsid = doc["ssid"].as<String>();
              String newPassword = doc["password"].as<String>();
              
              if (newSsid.length() > 0) {
                ssid = newSsid;
                password = newPassword;
                
                Serial.println("[BT] New credentials received:");
                Serial.println("SSID: " + ssid);
                Serial.println("Password: [HIDDEN]");
                
                SerialBT.println("Credentials received! Attempting WiFi connection...");
                SerialBT.flush();
                delay(500);
                
                // Try to connect with new credentials
                if (connectWiFi()) {
                  Serial.println("[MAIN] WiFi connected with new credentials!");
                  // Bluetooth will be stopped in connectWiFi()
                } else {
                  Serial.println("[MAIN] WiFi connection failed with new credentials");
                  startBluetooth();
                  if (btInitialized) {
                    SerialBT.println("WiFi connection failed. Please check credentials and try again.");
                  }
                }
              } else {
                SerialBT.println("ERROR: SSID cannot be empty");
              }
            } else {
              SerialBT.println("ERROR: Missing 'ssid' or 'password' in JSON");
            }
          }
        }
      }
      break;
  }
  
  // Additional watchdog feeding
  if (now % 1000 == 0) {
    yield();
  }
}