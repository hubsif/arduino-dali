/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
*/

#include "DaliBus.h"

#ifdef DALI_TIMER
#if defined(ARDUINO_ARCH_RP2040)
RPI_PICO_Timer timer2(DALI_TIMER);
void __isr __time_critical_func(DaliBus_wrapper_pinchangeISR)() { DaliBus.pinchangeISR(); }
#elif defined(ARDUINO_ARCH_ESP32)
ESP32Timer timer2(DALI_TIMER);
void IRAM_ATTR DaliBus_wrapper_pinchangeISR() { DaliBus.pinchangeISR(); }
#elif defined(ARDUINO_ARCH_ESP8266)
ESP8266Timer timer2(DALI_TIMER);
void IRAM_ATTR DaliBus_wrapper_pinchangeISR() { DaliBus.pinchangeISR(); }
#elif defined(ARDUINO_ARCH_AVR)
  #if DALI_TIMER==1
  #define timer2 ITimer1
  #elif DALI_TIMER==2
  #define timer2 ITimer2
  #else
  #define timer2 ITimer3
  #endif
void DaliBus_wrapper_pinchangeISR() { DaliBus.pinchangeISR(); }
#endif
#endif

// static void gpio_toggle ()
// {
//   DaliBus.pinchangeISR();
//   gpio_acknowledge_irq(16, IO_IRQ_BANK0);
// }

void _gpioInterruptDispatcher3(uint gpio, uint32_t events) {
  DaliBus.pinchangeISR();
}

void DaliBusClass::begin(byte tx_pin, byte rx_pin, bool active_low) {
  txPin = tx_pin;
  rxPin = rx_pin;
  activeLow = active_low;

  // init bus state
  busState = IDLE;

  // TX pin setup
  pinMode(txPin, OUTPUT);
  setBusLevel(HIGH);

  // RX pin setup
  pinMode(rxPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(rxPin), DaliBus_wrapper_pinchangeISR, CHANGE);

  //gpio_set_irq_enabled_with_callback(rxPin, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, _gpioInterruptDispatcher3);

  // irq_set_exclusive_handler(IO_IRQ_BANK0, gpio_toggle);
  // gpio_set_irq_enabled(rxPin, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
  // irq_set_enabled(IO_IRQ_BANK0, true);

  #ifdef DALI_TIMER
  #if defined(ARDUINO_ARCH_RP2040)
  timer2.attachInterrupt(2398, [](repeating_timer *t) -> bool {
    DaliBus.timerISR();
    return true;
  });
  #elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  timer2.attachInterrupt(2398, +[](void * timer) -> bool {
    DaliBus.timerISR();
    return true;
  });
  #elif defined(ARDUINO_ARCH_AVR)
    timer2.init();
    timer2.attachInterrupt(2398, +[](unsigned int outputPin) {
      DaliBus.timerISR();
    });
  #endif
  #endif
}

daliReturnValue DaliBusClass::sendRaw(const byte * message, byte length) {
  if (length > 3) return DALI_INVALID_PARAMETER;
  if (busState != IDLE) return DALI_BUSY;

  // prepare variables for sending
  for (byte i = 0; i < length; i++)
    txMessage[i] = message[i];
  txLength = length * 8;
  txCollision = 0;
  rxMessage = DALI_RX_EMPTY;
  rxLength = 0;

  // initiate transmission
  busState = TX_START_1ST;
  return DALI_SENT;
}

bool DaliBusClass::busIsIdle() {
  return (busState == IDLE);
}

int DaliBusClass::getLastResponse() {
  int response;
  switch (rxLength) {
    case 16:
      response = rxMessage;
      break;
    case 0:
      response = DALI_RX_EMPTY;
      break;
    default:
      response = DALI_RX_ERROR;
  }
  rxLength = 0;
  return response;
}

