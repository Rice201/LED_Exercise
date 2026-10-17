#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BUTTON_NODE DT_ALIAS(sw0)
#define LED_NODE    DT_ALIAS(led_red)

#if !DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
#error "Button alias sw0 is not available"
#endif

#if !DT_NODE_HAS_STATUS(LED_NODE, okay)
#error "External LED alias led-red is not available"
#endif

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(LED_NODE, gpios);

int main(void)
{
    int ret;
    int led_state = 0;
    int previous_button = 0;

    if (!gpio_is_ready_dt(&button) ||
        !gpio_is_ready_dt(&led)) {
        printk("GPIO device not ready\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Button configuration error: %d\n", ret);
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("LED configuration error: %d\n", ret);
        return 0;
    }

    printk("External LED on PC4. Press B2 to toggle.\n");

    while (1) {
        int button_state = gpio_pin_get_dt(&button);

        if (button_state < 0) {
            printk("Button read error: %d\n", button_state);
            k_sleep(K_MSEC(20));
            continue;
        }

        if (button_state && !previous_button) {
            led_state = !led_state;
            gpio_pin_set_dt(&led, led_state);
            printk("External LED: %s\n",
                   led_state ? "ON" : "OFF");
        }

        previous_button = button_state;
        k_sleep(K_MSEC(20));
    }

    return 0;
}
