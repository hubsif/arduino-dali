# arduino-dali
A timer-based DALI library for Arduino

This library allows you to create a DALI controller/gateway with an Arduino device. It supports sending commands, receiving responses and commissioning devices. It requires the TimerOne library for transmission and the PinChangeInterrupt library for reception. These libraries need to be installed with the Library Manager in the Arduino IDE.


Library Documentation: https://hubsif.github.io/arduino-dali/

Short example usage:

```c
#include <Dali.h>

void setup() {
  Dali.begin(2, 3);
}

void loop() {
  // blink ballast with short address 3
  Dali.sendArc(3, 254);
  // alternatively to prevent fading you could use
  // Dali.sendCmd(3, Dali.CMD_RECALL_MIN);
  delay(1000);

  Dali.sendArc(3, 0);
  // alternatively to prevent fading you could use
  // Dali.sendCmd(3, Dali.CMD_OFF);
  delay(1000);
}
```

more to follow...
