/** @file dali_blink.ino
 *  simple DALI library usage example
 */
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
