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

#include "Dali.h"

/*    
    0- 31  arc power control commands
   32-143  configuration commands
  144-223  query commands
  224-255  application extended commands
  256-271  special commands
  272-287  extended special commands

  or:
  256-271  0b10100001
  272-287  0b11000001

  no address: >= 256
  repeat: 32-128(-143), 258, 259, 
*/

void DaliClass::begin(byte tx_pin, byte rx_pin, bool active_low) {
  DaliBus.begin(tx_pin, rx_pin, active_low);
}

void DaliClass::setCallback(EventHandlerReceivedDataFuncPtr callback)
{
  DaliBus.receivedCallback = callback;
}

void DaliClass::setActivityCallback(EventHandlerActivityFuncPtr callback)
{
  DaliBus.activityCallback = callback;
}

int DaliClass::sendRawWait(const byte * message, uint8_t bits, byte timeout) {
  unsigned long time = millis();
  int result;

  while (!DaliBus.busIsIdle())
    if (millis() - time > timeout) return DALI_READY_TIMEOUT;

  result = DaliBus.sendRaw(message, bits);

  while (!DaliBus.busIsIdle())
    if (millis() - time > timeout) return DALI_READY_TIMEOUT;

  return (result != DALI_SENT) ? result : DaliBus.getLastResponse();
}

byte * DaliClass::prepareCmd(byte * message, byte address, byte command, byte type, byte selector) {
  message[0] = type << 7;
  message[0] |= address << 1;
  message[0] |= selector;

  message[1] = command;
  return message;  
}

daliReturnValue DaliClass::sendArcBroadcast(byte value) {
  return sendArc(0xFF, value, 1);
}

daliReturnValue DaliClass::sendArc(byte address, byte value, byte addr_type) {
  byte message[2];
  return DaliBus.sendRaw(prepareCmd(message, address, value, addr_type, 0), 16);
}

daliReturnValue DaliClass::sendArcBroadcastWait(byte value, byte timeout) {
  return sendArcWait(0xFF, value, 1, timeout);
}

daliReturnValue DaliClass::sendArcWait(byte address, byte value, byte addr_type, byte timeout) {
  byte message[2];
  return (daliReturnValue)sendRawWait(prepareCmd(message, address, value, addr_type, 0), 16, timeout);
}

daliReturnValue DaliClass::sendCmdBroadcast(DaliCmd command) {
  return sendCmd(0xFF, command, 1);
}

daliReturnValue DaliClass::sendCmd(byte address, DaliCmd command, byte addr_type) {
  byte message[2];
  return DaliBus.sendRaw(prepareCmd(message, address, command, addr_type, 1), 16);
}

int DaliClass::sendCmdBroadcastWait(DaliCmd command, byte timeout) {
  return sendCmdWait(0xFF, command, 1, timeout);
}

int DaliClass::sendCmdWait(byte address, DaliCmd command, byte addr_type, byte timeout) {
  byte sendCount = (command > 32 && command < 143) ? 2 : 1; // config commands need to be sent twice

  byte message[2];
  int result;

  while (sendCount) {
    result = sendRawWait(prepareCmd(message, address, command, addr_type, 1), 2, timeout);
    if (result != DALI_RX_EMPTY) return result;
    sendCount--;
  }

  return result;
}

byte * DaliClass::prepareSpecialCmd(byte * message, word command, byte value) {
  message[0] = ((byte) command + 16) << 1; // convert command number
  message[0] |= 0b10000001;                // to special command byte

  message[1] = value;
  return message;
}

daliReturnValue DaliClass::sendSpecialCmd(DaliSpecialCmd cmd, byte value) {
  word command = static_cast<word>(cmd);
  if (command < 256 || command > 287) return DALI_INVALID_PARAMETER;
  byte message[2];
  return DaliBus.sendRaw(prepareSpecialCmd(message, command, value), 16);
}

int DaliClass::sendSpecialCmdWait(word command, byte value, byte timeout) {
  byte message[2];
  return sendRawWait(prepareSpecialCmd(message, command, value), 16);
}

#ifndef DALI_NO_COMMISSIONING
void DaliClass::commission(byte startAddress, bool onlyNew) {
  nextShortAddress = startAddress;
  commissionOnlyNew = onlyNew;
  
  // start commissioning
  commissionState = COMMISSION_INIT;
}

