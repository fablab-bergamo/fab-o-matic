#include <cstdint>
#include <string>
#include <array>
#include "LCDWrapper.h"

template <uint8_t _COLS, uint8_t _ROWS>
LCDWrapper<_COLS, _ROWS>::LCDWrapper(uint8_t rs, uint8_t enable, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3) : 
  lcd(rs, enable, d0, d1, d2, d3), 
  backlight_pin(255),
  backlight_active_low(false),
  show_connection_status(false),
  connection_status(false)
{
  buffer.fill({0});
  current.fill({0});
}

template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::begin()
{
  this->lcd.begin(_ROWS, _COLS);
  this->lcd.createChar(0, this->antenna_char);
  this->lcd.createChar(1, this->connection_char);
  this->lcd.createChar(2, this->noconnection_char);
}

template <uint8_t _COLS, uint8_t _ROWS>
std::string LCDWrapper<_COLS, _ROWS>::convertSecondsToHHMMSS(unsigned long milliseconds)
{
  //! since something something does not support to_string we have to resort to ye olde cstring stuff
  char buffer[9];
  unsigned long seconds = milliseconds / 1000;
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", (int)(seconds / 3600), (int)((seconds % 3600) / 60), (int)(seconds % 60));

  std::string result(buffer);
  return {result};
}

template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::update(BoardStatus status, FabServer server, FabMember user, Machine machine)
{
  char buffer[conf::lcd::COLS];

  switch (status)
  {
  case BoardStatus::CLEAR:
    this->clear();
    break;
  case BoardStatus::FREE:
    this->setRow(0, server.isOnline() ? "Disponibile" : "OFFLINE");
    this->setRow(1, "Avvicina carta");
    break;
  case BoardStatus::ALREADY_IN_USE:
    this->setRow(0, "In uso da");
    this->setRow(1, machine.getActiveUser().getName());
    break;
  case BoardStatus::LOGGED_IN:
    this->setRow(0, "Inizio uso");
    this->setRow(1, user.getName());
    break;
  case BoardStatus::LOGIN_DENIED:
    this->setRow(0, "Negato");
    this->setRow(1, "Carta sconosciuta");
    break;
  case BoardStatus::LOGOUT:
    this->setRow(0, "Arrivederci");
    this->setRow(1, user.getName());
    break;
  case BoardStatus::CONNECTING:
    this->setRow(0, "Connecting");
    break;
  case BoardStatus::CONNECTED:
    this->setRow(0, "Connected");
    break;
  case BoardStatus::IN_USE:
    snprintf(buffer, sizeof(buffer), "Ciao %s", machine.getActiveUser().getName());
    this->setRow(0, buffer);
    this->setRow(1, this->convertSecondsToHHMMSS(machine.getUsageTime()));
    break;
  case BoardStatus::BUSY:
    this->setRow(0, "Busy");
    break;
  case BoardStatus::OFFLINE:
    this->setRow(0, "OFFLINE MODE");
    break;
  }
  this->update_chars();
}

template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::clear()
{
  this->lcd.clear();
}


template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::update_chars()
{
  if (this->needsUpdate())
  {
    this->lcd.clear();
    this->lcd.setCursor(0, 0);

    char why_arduino_has_not_implemented_liquidcrystal_print_from_char_array_yet[16];
    memcpy(why_arduino_has_not_implemented_liquidcrystal_print_from_char_array_yet, this->buffer[0], _COLS);
    
    this->lcd.print(why_arduino_has_not_implemented_liquidcrystal_print_from_char_array_yet);

    if (this->show_connection_status)
    {
      this->lcd.setCursor(14, 0);
      this->lcd.write((uint8_t)0);
      this->lcd.write(this->connection_status ? (uint8_t)1 : (uint8_t)2);
    }

    this->lcd.setCursor(0, 1);
    memcpy(why_arduino_has_not_implemented_liquidcrystal_print_from_char_array_yet, this->buffer[1], _COLS);
    this->lcd.print(why_arduino_has_not_implemented_liquidcrystal_print_from_char_array_yet);

    current = buffer;
  }
}

template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::setConnectionState(bool connected)
{
  this->connection_status = connected;
}

template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::showConnection(bool show)
{
  this->show_connection_status = show;
}

template <uint8_t _COLS, uint8_t _ROWS>
bool LCDWrapper<_COLS, _ROWS>::needsUpdate()
{
  if (this->current != this->buffer)
  {
    Serial.println("buffer dump:");
    for (auto i = 0; i < _ROWS; i++)
    {
      for (auto j = 0; j < _COLS; j++)
      {
        Serial.print(this->buffer[i][j]);
      }
      Serial.println();
      for (auto j = 0; j < _COLS; j++)
      {
        Serial.print(this->current[i][j]);
      }
      Serial.println();
      Serial.println();
    }

    return true;
  }
  return false;
}

template <uint8_t _COLS, uint8_t _ROWS>
void LCDWrapper<_COLS, _ROWS>::setRow(uint8_t row, std::string text)
{
  if (row < _ROWS)
  {
    this->buffer[row].fill({0});
    for (auto i = 0; i < text.length(); i++)
    {
      this->buffer[row][i] = text[i];
    }
  }
}
