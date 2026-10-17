#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#include "app.h"

#define SAMPLE_PERIOD_MS 2000

static const struct adc_dt_spec adc_input =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int brightness_init(void)
{
	if (!adc_is_ready_dt(&adc_input)) {
		return -ENODEV;
	}
	return adc_channel_setup_dt(&adc_input);
}

static void brightness_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int16_t sample;
	struct adc_sequence sequence = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};
	const uint32_t full_scale = BIT(adc_input.resolution) - 1U;
	int64_t next_sample;

	k_sem_take(&brightness_wake, K_FOREVER);
	next_sample = k_uptime_get();

	while (1) {
		k_mutex_lock(&mode_mutex, K_FOREVER);
		if (current_mode != MODE_NORMAL) {
			k_mutex_unlock(&mode_mutex);
			k_sem_take(&brightness_wake, K_FOREVER);
			next_sample = k_uptime_get();
			continue;
		}
		if (k_uptime_get() < next_sample) {
			k_mutex_unlock(&mode_mutex);
			if (k_sem_take(&brightness_wake,
				       K_TIMEOUT_ABS_MS(next_sample)) == 0) {
				next_sample = k_uptime_get();
			}
			continue;
		}

		// Keep a completed ADC update ordered before any subsequent mode change.
		int err = adc_sequence_init_dt(&adc_input, &sequence);

		if (err == 0) {
			err = adc_read_dt(&adc_input, &sequence);
		}
		if (err < 0) {
			printk("Error: ADC acquisition (%d)\n", err);
			rgb_led_set(RGB_OFF);
		} else {
			uint32_t scaled = (uint32_t)sample * 100U;
			enum rgb_color color;
			const char *color_name;

			if (scaled < 33U * full_scale) {
				color = RGB_RED;
				color_name = "RED";
			} else if (scaled <= 66U * full_scale) {
				color = RGB_YELLOW;
				color_name = "YELLOW";
			} else {
				color = RGB_GREEN;
				color_name = "GREEN";
			}
			rgb_led_set(color);
			printk("NORMAL: raw ADC=%d/%u, brightness=%u.%02u%%, RGB=%s\n",
			       sample, full_scale, scaled / full_scale,
			       (scaled * 100U / full_scale) % 100U, color_name);
		}

		next_sample += SAMPLE_PERIOD_MS;
		if (next_sample <= k_uptime_get()) {
			next_sample = k_uptime_get() + SAMPLE_PERIOD_MS;
		}
		k_mutex_unlock(&mode_mutex);
	}
}

K_THREAD_DEFINE(brightness_thread_id, 1536, brightness_thread,
		NULL, NULL, NULL, 6, 0, 0);
