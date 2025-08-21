/** @file dali_blink.ino
 *  simple DALI library usage example
 */
#include "Arduino.h"
#include "../src/Dali.h"

uint8_t buffer[256];
uint8_t currentMessage = 0;
uint8_t currentPosition = 0;

// IRAM_ATTR for ESP32
// __time_critical_func for rp2040
void DaliCommand(uint8_t *data, uint8_t len)
{
    uint8_t localMessage = currentMessage;
    // move currentMessage pointer forward
    currentMessage += 4;

    // store the length at the current position
    buffer[localMessage] = len;

    // manually copy the data (since only 3 bytes, this is efficient)
    buffer[localMessage + 1] = data[0];
    buffer[localMessage + 2] = data[1];
    buffer[localMessage + 3] = data[2];
}

void setup() {
    // start serial
    Serial.begin(115200);
    // reset buffer with zeros
    memset(buffer, 0, 256);
    // begin dali
    Dali.begin(2, 3);
    // register callback for incoming commands (and also answers)
    Dali.setCallback(DaliCommand);
}

void loop() {
    // return if bus is busy
    if(!DaliBus.busIsIdle())
        return;
    // return if we have no new commands in buffer
    if (buffer[currentPosition] == 0)
        return;
    
    // Print dali command to serial
    Serial.printf("recv dali: %i - %.2X %.2X %.2X\n", buffer[currentPosition], buffer[currentPosition + 1], buffer[currentPosition + 2], buffer[currentPosition + 3]);
    
    // reset current position
    buffer[currentPosition] = 0;
    // move to next position in buffer
    currentPosition += 4;
}
