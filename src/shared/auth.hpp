// auth.hpp
#ifndef AUTH_HPP
#define AUTH_HPP

#include <Arduino.h>
#include <WiFi.h>

class Auth {
public:
    Auth(const String& ssid, const String& password);
    void connect_wifi();
    void loop_wifi();
    bool is_connected();
    void disconnect_wifi();
    String _ssid;

    void setCredentials(const String& ssid, const String& password);

private:
    String _password;
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _reconnectInterval = 5000;
    void print_wifi_status();
};

#endif
