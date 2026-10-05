/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_key_press

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>
#include <zmk/keys.h>
#include <dt-bindings/zmk/hid_usage.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#ifdef CONFIG_ZMK_KEY_REPEAT
#include <zephyr/sys/atomic.h>

#define KEY_REPEAT_DEFAULT_INITIAL_DELAY_MS 20
#define KEY_REPEAT_DEFAULT_INTERVAL_MS 30
#define KEY_REPEAT_MIN_INTERVAL_MS 10
#define KEY_REPEAT_MAX_INTERVAL_MS 100
#define KEY_REPEAT_INTERVAL_STEP_MS 5
#define KEY_REPEAT_KEY_DOWN_MS 10
#define KEY_REPEAT_MAX_ACTIVE 8
#define KEY_REPEAT_PAUSE_MS 5000

struct active_key_repeat {
    uint32_t position;
    uint32_t keycode;
    bool in_use;
    bool physical_down;
    bool active;
    bool output_pressed;
    bool initial_repeat;
    struct k_work_delayable work;
};

static struct active_key_repeat active_key_repeats[KEY_REPEAT_MAX_ACTIVE];
static struct k_mutex key_repeat_lock;
static atomic_t key_repeat_interval_ms = ATOMIC_INIT(KEY_REPEAT_DEFAULT_INTERVAL_MS);
static atomic_t key_repeat_initial_delay_ms = ATOMIC_INIT(KEY_REPEAT_DEFAULT_INITIAL_DELAY_MS);
static atomic_t key_repeat_enabled = ATOMIC_INIT(0);
static bool key_repeat_paused;
static struct k_work_delayable key_repeat_pause_work;

static void key_repeat_timer_handler(struct k_work *work);
static void key_repeat_pause_timer_handler(struct k_work *work);

static struct active_key_repeat *find_key_repeat(uint32_t position) {
    for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
        if (active_key_repeats[i].in_use && active_key_repeats[i].position == position) {
            return &active_key_repeats[i];
        }
    }
    return NULL;
}

static struct active_key_repeat *find_free_key_repeat(void) {
    for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
        if (!active_key_repeats[i].in_use) {
            return &active_key_repeats[i];
        }
    }
    return NULL;
}

static bool keycode_is_repeatable(uint32_t encoded) {
    uint16_t usage_page = ZMK_HID_USAGE_PAGE(encoded);
    uint32_t keycode = ZMK_HID_USAGE_ID(encoded);

    if (usage_page == 0) {
        usage_page = HID_USAGE_KEY;
    }

    if (usage_page != HID_USAGE_KEY || keycode == 0 || is_mod(usage_page, keycode)) {
        return false;
    }

    switch (keycode) {
    case HID_USAGE_KEY_KEYBOARD_CAPS_LOCK:
    case HID_USAGE_KEY_KEYBOARD_SCROLL_LOCK:
    case HID_USAGE_KEY_KEYPAD_NUM_LOCK_AND_CLEAR:
        return false;
    default:
        return true;
    }
}

static void key_repeat_timer_handler(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct active_key_repeat *repeat = CONTAINER_OF(delayable, struct active_key_repeat, work);

    k_mutex_lock(&key_repeat_lock, K_FOREVER);
    if (!repeat->active) {
        if (!repeat->physical_down) {
            repeat->in_use = false;
        }
        k_mutex_unlock(&key_repeat_lock);
        return;
    }
    if (key_repeat_paused) {
        k_mutex_unlock(&key_repeat_lock);
        return;
    }

    repeat->output_pressed = !repeat->output_pressed;
    int result = raise_zmk_keycode_state_changed_from_encoded(
        repeat->keycode, repeat->output_pressed, k_uptime_get());
    if (result < 0) {
        LOG_ERR("Failed to emit repeated key event: %d", result);
        repeat->active = false;
        repeat->output_pressed = false;
        if (!repeat->physical_down) {
            repeat->in_use = false;
        }
        k_mutex_unlock(&key_repeat_lock);
        return;
    }

    int32_t next_delay_ms;
    if (repeat->output_pressed) {
        next_delay_ms = KEY_REPEAT_KEY_DOWN_MS;
    } else if (repeat->initial_repeat) {
        repeat->initial_repeat = false;
        next_delay_ms =
            (int32_t)atomic_get(&key_repeat_initial_delay_ms) - KEY_REPEAT_KEY_DOWN_MS;
    } else {
        next_delay_ms =
            (int32_t)atomic_get(&key_repeat_interval_ms) - KEY_REPEAT_KEY_DOWN_MS;
    }

    result = k_work_reschedule(&repeat->work, K_MSEC(next_delay_ms));
    if (result < 0) {
        LOG_ERR("Failed to schedule key repeat: %d", result);
        if (repeat->output_pressed) {
            int release_result = raise_zmk_keycode_state_changed_from_encoded(
                repeat->keycode, false, k_uptime_get());
            if (release_result < 0) {
                LOG_ERR("Failed to release key after repeat scheduling failure: %d",
                        release_result);
            } else {
                repeat->output_pressed = false;
            }
        }
        repeat->active = false;
        if (!repeat->physical_down) {
            repeat->in_use = false;
        }
    }
    k_mutex_unlock(&key_repeat_lock);
}

