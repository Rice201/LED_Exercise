#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "app.h"

static const struct gpio_dt_spec channels[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(rgb_red), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(rgb_green), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(rgb_blue), gpios),
};

K_MSGQ_DEFINE(rgb_commands, sizeof(enum rgb_color), 8, 4);
K_SEM_DEFINE(rgb_ready, 0, 1);
static int rgb_init_result;

static int apply_color(enum rgb_color color)
{
	uint32_t mask;

	switch (color) {
	case RGB_OFF:
		mask = 0;
		break;
	case RGB_RED:
		mask = BIT(0);
		break;
	case RGB_YELLOW:
		mask = BIT(0) | BIT(1);
		break;
	case RGB_GREEN:
		mask = BIT(1);
		break;
	case RGB_BLUE:
		mask = BIT(2);
		break;
	default:
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		// ACTIVE_LOW translates logical ON (1) into a physical LOW output.
		int err = gpio_pin_set_dt(&channels[i], (mask & BIT(i)) != 0);

		if (err < 0) {
			return err;
		}
	}
	return 0;
}

static void rgb_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	enum rgb_color color;

	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		if (!gpio_is_ready_dt(&channels[i])) {
			rgb_init_result = -ENODEV;
			break;
		}
		rgb_init_result = gpio_pin_configure_dt(&channels[i], GPIO_OUTPUT_INACTIVE);
		if (rgb_init_result < 0) {
			break;
		}
	}
	k_sem_give(&rgb_ready);
	if (rgb_init_result < 0) {
		return;
	}

	while (1) {
		k_msgq_get(&rgb_commands, &color, K_FOREVER);
		int err = apply_color(color);

		if (err < 0) {
			printk("Error: RGB output (%d)\n", err);
		}
	}
}

K_THREAD_DEFINE(rgb_thread_id, 1024, rgb_thread,
		NULL, NULL, NULL, 5, 0, 0);

int rgb_led_init(void)
{
	k_sem_take(&rgb_ready, K_FOREVER);
	return rgb_init_result;
}

void rgb_led_set(enum rgb_color color)
{
	k_msgq_put(&rgb_commands, &color, K_FOREVER);
}
