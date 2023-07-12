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

/**
 * DALI library base class.
 */
class DaliClass {
  public:
    /** DALI commands */
    enum daliCmd {
      CMD_OFF = 0, CMD_UP = 1, CMD_DOWN = 2, CMD_STEP_UP = 3, CMD_STEP_DOWN = 4,
      CMD_RECALL_MAX = 5, CMD_RECALL_MIN = 6, CMD_STEP_DOWN_AND_OFF = 7, CMD_ON_AND_STEP_UP = 8,
      CMD_GO_TO_LAST = 10, // DALI-2
      CMD_GO_TO_SCENE = 16,
      CMD_RESET = 32, CMD_ARC_TO_DTR = 33,
      CMD_SAVE_VARS = 34, CMD_SET_OPMODE = 35, CMD_RESET_MEM = 36, CMD_IDENTIFY = 37, // DALI-2
      CMD_DTR_AS_MAX = 42, CMD_DTR_AS_MIN = 43, CMD_DTR_AS_FAIL = 44, CMD_DTR_AS_POWER_ON = 45, CMD_DTR_AS_FADE_TIME = 46, CMD_DTR_AS_FADE_RATE = 47,
      CMD_DTR_AS_EXT_FADE_TIME = 48, // DALI-2
      CMD_DTR_AS_SCENE = 64, CMD_REMOVE_FROM_SCENE = 80,
      CMD_ADD_TO_GROUP = 96, CMD_REMOVE_FROM_GROUP = 112,
      CMD_DTR_AS_SHORT = 128,
      CMD_QUERY_STATUS = 144, CMD_QUERY_BALLAST = 145, CMD_QUERY_LAMP_FAILURE = 146, CMD_QUERY_LAMP_POWER_ON = 147, CMD_QUERY_LIMIT_ERROR = 148,
      CMD_QUERY_RESET_STATE = 149, CMD_QUERY_MISSING_SHORT = 150, CMD_QUERY_VERSION = 151, CMD_QUERY_DTR = 152, CMD_QUERY_DEVICE_TYPE = 153,
      CMD_QUERY_PHYS_MIN = 154, CMD_QUERY_POWER_FAILURE = 155,
      CMD_QUERY_OPMODE = 158, CMD_QUERY_LIGHTTYPE = 159, // DALI-2
      CMD_QUERY_ACTUAL_LEVEL = 160, CMD_QUERY_MAX_LEVEL = 161, CMD_QUERY_MIN_LEVEL = 162, CMD_QUERY_POWER_ON_LEVEL = 163, CMD_QUERY_FAIL_LEVEL = 164, CMD_QUERY_FADE_SPEEDS = 165,
      CMD_QUERY_SPECMODE = 166, CMD_QUERY_NEXT_DEVTYPE = 167, CMD_QUERY_EXT_FADE_TIME = 168, CMD_QUERY_CTRL_GEAR_FAIL = 169, // DALI-2
      CMD_QUERY_SCENE_LEVEL = 176,
      CMD_QUERY_GROUPS_0_7 = 192, CMD_QUERY_GROUPS_8_15 = 193,
      CMD_QUERY_ADDRH = 194, CMD_QUERY_ADDRM = 195, CMD_QUERY_ADDRL = 196
    };
    
    /** DALI special commands */
    enum daliSpecialCmd {
      CMD_TERMINATE = 256, CMD_SET_DTR = 257,
      CMD_INITIALISE = 258, CMD_RANDOMISE = 259, CMD_COMPARE = 260, CMD_WITHDRAW = 261,
      CMD_SEARCHADDRH = 264, CMD_SEARCHADDRM = 265, CMD_SEARCHADDRL = 266,
      CMD_PROGRAMSHORT = 267, CMD_VERIFYSHORT = 268, CMD_QUERY_SHORT = 269, CMD_PHYS_SEL = 270,
      CMD_ENABLE_DT = 272, CMD_LOAD_DTR1 = 273, CMD_LOAD_DTR2 = 274, CMD_WRITE_MEM_LOC = 275,
      CMD_WRITE_MEM_LOC_NOREPLY = 276 // DALI-2
    };

    /** DALI device types */
    enum daliDevTypes {
      FLUORESCENT_LAMP,
      EMERGENCY_LIGHT,
      DISCHARGE_LAMP,
      HALOGEN_LAMP,
      INCANDESCENT_LAMP,
      DC_CONVERTER,
      LED_MODULE,
      SWITCH,
      COLOUR_CTRL,
      SEQUENCER,
      OPTICAL_CTRL
    };

    /** Type of address (short/group) */
    enum daliAddressTypes {
      DALI_SHORT_ADDRESS = 0,
      DALI_GROUP_ADDRESS = 1
    };

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
    daliReturnValue sendArc(byte address, byte value, byte addr_type = DALI_SHORT_ADDRESS);
    daliReturnValue sendArcBroadcast(byte value);

    /** Send a direct arc level command and wait for its completion
      * @param  address    destination address
      * @param  value      arc level
      * @param  addr_type  address type (short/group)
      * @return ::daliReturnValue 
      *
      * This methods sends a "direct arc power control command" to the bus
      * It uses sendRawWait(), so it waits for the bus to become idle before and after transmission. */
    daliReturnValue sendArcWait(byte address, byte value, byte addr_type = DALI_SHORT_ADDRESS, byte timeout = 50);
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
    daliReturnValue sendCmd(byte address, byte command, byte addr_type = DALI_SHORT_ADDRESS);
    daliReturnValue sendCmdBroadcast(byte command);

    /** Send a DALI command, wait for its completion and return the response if available
      * @param  address    destination address
      * @param  command    DALI command
      * @param  addr_type  address type (short/group)
      * @param  timeout    time in ms to wait for action to complete
      * @return returns either the response, DALI_RX_EMPTY or any of ::daliReturnValue on error  */
    int sendCmdWait(byte address, byte command, byte addr_type = DALI_SHORT_ADDRESS, byte timeout = 50);
    int sendCmdBroadcastWait(byte command, byte timeout = 50);

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
    daliReturnValue sendSpecialCmd(word command, byte value = 0);

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
      * @param length   length of the byte array
      * @param timeout  time in ms to wait for action to complete
      *
      * This method sends a raw byte array of @p length to the bus. The array can be three bytes max.
      * It waits for the bus to become idle before and after transmission. It returns either the received response,
      * DALI_RX_EMPTY if no response has been received or any of ::daliReturnValue if an error has occurred. */
    int sendRawWait(const byte * message, byte length, byte timeout = 50);

    /** Set Callback for receiving messages. */
    void setCallback(EventHandlerReceivedDataFuncPtr callback);

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
