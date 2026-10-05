/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

int zmk_key_repeat_rate_adjust(int32_t steps);
int zmk_key_repeat_set_enabled(bool enabled);
int zmk_key_repeat_set_initial_delay(uint32_t delay_ms);
int zmk_key_repeat_pause(void);
