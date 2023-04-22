#include "RFIDWrapper.h"
#include "pins.h"
#include "MFRC522v2.h"
#include "MFRC522DriverSPI.h"
#include "MFRC522DriverPinSimple.h"
#include "MFRC522Debug.h"
#include "conf.h"
#include "card.h"
#include <memory>

RFIDWrapper::RFIDWrapper()
{
    // Configure SPI bus
    SPI.begin(pins.mfrc522.sck_pin, pins.mfrc522.miso_pin, pins.mfrc522.mosi_pin, pins.mfrc522.sda_pin);

    // Smart pointers members destructors will run & free the memory, when class will be distructed.
    this->rfid_simple_driver = std::unique_ptr<MFRC522DriverPinSimple>(new MFRC522DriverPinSimple(pins.mfrc522.sda_pin));
    this->spi_rfid_driver = std::unique_ptr<MFRC522DriverSPI>(new MFRC522DriverSPI(*this->rfid_simple_driver)); // Create SPI driver.
    this->mfrc522 = std::unique_ptr<MFRC522>(new MFRC522(*this->spi_rfid_driver));
}

bool RFIDWrapper::isNewCardPresent() const
{
    auto result = this->mfrc522->PICC_IsNewCardPresent();

    if (conf::debug::DEBUG && result)
        Serial.printf("isNewCardPresent=%d\n", result);

    return result;
}

bool RFIDWrapper::readCardSerial() const
{
    auto result = this->mfrc522->PICC_ReadCardSerial();

    if (conf::debug::DEBUG)
    {
        Serial.printf("readCardSerial=%d (SAK=%d, Size=%d)\n", result, this->mfrc522->uid.sak, this->mfrc522->uid.size);
    }

    return result;
}

bool RFIDWrapper::cardStillThere(const card::uid_t original) const
{
    for (auto i = 0; i < 3; i++)
    {
        // Detect Tag without looking for collisions
        byte bufferATQA[2];
        byte bufferSize = sizeof(bufferATQA);

        MFRC522::StatusCode result = this->mfrc522->PICC_WakeupA(bufferATQA, &bufferSize);

        if (result == MFRC522::StatusCode::STATUS_OK)
        {
            if (this->readCardSerial() && this->getUid() == original)
                return true;
        }
        delay(5);
    }
    return false;
}

/// @brief Transforms the RFID acquired bytes into a uid_id object
/// @return card ID
card::uid_t RFIDWrapper::getUid() const
{
    uint8_t arr[conf::whitelist::UID_BYTE_LEN] {0};
    
    memcpy(arr, this->mfrc522->uid.uidByte, std::min(conf::whitelist::UID_BYTE_LEN, this->mfrc522->uid.size));

    auto c = card::from_array(arr);

    if (conf::debug::DEBUG)
    {
        auto str_id = card::uid_str(c);
        Serial.printf("getUid=%s\n", str_id.c_str());
    }

    return c;
}

/// @brief Initializes RFID chip including self test
bool RFIDWrapper::init() const
{
    if (conf::debug::DEBUG)
    {
        char buffer[80] = {0};
        sprintf(buffer, "Configuring SPI RFID (SCK=%d, MISO=%d, MOSI=%d, SDA=%d)", pins.mfrc522.sck_pin, pins.mfrc522.miso_pin, pins.mfrc522.mosi_pin, pins.mfrc522.sda_pin);
        Serial.println(buffer);
    }

    if (!this->mfrc522->PCD_Init())
    {
        Serial.println("mfrc522 Init failed");
        return false;
    }

    if (conf::debug::DEBUG)
        MFRC522Debug::PCD_DumpVersionToSerial(*this->mfrc522, Serial);

    this->mfrc522->PCD_SetAntennaGain(MFRC522::PCD_RxGain::RxGain_max);

    if (!this->mfrc522->PCD_PerformSelfTest())
    {
        Serial.println("Self-test failure for RFID");
        return false;
    }
    return true;
}