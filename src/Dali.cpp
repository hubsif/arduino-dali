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

void DaliClass::begin(byte tx_pin, byte rx_pin, bool active_low = true) {
  DaliBus.begin(tx_pin, rx_pin, active_low);
}

void DaliClass::setCallback(EventHandlerReceivedDataFuncPtr callback)
{
  DaliBus.receivedCallback = callback;
}

int DaliClass::sendRawWait(const byte * message, byte length, byte timeout) {
  unsigned long time = millis();
  int result;

  while (!DaliBus.busIsIdle())
    if (millis() - time > timeout) return DALI_READY_TIMEOUT;

  result = DaliBus.sendRaw(message, length);

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
  return DaliBus.sendRaw(prepareCmd(message, address, value, addr_type, 0), 2);
}

daliReturnValue DaliClass::sendArcBroadcastWait(byte value, byte timeout) {
  return sendArcWait(0xFF, value, 1, timeout);
}

daliReturnValue DaliClass::sendArcWait(byte address, byte value, byte addr_type, byte timeout) {
  byte message[2];
  return sendRawWait(prepareCmd(message, address, value, addr_type, 0), 2, timeout);
}

daliReturnValue DaliClass::sendCmdBroadcast(byte command) {
  return sendCmd(0xFF, command, 1);
}

daliReturnValue DaliClass::sendCmd(byte address, byte command, byte addr_type) {
  byte message[2];
  return DaliBus.sendRaw(prepareCmd(message, address, command, addr_type, 1), 2);
}

int DaliClass::sendCmdBroadcastWait(byte command, byte timeout) {
  return sendCmdWait(0xFF, command, 1, timeout);
}

int DaliClass::sendCmdWait(byte address, byte command, byte addr_type, byte timeout) {
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

daliReturnValue DaliClass::sendSpecialCmd(word command, byte value = 0) {
  if (command < 256 || command > 287) return 1;
  byte message[2];
  return DaliBus.sendRaw(prepareSpecialCmd(message, command, value), 2);
}

int DaliClass::sendSpecialCmdWait(word command, byte value = 0, byte timeout = 50) {
  byte message[2];
  return sendRawWait(prepareSpecialCmd(message, command, value), 2);
}

void DaliClass::commission(byte startAddress = 0, bool onlyNew = false) {
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
        sendSpecialCmd(CMD_INITIALISE, (commissionOnlyNew ? 255 : 0));
        commissionState = COMMISSION_INIT2;
        break;
      case COMMISSION_INIT2:
        sendSpecialCmd(CMD_INITIALISE, (commissionOnlyNew ? 255 : 0));
        commissionState = (commissionOnlyNew ? COMMISSION_RANDOM : COMMISSION_WRITE_DTR);
        break;
      case COMMISSION_WRITE_DTR:
        sendSpecialCmd(CMD_SET_DTR, 255);
        commissionState = COMMISSION_REMOVE_SHORT;
        break;
      case COMMISSION_REMOVE_SHORT:
        sendCmd(63, CMD_DTR_AS_SHORT, DALI_GROUP_ADDRESS);
        commissionState = COMMISSION_REMOVE_SHORT2;
        break;
      case COMMISSION_REMOVE_SHORT2:
        sendCmd(63, CMD_DTR_AS_SHORT, DALI_GROUP_ADDRESS);
        commissionState = COMMISSION_RANDOM;
        break;
      case COMMISSION_RANDOM:
        sendSpecialCmd(CMD_RANDOMISE);
        commissionState = COMMISSION_RANDOM2;
        break;
      case COMMISSION_RANDOM2:
        sendSpecialCmd(CMD_RANDOMISE);
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
        sendSpecialCmd(CMD_SEARCHADDRH, (currentSearchAddress >> 16) & 0xFF);
        commissionState = COMMISSION_SEARCHMID;
        break;
      case COMMISSION_SEARCHMID:
        sendSpecialCmd(CMD_SEARCHADDRM, (currentSearchAddress >> 8) & 0xFF);
        commissionState = COMMISSION_SEARCHLOW;
        break;
      case COMMISSION_SEARCHLOW:
        sendSpecialCmd(CMD_SEARCHADDRL, (currentSearchAddress) & 0xFF);
        commissionState = COMMISSION_COMPARE;
        break;
      case COMMISSION_COMPARE:
        sendSpecialCmd(CMD_COMPARE);
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
        sendSpecialCmd(CMD_PROGRAMSHORT, (nextShortAddress << 1) | 1);
        commissionState = COMMISSION_VERIFYSHORT;
        break;
      case COMMISSION_VERIFYSHORT:
        sendSpecialCmd(CMD_VERIFYSHORT, (nextShortAddress << 1) | 1);
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
        sendSpecialCmd(CMD_WITHDRAW);
        commissionState = COMMISSION_STARTSEARCH;
        break;
      case COMMISSION_TERMINATE:
        sendSpecialCmd(CMD_TERMINATE);
        commissionState = COMMISSION_OFF;
        break;
    }
  }
}

DaliClass Dali;
