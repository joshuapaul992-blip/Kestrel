/* include/kernel/evdev.h -- /dev/input/event*: keyboard and mouse as Linux evdev devices
 * (drivers/evdev.c). kinput.c feeds them. */
#ifndef KERNEL_EVDEV_H
#define KERNEL_EVDEV_H

#include <stdint.h>
#include <stdbool.h>

#define EVDEV_KEYBOARD  0               /* /dev/input/event0 */
#define EVDEV_MOUSE     1               /* /dev/input/event1 */
#define EVDEV_COUNT     2

void evdev_init(void);                  /* after devfs and sysfs */
/* One event (type, code and value as Linux's); EV_SYN ends a report. Any context. */
void evdev_event(int dev, uint16_t type, uint16_t code, int32_t value);
bool evdev_grabbed(void);               /* a file holds EVIOCGRAB on either device */

#endif
