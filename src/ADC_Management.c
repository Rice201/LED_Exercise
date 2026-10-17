#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "../include/app.h"

#define BUTTON_NODE DT_ALIAS(sw1)
#define LED_NODE    DT_NODELABEL(led_1)

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(LED_NODE, gpios);

static int led_state;
static int previous_button;

int external_led_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&button) ||
        !gpio_is_ready_dt(&led)) {
        printk("GPIO device not ready\n");
        return -ENODEV;
    }

    led_state = 0;
    previous_button = 0;

    printk("External LED initialized. Press B2 to toggle.\n");

    return 0;
}

void external_led_task(void)
{
    int button_state = gpio_pin_get_dt(&button);

    if (button_state && !previous_button) {
        led_state = !led_state;

        gpio_pin_set_dt(&led, led_state);

        printk("External LED: %s\n",
               led_state ? "ON" : "OFF");
    }

    previous_button = button_state;
}