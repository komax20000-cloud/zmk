/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_key_repeat_rate

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_key_repeat_rate.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_key_repeat_rate_binding_pressed(struct zmk_behavior_binding *binding,
                                              struct zmk_behavior_binding_event event) {
    int32_t steps;
    if (binding->param1 == 0) {
        steps = -1;
    } else if (binding->param1 == 1) {
        steps = 1;
    } else {
        LOG_ERR("Invalid key repeat adjustment parameter %u", binding->param1);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int result = zmk_key_repeat_rate_adjust(steps);
    if (result < 0) {
        LOG_ERR("Invalid key repeat adjustment %d: %d", steps, result);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_key_repeat_rate_binding_released(struct zmk_behavior_binding *binding,
                                               struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_key_repeat_rate_driver_api = {
    .binding_pressed = on_key_repeat_rate_binding_pressed,
    .binding_released = on_key_repeat_rate_binding_released,
};

static int behavior_key_repeat_rate_init(const struct device *dev) { return 0; }

#define KR_RATE_INST(n)                                                                            \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_key_repeat_rate_init, NULL, NULL, NULL, POST_KERNEL,        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_key_repeat_rate_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KR_RATE_INST)

#endif
