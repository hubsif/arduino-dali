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

#ifdef ARDUINO_ARCH_RP2040
#include "TimerInterrupt_Generic.h"
#ifndef DALI_TIMER
  #ifndef __DALI_TIMER_WARNING
    #define __DALI_TIMER_WARNING
    #warning DALI_TIMER not set; make sure to call DaliBusClass::timerISR
  #endif
#else
  #if DALI_TIMER < 0 || DALI_TIMER > 3
    #error TIMER has invalid value (valid values: 0-3)
  #endif
#endif
#else
// TimerOne library for tx timer
#include "TimerOne.h"
// PinChangeInterrupt library for rx interrupt
#include "PinChangeInterrupt.h"
#endif

const int DALI_BAUD = 1200;
const unsigned long DALI_TE = 417;
const unsigned long DALI_TE_MIN = 217;                 // 333us
const unsigned long DALI_TE_MAX = 617;                 // 500us


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
    EventHandlerErrorFuncPtr errorCallback;

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

    bool isDeltaWithinTE(unsigned long delta);
    bool isDeltaWithin2TE(unsigned long delta);
    byte getBusLevel();
    void setBusLevel(byte level);
};

extern DaliBusClass DaliBus;
