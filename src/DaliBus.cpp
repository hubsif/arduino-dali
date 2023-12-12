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

#ifndef ARDUINO_ARCH_RP2040
  // wrapper for interrupt handler
void DaliBus_wrapper_pinchangeISR() { DaliBus.pinchangeISR(); }
#ifdef __time_critical_func
    void __isr __time_critical_func(DaliBus_wrapper_timerISR)()
#else
    void DaliBus_wrapper_timerISR() 
#endif
{ DaliBus.timerISR(); }
#else
#ifdef DALI_TIMER
RPI_PICO_Timer timer2(DALI_TIMER);
#endif
#ifdef __time_critical_func
    void __isr __time_critical_func(DaliBus_wrapper_pinchangeISR)()
#else
    void DaliBus_wrapper_pinchangeISR() 
#endif
{ DaliBus.pinchangeISR(); }
#endif

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

#ifdef ARDUINO_ARCH_RP2040
  attachInterrupt(digitalPinToInterrupt(rxPin), DaliBus_wrapper_pinchangeISR, CHANGE);

  #ifdef DALI_TIMER
  timer2.attachInterrupt(2398, [](repeating_timer *t) -> bool {
    DaliBus.timerISR();
    return true;
  });
  #endif
#else
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(rxPin), DaliBus_wrapper_pinchangeISR, CHANGE);

  // Timer setup
  Timer1.initialize(DALI_TE); // set timer to time of one half-bit
  Timer1.attachInterrupt(DaliBus_wrapper_timerISR);
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

void DaliBusClass::timerISR() {
  if (busIdleCount < 0xff) // increment idle counter avoiding overflow
    busIdleCount++;

  if (busIdleCount == 4 && getBusLevel() == LOW) { // bus is low idle for more than 2 TE, something's pulling down for too long
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
        setBusLevel(LOW);
      else
        setBusLevel(HIGH);
      busState = TX_BIT_2ND;
      break;
    case TX_BIT_2ND: // send bit (2nd half)
      if (txMessage[txPos >> 3] & 1 << (7 - (txPos & 0x7)))
        setBusLevel(HIGH);
      else
        setBusLevel(LOW);
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

void __time_critical_func(DaliBusClass::pinchangeISR)() {
  byte busLevel = getBusLevel(); // TODO: do we have to check if level actually changed?
  busIdleCount = 0;           // reset idle counter so timer knows that something's happening

  if (busState <= TX_STOP) {          // check if we are transmitting
#ifndef DALI_NO_COLLISSION_CHECK
    if (busLevel != txBusLevel) { // check for collision
      txCollision = 1;	           // signal collision
      logInfo("Dali", "collision %i", busState);
      timer2.restartTimer();
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

bool DaliBusClass::isDeltaWithinTE(unsigned long delta) {
  return (DALI_TE_MIN <= delta && delta <= DALI_TE_MAX);
}

bool DaliBusClass::isDeltaWithin2TE(unsigned long delta) {
  return (2*DALI_TE_MIN <= delta && delta <= 2*DALI_TE_MAX);
}

byte DaliBusClass::getBusLevel() {
  return (activeLow ? !digitalRead(rxPin) : digitalRead(rxPin));
}

void DaliBusClass::setBusLevel(byte level) {
  digitalWrite(txPin, (activeLow ? !level : level));
  txBusLevel = level;
}

DaliBusClass DaliBus;
