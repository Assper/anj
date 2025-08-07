#ifndef MQTT_HPP
#define MQTT_HPP

class MQTT
{
public:
  void connect();
  void disconnect();
  void send_message(const char *message);
  void receive_message();
};

#endif // MQTT_HPP
