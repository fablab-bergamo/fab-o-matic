#include "Machine.hpp"
#include "Arduino.h"
#include <sstream>
#include <cstdint>
#include <type_traits>
#include <chrono>
#include "pins.hpp"
#include "secrets.hpp"
#include "FabServer.hpp"

namespace fablabbg
{
  using namespace std::chrono_literals;
  using namespace std::chrono;

  /// @brief Creates a new machine
  /// @param user_conf configuration of the machine
  Machine::Machine(MachineConfig user_conf, FabServer &serv) : maintenanceNeeded(false), allowed(true),
                                                               config(user_conf), server(serv),
                                                               active(false),
                                                               power_state(PowerState::UNKNOWN)

  {
    this->current_user = FabUser();

    if (this->config.hasRelay())
    {
      pinMode(this->config.relay_config.pin, OUTPUT);
    }
  }

  /// @brief Returns the machine identifier
  /// @return Machine identifier
  MachineID Machine::getMachineId() const
  {
    return this->config.machine_id;
  }

  /// @brief Indicates whether the machine is used by somebody
  /// @return boolean
  bool Machine::isFree() const
  {
    return !this->active;
  }

  /// @brief Log the given user onto the machine, if free and not blocked
  /// @param user user to login
  /// @return true of the user has been successfully logged in
  bool Machine::login(FabUser user)
  {
    if (this->isFree() && this->allowed)
    {
      this->active = true;
      this->current_user = user;
      this->power(true);
      this->usage_start_timestamp = system_clock::now();
      return true;
    }
    return false;
  }

  /// @brief Returns the current power state of the machine
  /// @return
  Machine::PowerState Machine::getPowerState() const
  {
    return this->power_state;
  }

  /// @brief Removes the user from the machine, and powers off the machine (respecting POWEROFF_DELAY_MINUTES setting)
  void Machine::logout()
  {
    if (this->active)
    {
      this->active = false;
      this->power_state = PowerState::WAITING_FOR_POWER_OFF;
      this->usage_start_timestamp = std::nullopt;

      // Sets the countdown to power off
      if (conf::machine::POWEROFF_GRACE_PERIOD > 0s)
      {
        this->logout_timestamp = system_clock::now();
        if (conf::debug::ENABLE_LOGS)
          Serial.printf("Machine will be shutdown in %lld s\r\n",
                        duration_cast<seconds>(conf::machine::POWEROFF_GRACE_PERIOD).count());
      }
      else
      {
        this->logout_timestamp = system_clock::now();
        this->power(false);
      }
    }
  }

  /// @brief indicates if the machine can be powered off
  /// @return true if the delay has expired
  bool Machine::canPowerOff() const
  {
    if (!this->logout_timestamp.has_value())
      return false;

    return (this->power_state == PowerState::WAITING_FOR_POWER_OFF &&
            system_clock::now() - this->logout_timestamp.value() > conf::machine::POWEROFF_GRACE_PERIOD);
  }

  /// @brief indicates if the machine is about to shudown and board should beep
  /// @return true if shutdown is imminent
  bool Machine::isShutdownImminent() const
  {
    if (!this->logout_timestamp.has_value() || conf::machine::BEEP_PERIOD == 0ms)
      return false;

    return (this->power_state == PowerState::WAITING_FOR_POWER_OFF &&
            system_clock::now() - this->logout_timestamp.value() > conf::machine::BEEP_PERIOD);
  }

  /// @brief sets the machine power to on (true) or off (false)
  /// @param value setpoint
  void Machine::power_relay(bool value)
  {
    if (conf::debug::ENABLE_LOGS)
      Serial.printf("Machine::power_relay : power set to %d\r\n", value);

    if (this->config.relay_config.active_low)
    {
      digitalWrite(this->config.relay_config.pin, value ? LOW : HIGH);
    }
    else
    {
      digitalWrite(this->config.relay_config.pin, value ? HIGH : LOW);
    }

    if (value)
    {
      this->logout_timestamp = std::nullopt;
      this->power_state = PowerState::POWERED_ON;
    }
    else
    {
      this->power_state = PowerState::POWERED_OFF;
    }
  }

  /// @brief sets the machine power to on (true) or off (false)
  /// @param value setpoint
  void Machine::power_mqtt(bool value)
  {
    if (conf::debug::ENABLE_LOGS)
      Serial.printf("Machine::power_mqtt : power set to %d\r\n", value);

    String topic{this->config.mqtt_config.topic.data()};
    String payload = value ? this->config.mqtt_config.on_message.data() : this->config.mqtt_config.off_message.data();

    auto retries = 0;
    while (!server.publish(topic, payload))
    {
      if (conf::debug::ENABLE_LOGS)
        Serial.printf("Error while publishing %s to %s\r\n", payload.c_str(), topic.c_str());

      server.connect();
      delay(500);
      retries++;
      if (retries > 5)
      {
        Serial.println("Unable to publish to MQTT");
        return;
      }
    }

    if (value)
    {
      this->logout_timestamp = std::nullopt;
      this->power_state = PowerState::POWERED_ON;
    }
    else
    {
      this->power_state = PowerState::POWERED_OFF;
    }
  }

  void Machine::power(bool on_or_off)
  {
    if (conf::debug::ENABLE_LOGS)
      Serial.printf("Machine::power : power set to %d\r\n", on_or_off);

    if (this->config.hasRelay())
    {
      this->power_relay(on_or_off);
    }
    if (this->config.hasMqttSwitch())
    {
      this->power_mqtt(on_or_off);
    }
  }

  FabUser &Machine::getActiveUser()
  {
    return this->current_user;
  }

  /// @brief Gets the duration the machine has been used
  /// @return milliseconds since the machine has been started
  seconds Machine::getUsageDuration() const
  {
    if (this->usage_start_timestamp.has_value())
    {
      return duration_cast<seconds>(system_clock::now() - this->usage_start_timestamp.value());
    }
    return 0s;
  }

  bool Machine::operator==(const Machine &v) const
  {
    return (this->config.machine_id.id == v.config.machine_id.id);
  }

  bool Machine::operator!=(const Machine &v) const
  {
    return (this->config.machine_id.id != v.config.machine_id.id);
  }

  std::string Machine::getMachineName() const
  {
    return this->config.machine_name;
  }

  std::string Machine::toString() const
  {
    std::stringstream sstream;

    sstream << "Machine (ID:" << this->getMachineId().id;
    sstream << ", Name:" << this->getMachineName();
    sstream << ", IsFree: " << this->isFree();
    sstream << ", IsAllowed:" << this->allowed;
    sstream << ", PowerState:" << static_cast<int>(this->getPowerState());
    sstream << ", " << this->current_user.toString();
    sstream << ", UsageDuration (s):" << this->getUsageDuration().count();
    sstream << ", ShutdownImminent:" << this->isShutdownImminent();
    sstream << ", MaintenanceNeeded:" << this->maintenanceNeeded;
    sstream << ", " << this->config.toString();
    sstream << ")";

    return sstream.str();
  }

  std::chrono::minutes Machine::getAutologoffDelay() const
  {
    return this->config.autologoff;
  }

  void Machine::setAutologoffDelay(std::chrono::minutes new_delay)
  {
    if (conf::debug::ENABLE_LOGS && this->config.autologoff != new_delay)
      Serial.printf("Setting autologoff delay to %lld min\r\n", new_delay.count());

    this->config.autologoff = new_delay;
  }

  bool Machine::isAutologoffExpired() const
  {
    return this->getAutologoffDelay() > 0min &&
           this->getUsageDuration() > this->getAutologoffDelay();
  }
} // namespace fablabbg