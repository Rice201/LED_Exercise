#ifndef LED_EXERCISE_APP_H
#define LED_EXERCISE_APP_H

#include <zephyr/kernel.h>

enum app_mode {
	MODE_NORMAL,
	MODE_BLUE,
	MODE_OFF,
};

enum rgb_color {
	RGB_OFF,
	RGB_RED,
	RGB_YELLOW,
	RGB_GREEN,
	RGB_BLUE,
};

extern enum app_mode current_mode;
extern struct k_mutex mode_mutex;
extern struct k_sem brightness_wake;

int brightness_init(void);
int rgb_led_init(void);
void rgb_led_set(enum rgb_color color);

#endif