static void key_repeat_pause_timer_handler(struct k_work *work) {
    k_mutex_lock(&key_repeat_lock, K_FOREVER);
    key_repeat_paused = false;

    if (atomic_get(&key_repeat_enabled)) {
        for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
            struct active_key_repeat *repeat = &active_key_repeats[i];
            if (!repeat->active) {
                continue;
            }

            if (!repeat->output_pressed) {
                int press_result = raise_zmk_keycode_state_changed_from_encoded(
                    repeat->keycode, true, k_uptime_get());
                if (press_result < 0) {
                    LOG_ERR("Failed to resume held key after repeat pause: %d", press_result);
                    repeat->active = false;
                    continue;
                }
                repeat->output_pressed = true;
            }

            repeat->initial_repeat = true;
            int result = k_work_reschedule(&repeat->work, K_MSEC(KEY_REPEAT_KEY_DOWN_MS));
            if (result < 0) {
                LOG_ERR("Failed to resume key repeat: %d", result);
                if (repeat->output_pressed) {
                    int release_result = raise_zmk_keycode_state_changed_from_encoded(
                        repeat->keycode, false, k_uptime_get());
                    if (release_result < 0) {
                        LOG_ERR("Failed to release held key after repeat resume failure: %d",
                                release_result);
                    } else {
                        repeat->output_pressed = false;
                    }
                }
                repeat->active = false;
                if (!repeat->physical_down) {
                    repeat->in_use = false;
                }
            }
        }
    }

    k_mutex_unlock(&key_repeat_lock);
}

int zmk_key_repeat_pause(void) {
    k_mutex_lock(&key_repeat_lock, K_FOREVER);
    if (!atomic_get(&key_repeat_enabled)) {
        k_mutex_unlock(&key_repeat_lock);
        return 0;
    }
    key_repeat_paused = true;

    int result = 0;
    for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
        struct active_key_repeat *repeat = &active_key_repeats[i];
        if (!repeat->active) {
            continue;
        }

        int cancel_result = k_work_cancel_delayable(&repeat->work);
        if (cancel_result < 0 && cancel_result != -EINPROGRESS) {
            LOG_ERR("Failed to pause key repeat: %d", cancel_result);
            if (result == 0) {
                result = cancel_result;
            }
        }

        if (repeat->output_pressed) {
            int release_result = raise_zmk_keycode_state_changed_from_encoded(
                repeat->keycode, false, k_uptime_get());
            if (release_result < 0) {
                LOG_ERR("Failed to release held key while pausing repeat: %d", release_result);
                if (result == 0) {
                    result = release_result;
                }
            } else {
                repeat->output_pressed = false;
            }
        }
    }

    int schedule_result =
        k_work_reschedule(&key_repeat_pause_work, K_MSEC(KEY_REPEAT_PAUSE_MS));
    if (schedule_result < 0) {
        LOG_ERR("Failed to schedule key-repeat pause timeout: %d", schedule_result);
        key_repeat_paused = false;
        if (result == 0) {
            result = schedule_result;
        }
        for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
            struct active_key_repeat *repeat = &active_key_repeats[i];
            if (repeat->active) {
                if (!repeat->output_pressed) {
                    int press_result = raise_zmk_keycode_state_changed_from_encoded(
                        repeat->keycode, true, k_uptime_get());
                    if (press_result < 0) {
                        LOG_ERR("Failed to restore held key after pause timeout error: %d",
                                press_result);
                        repeat->active = false;
                        continue;
                    }
                    repeat->output_pressed = true;
                }
                repeat->initial_repeat = true;
                int resume_result =
                    k_work_reschedule(&repeat->work, K_MSEC(KEY_REPEAT_KEY_DOWN_MS));
                if (resume_result < 0) {
                    LOG_ERR("Failed to resume key repeat after pause timeout error: %d",
                            resume_result);
                    if (repeat->output_pressed) {
                        int release_result = raise_zmk_keycode_state_changed_from_encoded(
                            repeat->keycode, false, k_uptime_get());
                        if (release_result < 0) {
                            LOG_ERR("Failed to release held key after repeat resume failure: %d",
                                    release_result);
                        }
                    }
                    repeat->in_use = false;
                    repeat->active = false;
                    repeat->output_pressed = false;
                }
            }
        }
    }

    k_mutex_unlock(&key_repeat_lock);
    return result;
}

