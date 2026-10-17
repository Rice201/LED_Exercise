#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "../include/app.h"

int main(void)
{
    int ret = rgb_led_init();

    if (ret < 0) {
        printk("RGB LED initialization failed: %d\n", ret);
        return ret;
    }

    while (1) {
        external_led_task();
        ldr_read_task();
        k_sleep(K_MSEC(20));
    }

    return 0;
}