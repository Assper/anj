#ifndef MQTT_HPP
#define MQTT_HPP

#include "../../../src/shared/auth.hpp"  // включи здесь, чтобы 'Auth' был известен

class MQTT {
public:
    void connect();
    void disconnect();
    void send_message(const char *message);
    void receive_message();
    void setAuthInstance(Auth* auth);
};

#endif
