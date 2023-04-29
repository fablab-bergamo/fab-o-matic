#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "FabServer.h"
#include "Machine.h"
#include "LCDWrapper.h"
#include "conf.h"
#include "RFIDWrapper.h"
#include "pins.h"
#include "secrets.h"
#include "AuthProvider.h"
#include "BoardLogic.h"
#include <TaskScheduler.h>

// Global variables
namespace Board
{
  RFIDWrapper rfid;
  LCDWrapper<conf::lcd::COLS, conf::lcd::ROWS> lcd(pins.lcd);

  FabServer server(secrets::wifi::ssid, secrets::wifi::password, secrets::mqtt::server);

  const Machine::Config config1(secrets::machine::machine_id,
                                secrets::machine::machine_type,
                                secrets::machine::machine_name,
                                pins.relay.ch1_pin, false);

  Machine machine(config1);
  AuthProvider auth(secrets::cards::whitelist);
  BoardLogic logic;
} // namespace Board

namespace Tasks
{
  Scheduler ts;
}

#endif // GLOBALS_H_