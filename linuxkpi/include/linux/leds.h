/* linux/leds.h -- Kestrel's Linux KPI: LED class (no LED devices are registered) */
#ifndef _LKPI_LINUX_LEDS_H
#define _LKPI_LINUX_LEDS_H

#include <linux/types.h>
#include <linux/errno.h>

struct device;
enum led_brightness { LED_OFF = 0, LED_ON = 1, LED_HALF = 127, LED_FULL = 255 };
struct led_classdev {
    const char *name;
    unsigned int brightness;
    unsigned int max_brightness;
    void (*brightness_set)(struct led_classdev *led_cdev, enum led_brightness brightness);
    enum led_brightness (*brightness_get)(struct led_classdev *led_cdev);
    struct device *dev;
};
static inline int led_classdev_register(struct device *parent, struct led_classdev *led_cdev) { return 0; }
static inline void led_classdev_unregister(struct led_classdev *led_cdev) { }
static inline void led_classdev_suspend(struct led_classdev *led_cdev) { }
static inline void led_classdev_resume(struct led_classdev *led_cdev) { }

#endif
