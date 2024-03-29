#ifndef LED_HPP_
#define LED_HPP_

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <memory>
#include <optional>

#include "pins.hpp"

namespace fablabbg
{
  class Led
  {
  public:
    enum class Status : uint8_t
    {
      Off,
      On,
      Blinking,
    };

    Led() = default;

    void set(Status status);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void update();

  private:
    std::optional<Adafruit_NeoPixel> pixel;
    std::array<uint8_t, 3> color{128, 128, 128};
    Status status{Status::On};
    bool isOn{false};
    bool initialized{false};
    void outputColor(uint8_t r, uint8_t g, uint8_t b);
    void init();
  };

} // namespace fablabbg

#endif // LED_HPP_