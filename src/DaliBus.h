#pragma once

/***********************************************************************
 * This library is free software; you can redistribute it and/or       *
 * modify it under the terms of the GNU Lesser General Public          *
 * License as published by the Free Software Foundation; either        *
 * version 2.1 of the License, or (at your option) any later version.  *
 *                                                                     *
 * This library is distributed in the hope that it will be useful,     *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of      *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU   *
 * Lesser General Public License for more details.                     *
 *                                                                     *
 * You should have received a copy of the GNU Lesser General Public    *
 * License along with this library; if not, write to the Free Software *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,          *
 * MA 02110-1301  USA                                                  *
 ***********************************************************************/

/**
 * @file DaliBus.h
 * @brief DALI low level interface
 *
 * This file contains the low level part of the Dali library
 *
 * @author Hubert Nusser
 * @date 2019-05-14
 */

#include "Arduino.h"

#include "TimerInterrupt_Generic.h"

#ifndef DALI_NO_TIMER
  #ifndef DALI_TIMER
    #warning DALI_TIMER not set; default will be set (0)
    #define DALI_TIMER 0
  #endif
  #ifdef ARDUINO_ARCH_RP2040
  #if DALI_TIMER < 0 || DALI_TIMER > 3
    #error TIMER has invalid value (valid values: 0-3)
  #endif
  #elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  #if DALI_TIMER < 0 || DALI_TIMER > 1
    #error TIMER has invalid value (valid values: 0-1)
  #endif
  #elif defined(ARDUINO_ARCH_AVR)
  #if DALI_TIMER < 1 || DALI_TIMER > 3
    #error TIMER has invalid value (valid values: 1-3)
  #endif
  #endif
#else
  #warning DALI_TIMER not set; make sure to call DaliBusClass::timerISR
#endif

const int DALI_BAUD = 1200;
const unsigned long DALI_TE = 417;
const unsigned long DALI_TE_MIN = ( 50 * DALI_TE) / 100;                 // 333us
const unsigned long DALI_TE_MAX = (150 * DALI_TE) / 100;                 // 500us

#define isDeltaWithinTE(delta) (DALI_TE_MIN <= delta && delta <= DALI_TE_MAX)
#define isDeltaWithin2TE(delta) (2*DALI_TE_MIN <= delta && delta <= 2*DALI_TE_MAX)
#if defined(ARDUINO_ARCH_RP2040)
  #define getBusLevel (activeLow ? !gpio_get(rxPin) : gpio_get(rxPin))
  #define setBusLevel(level) gpio_put(txPin, (activeLow ? !level : level)); txBusLevel = level;
#else
  #define getBusLevel (activeLow ? !digitalRead(rxPin) : digitalRead(rxPin))
  #define setBusLevel(level) digitalWrite(txPin, (activeLow ? !level : level)); txBusLevel = level;
#endif

/** some enum */
typedef enum daliReturnValue {
  DALI_NO_ERROR = 0,
  DALI_RX_EMPTY = -1,
  DALI_RX_ERROR = -2,
  DALI_SENT = -3,
  DALI_INVALID_PARAMETER = -4,
  DALI_BUSY = -5,
  DALI_READY_TIMEOUT = -6,
  DALI_SEND_TIMEOUT = -7,
  DALI_COLLISION = -8,
  DALI_PULLDOWN = -9,
  DALI_CANT_BE_HIGH = -10,
  DALI_INVALID_STARTBIT = -11,
  DALI_ERROR_TIMING = -12,
} daliReturnValue;

typedef void (*EventHandlerReceivedDataFuncPtr)(uint8_t *data, uint8_t len);
typedef void (*EventHandlerActivityFuncPtr)();
typedef void (*EventHandlerErrorFuncPtr)(daliReturnValue errorCode);

class DaliBusClass {
  public:
    void begin(byte tx_pin, byte rx_pin, bool active_low = true);
    daliReturnValue sendRaw(const byte * message, byte length);

    int getLastResponse();

    bool busIsIdle();
    volatile byte busIdleCount;

    void timerISR();
    void pinchangeISR();
    EventHandlerReceivedDataFuncPtr receivedCallback;
    EventHandlerActivityFuncPtr activityCallback;
    EventHandlerErrorFuncPtr errorCallback;

    //TODO remove temp
    bool tempBusLevel = false;
    uint16_t tempDelta = 0;

  protected:
    byte txPin, rxPin;
    bool activeLow;
    byte txMessage[3];
    byte txLength;

    enum busStateEnum {
      TX_START_1ST, TX_START_2ND,
      TX_BIT_1ST, TX_BIT_2ND,
      TX_STOP_1ST, TX_STOP,
      IDLE,
      SHORT,
      WAIT_RX, RX_START, RX_BIT, RX_STOP
    };
    volatile busStateEnum busState;
    volatile byte txPos;
    volatile byte txBusLevel;
    volatile byte txCollision;

    volatile unsigned long rxLastChange;
    volatile byte rxMessage;
    volatile uint32_t rxCommand;
    volatile char rxLength;
    volatile char rxError;
    volatile bool rxIsResponse = false;
};

extern DaliBusClass DaliBus;
