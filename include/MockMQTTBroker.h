#ifdef WOKWI_SIMULATION
#ifndef MOCKMQTTBROKER_H_
#define MOCKMQTTBROKER_H_

#include "sMQTTBroker.h"

class MockMQTTBroker : public sMQTTBroker
{
private:
  constexpr static uint16_t MQTTPORT = 1883;
  bool is_running = false;
  std::string topic = "";
  std::string payload = "";

public:
  MockMQTTBroker();
  ~MockMQTTBroker() = default;

  bool isRunning() const;
  void start();
  bool onEvent(sMQTTEvent *event);
  std::string fakeReply() const;
};

#endif // MOCKMQTTBROKER_H_
#endif // WOKWI_SIMULATION