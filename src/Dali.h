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
 * @mainpage
 * This library allows you to create a DALI controller/gateway with an Arduino device.
 * It supports sending commands, receiving responses and commissioning devices.
 * It requires the TimerOne library for transmission and the PinChangeInterrupt library
 * for reception. These libraries need to be installed with the Library Manager in the
 * Arduino IDE.
 *
 * @par Simple example
 * @include dali_blink.ino
 *
 * @par Changelog
 * - v0.0.3 (2022-09-19) Make commissioning state public
 * - v0.0.2 (2019-05-14) Initial release
 *
 * @author hubsif <hubsif@gmx.de>
 * @example dali_blink.ino
 *
 * @file Dali.h
 * @brief Main header file of the DALI library for Arduino
 */


#ifndef DALI_H
#define DALI_H

#include <arduino.h>
#include "DaliBus.h"
#include "DaliCommands.h"

/**
 * DALI library base class.
 */
class DaliClass {
  public:

    /** Start the DALI bus
      * @param tx_pin       Pin to use for transmission
      * @param rx_pin       Pin to use for reception. Must support Pin Change Interrupt.
      * @param active_low  set to false if bus is active low
      *
      * Initialize the hardware for DALI usage (i.e. set pin modes, timer and interrupts). By default the bus is
      * driven active-low, meaning with the µC tx pin being low the DALI bus will be high (idle). For transmission
      * the µC pin will be set high, which will pull the DALI voltage low. This behaviour
      * is used by most DALI hardware interfaces. The same logic applies to the rx pin. */
    void begin(byte tx_pin, byte rx_pin, bool active_low = true);

    /** Send a direct arc level command
      * @param  address    destination address
      * @param  value      arc level
      * @param  addr_type  address type (short/group)
      * @return ::daliReturnValue 
      *
      * This methods sends a "direct arc power control command" to the bus
      * It doesn't check if the bus is ready and returns immediately, stating if transmission could be
      * initiated through its response value ::daliReturnValue. */
    daliReturnValue sendArc(byte address, byte value, byte addr_type = DaliAddressTypes::SHORT);
    daliReturnValue sendArcBroadcast(byte value);

    /** Send a direct arc level command and wait for its completion
      * @param  address    destination address
      * @param  value      arc level
      * @param  addr_type  address type (short/group)
      * @return ::daliReturnValue 
      *
      * This methods sends a "direct arc power control command" to the bus
      * It uses sendRawWait(), so it waits for the bus to become idle before and after transmission. */
    daliReturnValue sendArcWait(byte address, byte value, byte addr_type = DaliAddressTypes::SHORT, byte timeout = 50);
    daliReturnValue sendArcBroadcastWait(byte value, byte timeout = 50);

    /** Send a DALI command
      * @param  address    destination address
      * @param  command    DALI command
      * @param  addr_type  address type (short/group)
      * @return ::daliReturnValue 
      * This method sends a DALI Command to the bus (Commands from 0 to 255).
      * It doesn't check if the bus is ready and returns immediately, stating if transmission could be
      * initiated through its response value ::daliReturnValue.
      * Note that some of the special commands need to be sent twice (258 - INITIALISE, 259 - RANDOMISE), which
      * this method doesn't do by itself. */
    daliReturnValue sendCmd(byte address, DaliCmd command, byte addr_type = DaliAddressTypes::SHORT);
    daliReturnValue sendCmdBroadcast(DaliCmd command);

    /** Send a DALI command, wait for its completion and return the response if available
      * @param  address    destination address
      * @param  command    DALI command
      * @param  addr_type  address type (short/group)
      * @param  timeout    time in ms to wait for action to complete
      * @return returns either the response, DALI_RX_EMPTY or any of ::daliReturnValue on error  */
    int sendCmdWait(byte address, DaliCmd command, byte addr_type = DaliAddressTypes::SHORT, byte timeout = 50);
    int sendCmdBroadcastWait(DaliCmd command, byte timeout = 50);

    /** Send a DALI special command
      * @param  command  DALI special command
      * @param  value    Value (2nd byte)
      * @return ::daliReturnValue  
      * 
      * This method sends a DALI Special Command to the bus (Special Commands are commands from 256 to 287).
      * It doesn't check if the bus is ready and returns immediately, stating if transmission could be
      * initiated through its response value ::daliReturnValue.
      * Note that some of the special commands need to be sent twice (258 - INITIALISE, 259 - RANDOMISE), which
      * this method doesn't do by itself. */
    daliReturnValue sendSpecialCmd(DaliSpecialCmd command, byte value = 0);

