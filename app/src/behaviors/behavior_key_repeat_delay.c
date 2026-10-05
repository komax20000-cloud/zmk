/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_key_repeat_delay

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_key_repeat_rate.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_key_repeat_delay_binding_pressed(struct zmk_behavior_binding *binding,
                                              struct zmk_behavior_binding_event event) {
    int result = zmk_key_repeat_set_initial_delay(binding->param1);
    if (result < 0) {
        LOG_ERR("Invalid key repeat initial delay %u: %d", binding->param1, result);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_key_repeat_delay_binding_released(struct zmk_behavior_binding *binding,
                                               struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_key_repeat_delay_driver_api = {
    .binding_pressed = on_key_repeat_delay_binding_pressed,
    .binding_released = on_key_repeat_delay_binding_released,
};

static int behavior_key_repeat_delay_init(const struct device *dev) { return 0; }

#define KR_DELAY_INST(n)                                                                           \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_key_repeat_delay_init, NULL, NULL, NULL, POST_KERNEL,       \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_key_repeat_delay_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KR_DELAY_INST)

#endif
