#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(mks_mkb1_boot_led, LOG_LEVEL_INF);

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
};

static int set_leds(bool red, bool green, bool blue)
{
    const bool states[] = {red, green, blue};

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        int err = gpio_pin_set_dt(&leds[i], states[i]);
        if (err < 0) {
            LOG_ERR("Failed to set onboard LED %u: %d", (unsigned int)i, err);
            return err;
        }
    }

    return 0;
}

static void boot_led_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            LOG_ERR("Onboard LED %u GPIO is not ready", (unsigned int)i);
            return;
        }

        int err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (err < 0) {
            LOG_ERR("Failed to configure onboard LED %u: %d", (unsigned int)i, err);
            return;
        }
    }

    const bool colors[][3] = {
        {true, false, false},
        {false, true, false},
        {false, false, true},
    };

    for (size_t i = 0; i < ARRAY_SIZE(colors); i++) {
        if (set_leds(colors[i][0], colors[i][1], colors[i][2]) < 0) {
            return;
        }
        k_msleep(250);
    }

    set_leds(false, false, false);
}

K_THREAD_DEFINE(mks_mkb1_boot_led, 512, boot_led_thread, NULL, NULL, NULL,
                K_PRIO_PREEMPT(8), 0, 0);
