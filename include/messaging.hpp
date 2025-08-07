#ifndef MESSAGING_HPP
#define MESSAGING_HPP

class Messaging {
  public:
    void connect();
    void disconnect();
    void send_message(const char* message);
    void receive_message();
};

#endif