    /** Send a DALI special command, wait for its completion and return the response if available
      * @param  command  DALI special command
      * @param  value    Value (2nd byte)
      * @param  timeout  time in ms to wait for action to complete
      * @return returns either the response, DALI_RX_EMPTY or any of ::daliReturnValue on error
      * 
      * This method sends a DALI Special Command to the bus (Special Commands are commands from 256 to 287).
      * It uses sendRawWait(), so it waits for the bus to become idle before and after transmission.
      * It returns either the received response, DALI_RX_EMPTY if no response has been received or any of
      * ::daliReturnValue if an error has occurred.
      * Note that some of the special commands need to be sent twice (258 - INITIALISE, 259 - RANDOMISE), which
      * this method doesn't do by itself. */
    int sendSpecialCmdWait(word command, byte value = 0, byte timeout = 50);

    /** Send raw values to the DALI bus
      * @param message  byte array to send
      * @param bits   number of bits to send
      * @param timeout  time in ms to wait for action to complete
      *
      * This method sends a raw byte array of @p bits to the bus. The array can be three bytes max.
      * It waits for the bus to become idle before and after transmission. It returns either the received response,
      * DALI_RX_EMPTY if no response has been received or any of ::daliReturnValue if an error has occurred. */
    int sendRawWait(const byte * message, uint8_t bits, byte timeout = 50);

    /** Set Callback for receiving messages. */
    void setCallback(EventHandlerReceivedDataFuncPtr callback);

    /** Set Callback for activity. */
    void setActivityCallback(EventHandlerActivityFuncPtr callback);

#ifndef DALI_NO_COMMISSIONING
    /** Initiate commissioning of all DALI ballasts
      * @param startAddress  address starting short address assignment from
      * @param onlyNew       commission only ballasts without short address
      *
      * This method starts the DALI commissioning process. During commissioning the method commission_tick()
      * needs to be called repeatedly until commissioning has finished. By default commissioning is done for
      * all ballasts on the bus (@p onlyNew = false). With this, at first current short addresses from
      * all ballasts are removed. Then all found ballasts are assigned a new short address, starting
      * from @p startAddress. Commissioning has finished when @p commissionState is set back to COMMISSION_OFF.
      * The number of ballasts found can be determined from #nextShortAddress.
      * With @p onlyNew = true ballasts with a short address assigned are ignored. The caller is responsible
      * for setting an appropriate value to @p startAddress. */
    void commission(byte startAddress = 0, bool onlyNew = false);
    
    /** State machine ticker for commissioning. See commission(). */
    void commission_tick();
    
    /** next address to program on commissioning. When commissioning finished, reflects number of ballasts found. */
    byte nextShortAddress;
    
    /** When true, only ballasts without short address set are commissioned. */
    bool commissionOnlyNew;

    /** commissioning state machine states */
    enum commissionStateEnum { 
      COMMISSION_OFF, COMMISSION_INIT, COMMISSION_INIT2, COMMISSION_WRITE_DTR, COMMISSION_REMOVE_SHORT, COMMISSION_REMOVE_SHORT2, COMMISSION_RANDOM, COMMISSION_RANDOM2, COMMISSION_RANDOMWAIT,
      COMMISSION_STARTSEARCH, COMMISSION_SEARCHHIGH, COMMISSION_SEARCHMID, COMMISSION_SEARCHLOW,
      COMMISSION_COMPARE, COMMISSION_CHECKFOUND, COMMISSION_PROGRAMSHORT,
      COMMISSION_VERIFYSHORT, COMMISSION_VERIFYSHORTRESPONSE, COMMISSION_QUERYDEVICETYPE, COMMISSION_QUERYDEVICETYPERESPONSE,
      COMMISSION_WITHDRAW, COMMISSION_TERMINATE
    };
    commissionStateEnum commissionState = COMMISSION_OFF; /**< current state of commissioning state machine */
#endif

  protected:
    /** Prepares a byte array for sending DALI commands */
    byte * prepareCmd(byte * message, byte address, byte command, byte type, byte selector);
    
    /** Prepares a byte array for sending DALI Special Commands */
    byte * prepareSpecialCmd(byte * message, word command, byte value);
};

/** Dali class instance for main usage (seems to be common Arduino Library style) */
#ifndef DALI_DONT_EXPORT
extern DaliClass Dali;
#endif

#endif // DALI_H
