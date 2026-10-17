#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/adc.h>

#include "../include/app.h"

#define BUTTON_NODE DT_ALIAS(sw1)
#define LED1_NODE   DT_NODELABEL(led_1)
#define LED2_NODE   DT_NODELABEL(led_2)
#define LED3_NODE   DT_NODELABEL(led_3)
#define LDR_NODE    DT_PATH(zephyr_user)

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static const struct gpio_dt_spec led1 =
    GPIO_DT_SPEC_GET(LED1_NODE, gpios);

static const struct gpio_dt_spec led2 =
    GPIO_DT_SPEC_GET(LED2_NODE, gpios);

static const struct gpio_dt_spec led3 =
    GPIO_DT_SPEC_GET(LED3_NODE, gpios);

static const struct adc_dt_spec ldr =
    ADC_DT_SPEC_GET_BY_IDX(LDR_NODE, 0);

static int16_t ldr_sample;
static int led1_state;
static int led2_state;
static int led3_state;
static int previous_button;
static int count;

int rgb_led_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&button) ||
        !gpio_is_ready_dt(&led1) ||
        !gpio_is_ready_dt(&led2) ||
        !gpio_is_ready_dt(&led3)) {
        printk("GPIO device not ready\n");
        return -ENODEV;
    }

    /* Configure button as input */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Button configuration error: %d\n", ret);
        return ret;
    }

    /* Configure LEDs as outputs, initially OFF */
    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("LED1 configuration error: %d\n", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("LED2 configuration error: %d\n", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("LED3 configuration error: %d\n", ret);
        return ret;
    }

    led1_state = 0;
    led2_state = 0;
    led3_state = 0;
    count = 0;

    /* Read the initial button state */
    previous_button = gpio_pin_get_dt(&button);
    if (previous_button < 0) {
        printk("Button read error: %d\n", previous_button);
        return previous_button;
    }

    printk("LEDs initialized. Press B2 to change LED.\n");
    return 0;
}

void external_led_task(void)
{
    int button_state = gpio_pin_get_dt(&button);

    if (button_state < 0) {
        printk("Button read error: %d\n", button_state);
        return;
    }

    if (button_state && !previous_button) {

        count = (count + 1) % 3;

        led1_state = (count == 1);
        led2_state = (count == 2);
        led3_state = (count == 0);

        gpio_pin_set_dt(&led1, led1_state);
        gpio_pin_set_dt(&led2, led2_state);
        gpio_pin_set_dt(&led3, led3_state);

        printk("Count: %d | LED1: %d | LED2: %d | LED3: %d\n",
               count, led1_state, led2_state, led3_state);
    }

    previous_button = button_state;
}

void ldr_read_task(void)
{
    struct adc_sequence sequence = {
        .buffer = &ldr_sample,
        .buffer_size = sizeof(ldr_sample),
    };

    int ret = adc_sequence_init_dt(&ldr, &sequence);

    if (ret < 0) {
        printk("ADC sequence error: %d\n", ret);
        return;
    }

    ret = adc_read_dt(&ldr, &sequence);

    if (ret < 0) {
        printk("ADC read error: %d\n", ret);
        return;
    }

    int adc_value = ldr_sample;
    int percentage = ((4095 - adc_value) * 100) / 4095;

    printk("LDR ADC: %d | Percentage: %d%%\n",
           adc_value, percentage);
}