static int start_key_repeat(uint32_t keycode, struct zmk_behavior_binding_event event) {
    if (!keycode_is_repeatable(keycode)) {
        return raise_zmk_keycode_state_changed_from_encoded(keycode, true, event.timestamp);
    }

    k_mutex_lock(&key_repeat_lock, K_FOREVER);
    if (!atomic_get(&key_repeat_enabled)) {
        k_mutex_unlock(&key_repeat_lock);
        return raise_zmk_keycode_state_changed_from_encoded(keycode, true, event.timestamp);
    }

    struct active_key_repeat *repeat = find_key_repeat(event.position);
    if (repeat != NULL) {
        k_mutex_unlock(&key_repeat_lock);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    repeat = find_free_key_repeat();
    if (repeat == NULL) {
        LOG_ERR("No key repeat slots available for position %u", event.position);
        k_mutex_unlock(&key_repeat_lock);
        return raise_zmk_keycode_state_changed_from_encoded(keycode, true, event.timestamp);
    }

    repeat->position = event.position;
    repeat->keycode = keycode;
    repeat->in_use = true;
    repeat->physical_down = true;
    repeat->active = true;
    repeat->output_pressed = true;
    repeat->initial_repeat = true;

    int result =
        raise_zmk_keycode_state_changed_from_encoded(keycode, true, event.timestamp);
    if (result < 0) {
        repeat->in_use = false;
        repeat->active = false;
        repeat->output_pressed = false;
        k_mutex_unlock(&key_repeat_lock);
        return result;
    }

    if (key_repeat_paused) {
        result = raise_zmk_keycode_state_changed_from_encoded(keycode, false, k_uptime_get());
        if (result < 0) {
            LOG_ERR("Failed to release key during key-repeat pause: %d", result);
            k_mutex_unlock(&key_repeat_lock);
            return result;
        }
        repeat->output_pressed = false;
        k_mutex_unlock(&key_repeat_lock);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    result = k_work_reschedule(&repeat->work, K_MSEC(KEY_REPEAT_KEY_DOWN_MS));
    if (result < 0) {
        LOG_ERR("Failed to schedule key repeat: %d", result);
        repeat->active = false;
        int release_result =
            raise_zmk_keycode_state_changed_from_encoded(keycode, false, k_uptime_get());
        if (release_result < 0) {
            LOG_ERR("Failed to release key after repeat scheduling failure: %d", release_result);
        } else {
            repeat->output_pressed = false;
        }
        k_mutex_unlock(&key_repeat_lock);
        return release_result < 0 ? release_result : result;
    }

    k_mutex_unlock(&key_repeat_lock);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int stop_key_repeat(uint32_t keycode, struct zmk_behavior_binding_event event) {
    if (!keycode_is_repeatable(keycode)) {
        return raise_zmk_keycode_state_changed_from_encoded(keycode, false, event.timestamp);
    }

    k_mutex_lock(&key_repeat_lock, K_FOREVER);
    struct active_key_repeat *repeat = find_key_repeat(event.position);
    if (repeat == NULL) {
        k_mutex_unlock(&key_repeat_lock);
        return raise_zmk_keycode_state_changed_from_encoded(keycode, false, event.timestamp);
    }

    repeat->physical_down = false;
    repeat->active = false;
    int cancel_result = k_work_cancel_delayable(&repeat->work);
    if (cancel_result < 0 && cancel_result != -EINPROGRESS) {
        LOG_ERR("Failed to cancel key repeat: %d", cancel_result);
    }
    repeat->in_use = cancel_result == -EINPROGRESS;
    bool output_pressed = repeat->output_pressed;
    repeat->output_pressed = false;
    int result = ZMK_BEHAVIOR_OPAQUE;
    if (output_pressed) {
        result =
            raise_zmk_keycode_state_changed_from_encoded(keycode, false, event.timestamp);
    }
    k_mutex_unlock(&key_repeat_lock);
    return result;
}

int zmk_key_repeat_rate_adjust(int32_t steps) {
    if (steps != -1 && steps != 1) {
        return -EINVAL;
    }

    atomic_val_t current = atomic_get(&key_repeat_interval_ms);
    while (true) {
        atomic_val_t adjusted = current + steps * KEY_REPEAT_INTERVAL_STEP_MS;
        if (adjusted < KEY_REPEAT_MIN_INTERVAL_MS) {
            adjusted = KEY_REPEAT_MIN_INTERVAL_MS;
        } else if (adjusted > KEY_REPEAT_MAX_INTERVAL_MS) {
            adjusted = KEY_REPEAT_MAX_INTERVAL_MS;
        }
        if (atomic_cas(&key_repeat_interval_ms, current, adjusted)) {
            return 0;
        }
        current = atomic_get(&key_repeat_interval_ms);
    }
}

int zmk_key_repeat_set_initial_delay(uint32_t delay_ms) {
    if (delay_ms != 10 && delay_ms != 20 && delay_ms != 50) {
        return -EINVAL;
    }

    atomic_set(&key_repeat_initial_delay_ms, delay_ms);
    return 0;
}

int zmk_key_repeat_set_enabled(bool enabled) {
    k_mutex_lock(&key_repeat_lock, K_FOREVER);
    atomic_set(&key_repeat_enabled, enabled ? 1 : 0);

    int result = 0;
    if (!enabled) {
        key_repeat_paused = false;
        int pause_cancel_result = k_work_cancel_delayable(&key_repeat_pause_work);
        if (pause_cancel_result < 0 && pause_cancel_result != -EINPROGRESS) {
            LOG_ERR("Failed to cancel key-repeat pause timer: %d", pause_cancel_result);
            result = pause_cancel_result;
        }

        for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
            struct active_key_repeat *repeat = &active_key_repeats[i];
            if (!repeat->active) {
                continue;
            }

            repeat->active = false;
            int cancel_result = k_work_cancel_delayable(&repeat->work);
            if (cancel_result < 0 && cancel_result != -EINPROGRESS) {
                LOG_ERR("Failed to cancel key repeat: %d", cancel_result);
                if (result == 0) {
                    result = cancel_result;
                }
            }

            if (repeat->output_pressed) {
                int release_result = raise_zmk_keycode_state_changed_from_encoded(
                    repeat->keycode, false, k_uptime_get());
                if (release_result < 0) {
                    LOG_ERR("Failed to release held key after disabling repeat: %d",
                            release_result);
                    if (result == 0) {
                        result = release_result;
                    }
                } else {
                    repeat->output_pressed = false;
                }
            }
        }
    }

    k_mutex_unlock(&key_repeat_lock);
    return result;
}
#endif

static int behavior_key_press_init(const struct device *dev) {
#ifdef CONFIG_ZMK_KEY_REPEAT
    k_mutex_init(&key_repeat_lock);
    k_work_init_delayable(&key_repeat_pause_work, key_repeat_pause_timer_handler);
    for (int i = 0; i < KEY_REPEAT_MAX_ACTIVE; i++) {
        k_work_init_delayable(&active_key_repeats[i].work, key_repeat_timer_handler);
    }
#endif
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
#ifdef CONFIG_ZMK_KEY_REPEAT
    return start_key_repeat(binding->param1, event);
#else
    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
#endif
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
#ifdef CONFIG_ZMK_KEY_REPEAT
    return stop_key_repeat(binding->param1, event);
#else
    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
#endif
}

static const struct behavior_driver_api behavior_key_press_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

#define KP_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_key_press_init, NULL, NULL, NULL, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_key_press_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)