void DaliClass::commission_tick() {
  // TODO: set timeout for commissioning?
  // TODO: also clear group addresses?

  static byte searchIterations;
  static unsigned long currentSearchAddress;

  if (DaliBus.busIsIdle()) { // wait until bus is idle
    switch (commissionState) {
      case COMMISSION_INIT:
        sendSpecialCmd(DaliSpecialCmd::INITIALISE, (commissionOnlyNew ? 255 : 0));
        commissionState = COMMISSION_INIT2;
        break;
      case COMMISSION_INIT2:
        sendSpecialCmd(DaliSpecialCmd::INITIALISE, (commissionOnlyNew ? 255 : 0));
        commissionState = (commissionOnlyNew ? COMMISSION_RANDOM : COMMISSION_WRITE_DTR);
        break;
      case COMMISSION_WRITE_DTR:
        sendSpecialCmd(DaliSpecialCmd::SET_DTR, 255);
        commissionState = COMMISSION_REMOVE_SHORT;
        break;
      case COMMISSION_REMOVE_SHORT:
        sendCmd(63, DaliCmd::DTR_AS_SHORT, DaliAddressTypes::GROUP);
        commissionState = COMMISSION_REMOVE_SHORT2;
        break;
      case COMMISSION_REMOVE_SHORT2:
        sendCmd(63, DaliCmd::DTR_AS_SHORT, DaliAddressTypes::GROUP);
        commissionState = COMMISSION_RANDOM;
        break;
      case COMMISSION_RANDOM:
        sendSpecialCmd(DaliSpecialCmd::RANDOMISE);
        commissionState = COMMISSION_RANDOM2;
        break;
      case COMMISSION_RANDOM2:
        sendSpecialCmd(DaliSpecialCmd::RANDOMISE);
        commissionState = COMMISSION_RANDOMWAIT;
        break;
      case COMMISSION_RANDOMWAIT:  // wait 100ms for random address to generate
        if (DaliBus.busIdleCount >= 255)
          commissionState = COMMISSION_STARTSEARCH;
        break;
      case COMMISSION_STARTSEARCH:
        searchIterations = 0;
        currentSearchAddress = 0xFFFFFF;
      case COMMISSION_SEARCHHIGH:
        sendSpecialCmd(DaliSpecialCmd::SEARCHADDRH, (currentSearchAddress >> 16) & 0xFF);
        commissionState = COMMISSION_SEARCHMID;
        break;
      case COMMISSION_SEARCHMID:
        sendSpecialCmd(DaliSpecialCmd::SEARCHADDRM, (currentSearchAddress >> 8) & 0xFF);
        commissionState = COMMISSION_SEARCHLOW;
        break;
      case COMMISSION_SEARCHLOW:
        sendSpecialCmd(DaliSpecialCmd::SEARCHADDRL, (currentSearchAddress) & 0xFF);
        commissionState = COMMISSION_COMPARE;
        break;
      case COMMISSION_COMPARE:
        sendSpecialCmd(DaliSpecialCmd::COMPARE);
        commissionState = COMMISSION_CHECKFOUND;
        break;
      case COMMISSION_CHECKFOUND:
        {  // create scope for response variable
        int response = DaliBus.getLastResponse();
        if (response != DALI_RX_EMPTY)
          if (searchIterations >= 24) // ballast found
            commissionState = COMMISSION_PROGRAMSHORT;
          else {
            currentSearchAddress -= (0x800000 >> searchIterations);
            commissionState = COMMISSION_SEARCHHIGH;
          }
        else
          if (searchIterations == 0 || searchIterations > 24) // no device at all responded or error
            commissionState = COMMISSION_TERMINATE;
          else if (searchIterations == 24) {  // device responded before, but didn't now, so address is one higher
            currentSearchAddress++;           // and for the device to act at upcoming commands, we need to send the actual address
            commissionState = COMMISSION_SEARCHHIGH;
          }
          else {  // there's a device that didn't respond anymore, increase address
            currentSearchAddress += (0x800000 >> searchIterations);
            commissionState = COMMISSION_SEARCHHIGH;
          }
        searchIterations++;
        break;
        }
      case COMMISSION_PROGRAMSHORT:
        sendSpecialCmd(DaliSpecialCmd::PROGRAMSHORT, (nextShortAddress << 1) | 1);
        commissionState = COMMISSION_VERIFYSHORT;
        break;
      case COMMISSION_VERIFYSHORT:
        sendSpecialCmd(DaliSpecialCmd::VERIFYSHORT, (nextShortAddress << 1) | 1);
        commissionState = COMMISSION_VERIFYSHORTRESPONSE;
        break;
      case COMMISSION_VERIFYSHORTRESPONSE:
        if (DaliBus.getLastResponse() == 0xFF) {
          nextShortAddress++;
          commissionState = COMMISSION_WITHDRAW;
        } else
          // error, stop commissioning
          commissionState = COMMISSION_TERMINATE;
        break;
      case COMMISSION_WITHDRAW:
        sendSpecialCmd(DaliSpecialCmd::WITHDRAW);
        commissionState = COMMISSION_STARTSEARCH;
        break;
      case COMMISSION_TERMINATE:
        sendSpecialCmd(DaliSpecialCmd::TERMINATE);
        commissionState = COMMISSION_OFF;
        break;
    }
  }
}
#endif

#ifndef DALI_DONT_EXPORT
DaliClass Dali;
#endif
