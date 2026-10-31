#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(mks_mkb1_boot_led, LOG_LEVEL_INF);

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
};

static int set_leds(bool red, bool green, bool blue);

#if CONFIG_MKS_MKB1_595_DIAGNOSTIC
static const struct gpio_dt_spec columns[] = {
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 0),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 2),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 3),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 4),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 5),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 6),
    GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(kscan_fw), col_gpios, 7),
};

static int set_column(size_t active_column)
{
    for (size_t i = 0; i < ARRAY_SIZE(columns); i++) {
        int err = gpio_pin_set_dt(&columns[i], i == active_column);
        if (err < 0) {
            LOG_ERR("Failed to set 74HC595 column %u: %d", (unsigned int)i, err);
            return err;
        }
    }

    return 0;
}

static void run_595_diagnostic(void)
{
    const struct device *kscan = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    if (!device_is_ready(kscan)) {
        LOG_ERR("Key scanner is not ready");
        return;
    }

    int err = kscan_disable_callback(kscan);
    if (err < 0) {
        LOG_ERR("Failed to stop key scanning: %d", err);
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(columns); i++) {
        if (!gpio_is_ready_dt(&columns[i])) {
            LOG_ERR("74HC595 GPIO %u is not ready", (unsigned int)i);
            return;
        }

        err = gpio_pin_configure_dt(&columns[i], GPIO_OUTPUT_INACTIVE);
        if (err < 0) {
            LOG_ERR("Failed to configure 74HC595 GPIO %u: %d", (unsigned int)i, err);
            return;
        }
    }

    LOG_INF("Diagnostic mode: COL1 toggles every 1.5 seconds; key scanning is stopped");
    while (true) {
        if (set_column(1) < 0) {
            return;
        }
        if (set_leds(true, false, false) < 0) {
            return;
        }
        k_msleep(1500);

        if (set_column(ARRAY_SIZE(columns)) < 0) {
            return;
        }
        if (set_leds(false, false, false) < 0) {
            return;
        }
        k_msleep(1500);
    }
}
#endif

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

#if CONFIG_MKS_MKB1_595_DIAGNOSTIC
    k_msleep(1000);
    run_595_diagnostic();
#endif
}

K_THREAD_DEFINE(mks_mkb1_boot_led, 1024, boot_led_thread, NULL, NULL, NULL,
                K_PRIO_PREEMPT(8), 0, 0);
