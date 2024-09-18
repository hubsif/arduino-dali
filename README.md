[![PlatformIO Registry](https://badges.registry.platformio.org/packages/thewhobox/library/Dali.svg)](https://registry.platformio.org/libraries/thewhobox/Dali) [![CodeFactor](https://www.codefactor.io/repository/github/thewhobox/arduino-dali/badge)](https://www.codefactor.io/repository/github/thewhobox/arduino-dali)

### Fork
This repo is a fork with a few special enhancements:  
 - Defines to setup dali (see below)
 - Support for rp2040, ESP32, ESP8266*, AVR*
 - Supports externally called timer isr (DALI_NO_TIMER)
 - added functions for easy broadcast telegrams
 - added Dali Commands (DaliCmdExtendedDT8)
 - added receive dali commands
 - added callback for dali activity (ex to use a led to show activity)
 - use of macros for set/get BusLevel to reduce time spent in interrupt

\* not tested

# arduino-dali
A timer-based DALI library for Arduino

This library allows you to create a DALI controller/gateway with an Arduino device. It supports sending commands, receiving responses and commissioning devices. It requires the TimerOne library for transmission and the PinChangeInterrupt library for reception. These libraries need to be installed with the Library Manager in the Arduino IDE.


Library Documentation: https://hubsif.github.io/arduino-dali/

Short example usage:

```c
#include <Dali.h>

void DaliCommand(uint8_t *data, uint8_t len)
{
  // do wathever you want with this information
  // but keep it really short!
  // to long and you miss commands or corrupt sending commands
}

void DaliActivity()
{
  // do wathever you want with this information
  // but keep it really short! Really short!
  // to long and you miss commands or corrupt sending commands
}

void setup() {
  Dali.begin(2, 3);

  // add a callback for received commands
  Dali.setCallback(DaliCommand);

  // add a callback for activity notification (use for leds or something)
  Dali.setActivityCallback(DaliActivity);
}

void loop() {
  // blink ballast with short address 3
  Dali.sendArc(3, 254);
  // alternatively to prevent fading you could use
  // Dali.sendCmd(3, Dali.CMD_RECALL_MAX);
  delay(1000);

  Dali.sendArc(3, 0);
  // alternatively to prevent fading you could use
  // Dali.sendCmd(3, Dali.CMD_OFF);
  delay(1000);
}
```

### Use Defines
|Define|Description|Values|Default|
|---|---|---|---|
|DALI_TIMER|Specify Timer Instance |0-3 (rp2040)<br />0-1 (esp32/8266)<br />1-2 (avr)|0|
|DALI_NO_TIMER|Don`t use a timer. DaliBusClass::timerISR will be called external|-|-|
|DALI_NO_COMMISSIONING|Exclude commissioning Code|-|-|
|DALI_DONT_EXPORT|Don`t automaticly export a Dali instance|-|-|
|DALI_NO_COLLISSION_CHECK|Remove collission check if you are the only master (use with caution)|-|-|