#if defined(ARDUINO_ARCH_RP2040)
void __time_critical_func(DaliBusClass::timerISR()) {
#elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
void IRAM_ATTR DaliBusClass::timerISR() {
#elif defined(ARDUINO_ARCH_AVR)
void DaliBusClass::timerISR() {
#endif
  if (busIdleCount < 0xff) // increment idle counter avoiding overflow
    busIdleCount++;

  if (busIdleCount == 4 && getBusLevel == LOW) { // bus is low idle for more than 2 TE, something's pulling down for too long
    busState = SHORT;
    setBusLevel(HIGH);
    if(errorCallback != 0)
      errorCallback(DALI_PULLDOWN);
  }

  // timer state machine
  switch (busState) {
    case TX_START_1ST: // initiate transmission by setting bus low (1st half)
      if (busIdleCount >= 26) { // wait at least 9.17ms (22 TE) settling time before sending (little more for TCI compatibility)
        setBusLevel(LOW);
        busState = TX_START_2ND;
      }
      break;
    case TX_START_2ND: // send start bit (2nd half)
      setBusLevel(HIGH);
      txPos = 0;
      busState = TX_BIT_1ST;
      break;
    case TX_BIT_1ST: // prepare bus for bit (1st half)
      if (txMessage[txPos >> 3] & 1 << (7 - (txPos & 0x7)))
      {
        setBusLevel(LOW);
      } else {
        setBusLevel(HIGH);
      }
      busState = TX_BIT_2ND;
      break;
    case TX_BIT_2ND: // send bit (2nd half)
      if (txMessage[txPos >> 3] & 1 << (7 - (txPos & 0x7)))
      {
        setBusLevel(HIGH);
      } else {
        setBusLevel(LOW);
      }
      txPos++;
      if (txPos < txLength)
        busState = TX_BIT_1ST;
      else
        busState = TX_STOP_1ST;
      break;
    case TX_STOP_1ST: // 1st stop bit (1st half)
      setBusLevel(HIGH);
      busState = TX_STOP;
      break;
    case TX_STOP: // remaining stop half-bits
      if (busIdleCount >= 4) {
        busState = WAIT_RX;
        busIdleCount = 0;
      }   
      break;
    case WAIT_RX: // wait 9.17ms (22 TE) for a response
      if (busIdleCount > 23)
        busState = IDLE; // response timed out
      break;
    case RX_STOP:
      if (busIdleCount > 4) {
        // rx message incl stop bits finished. 
        busState = IDLE;
      }
      break;
    case RX_START:
    case RX_BIT:
      if (busIdleCount > 3) // bus has been inactive for too long
      {
        busState = IDLE;    // rx has been interrupted, bus is idle
        if(rxLength > 16)
        {
          if(receivedCallback != 0)
          {
            uint8_t len = (rxLength - (rxLength % 2)) / (2*8);
            if(len > 1)
            {
              uint8_t *data = new uint8_t[len];
              for(int i = 0; i < len; i++)
                data[i] = (rxCommand >> ((len-1-i)*8)) & 0xFF;
              receivedCallback(data, len);
              delete[] data;
            } else {
              receivedCallback((uint8_t*)&rxCommand, 1);
            }
          }
        }
      }
      break;
  }
}

#if defined(ARDUINO_ARCH_RP2040)
void __not_in_flash_func(DaliBusClass::pinchangeISR)() {
#elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
void IRAM_ATTR DaliBusClass::pinchangeISR() {
#elif defined(ARDUINO_ARCH_AVR)
void DaliBusClass::pinchangeISR() {
#endif
  byte busLevel = getBusLevel; // TODO: do we have to check if level actually changed?
  busIdleCount = 0;           // reset idle counter so timer knows that something's happening

  if(busLevel != 0 && activityCallback != 0)
    activityCallback();

  if (busState <= TX_STOP) {          // check if we are transmitting
#ifndef DALI_NO_COLLISSION_CHECK
    if (busLevel != txBusLevel) { // check for collision
      txCollision = 1;	           // signal collision
      if(errorCallback != 0)
        errorCallback(DALI_COLLISION);
      #ifdef DALI_TIMER
      timer2.restartTimer();
      #endif
      busState = IDLE;	               // stop transmission
    }
#endif
    return;                        // no collision, ignore pin change
  }

  // logical bus level changed -> store timings
  unsigned long tmp_ts = micros();
  unsigned long delta = tmp_ts - rxLastChange; // store delta since last change
  rxLastChange = tmp_ts;                       // store timestamp

  // rx state machine
  switch (busState) {
    case WAIT_RX:
      if (busLevel == LOW) { // start of rx frame
        //Timer1.restart();    // sync timer
        #ifdef DALI_TIMER
        timer2.restartTimer();
        #endif
        busState = RX_START;
        rxIsResponse = true;
      } else {
        busState = IDLE; // bus can't actually be high, reset
        if(errorCallback != 0)
          errorCallback(DALI_CANT_BE_HIGH);
      }
      break;
    case RX_START:
      if (busLevel == HIGH && isDeltaWithinTE(delta)) { // validate start bit
        rxLength = 0; // clear old rx message
        rxMessage = 0;
        busState = RX_BIT;
      } else {                                   // invalid start bit -> reset bus state
        tempBusLevel = busLevel;
        tempDelta = delta;
        rxLength = DALI_RX_ERROR;
        busState = RX_STOP;
        if(errorCallback != 0)
          errorCallback(DALI_INVALID_STARTBIT);
      }
      break;
    case RX_BIT:
      if (isDeltaWithinTE(delta)) {              // check if change is within time of a half-bit
        if (rxLength % 2)                        // if rxLength is odd (= actual bit change)
        {
          if(rxIsResponse)
            rxMessage = rxMessage << 1 | busLevel;   // shift in received bit
          else
            rxCommand = rxCommand << 1 | busLevel;
        }
        rxLength++;
      } else if (isDeltaWithin2TE(delta)) {       // check if change is within time of two half-bits
        if(rxIsResponse)
          rxMessage = rxMessage << 1 | busLevel;   // shift in received bit
        else
          rxCommand = rxCommand << 1 | busLevel;
        rxLength += 2;
      } else {
        rxLength = DALI_RX_ERROR;
        busState = RX_STOP; // timing error -> reset state
        tempDelta = delta;
        if(errorCallback != 0)
          errorCallback(DALI_ERROR_TIMING);
      }
      if (rxIsResponse && rxLength == 16) // check if all 8 bits have been received
        busState = RX_STOP;
      break;
    case SHORT:
      if (busLevel == HIGH)
        busState = IDLE; // recover from bus error
      break;
    case IDLE:
      if(busLevel == LOW) {
        busState = RX_START;
        rxIsResponse = false;
      }
      break;  // ignore, we didn't expect rx
  }
}

DaliBusClass DaliBus;
