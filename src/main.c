#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "app.h"

K_MUTEX_DEFINE(mode_mutex);
K_SEM_DEFINE(brightness_wake, 0, 1);

enum button_event {
	BUTTON_SHORT_PRESS,
	BUTTON_LONG_PRESS,
};

K_MSGQ_DEFINE(button_events, sizeof(enum button_event), 16, 4);

enum app_mode current_mode = MODE_BLUE;

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback button_callback;
static atomic_t button_pressed;
static atomic_t long_press_sent;
static atomic_t dropped_button_events;
static bool stop_states_locked;

static void allow_stop_states(bool allow)
{
	if (allow && stop_states_locked) {
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE,
					 PM_ALL_SUBSTATES);
		stop_states_locked = false;
	} else if (!allow && !stop_states_locked) {
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE,
					 PM_ALL_SUBSTATES);
		stop_states_locked = true;
	}
}

static void queue_button_event(enum button_event event)
{
	if (k_msgq_put(&button_events, &event, K_NO_WAIT) != 0) {
		atomic_inc(&dropped_button_events);
	}
}

static void long_press_timeout(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	if (atomic_get(&button_pressed) &&
	    atomic_cas(&long_press_sent, 0, 1)) {
		queue_button_event(BUTTON_LONG_PRESS);
	}
}

K_TIMER_DEFINE(long_press_timer, long_press_timeout, NULL);

static void button_interrupt(const struct device *port,
			     struct gpio_callback *callback, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(callback);
	ARG_UNUSED(pins);

	int pressed = gpio_pin_get_dt(&button);
	if (pressed < 0) {
		return;
	}
	if (pressed && atomic_cas(&button_pressed, 0, 1)) {
		atomic_clear(&long_press_sent);
		k_timer_start(&long_press_timer, K_SECONDS(1), K_NO_WAIT);
	} else if (!pressed && atomic_cas(&button_pressed, 1, 0)) {
		k_timer_stop(&long_press_timer);
		if (!atomic_get(&long_press_sent)) {
			queue_button_event(BUTTON_SHORT_PRESS);
		}
	}
}

static int button_init(void)
{
	int err;

	if (!gpio_is_ready_dt(&button)) {
		return -ENODEV;
	}
	err = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (err < 0) {
		return err;
	}
	// B2 is sw1 (PA1), active-low with the board's existing debounce circuit.
	gpio_init_callback(&button_callback, button_interrupt, BIT(button.pin));
	err = gpio_add_callback(button.port, &button_callback);
	if (err < 0) {
		return err;
	}
	return gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
}

static void change_mode(enum app_mode mode)
{
	k_mutex_lock(&mode_mutex, K_FOREVER);
	allow_stop_states(false);
	current_mode = mode;
	if (mode == MODE_OFF) {
		rgb_led_set(RGB_OFF);
		printk("System OFF\n");
		allow_stop_states(true);
	} else if (mode == MODE_BLUE) {
		rgb_led_set(RGB_BLUE);
		printk("Mode: BLUE - ADC acquisition suspended\n");
	} else {
		rgb_led_set(RGB_OFF);
		printk("Mode: NORMAL - ADC acquisition every 2 seconds\n");
	}
	k_mutex_unlock(&mode_mutex);
	k_sem_give(&brightness_wake);
}

int main(void)
{
	int err;
	enum button_event event;

	allow_stop_states(false);
	printk("Brightness monitor - NUCLEO-WL55JC\n");
	err = rgb_led_init();
	if (err < 0) {
		printk("Error: RGB initialization (%d)\n", err);
		return 0;
	}
	err = brightness_init();
	if (err < 0) {
		printk("Error: ADC initialization (%d)\n", err);
		return 0;
	}
	err = button_init();
	if (err < 0) {
		printk("Error: B2 initialization (%d)\n", err);
		return 0;
	}

	change_mode(MODE_NORMAL);
	while (1) {
		k_msgq_get(&button_events, &event, K_FOREVER);
		if (event == BUTTON_LONG_PRESS) {
			if (current_mode != MODE_OFF) {
				change_mode(MODE_OFF);
			}
		} else if (current_mode == MODE_OFF) {
			change_mode(MODE_NORMAL);
		} else {
			change_mode(current_mode == MODE_NORMAL ?
				    MODE_BLUE : MODE_NORMAL);
		}
		atomic_val_t dropped = atomic_set(&dropped_button_events, 0);

		if (dropped != 0) {
			printk("Warning: dropped %ld button events\n", (long)dropped);
		}
	}
	return 0;
}
