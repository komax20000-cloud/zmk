---
title: Behavior Configuration
sidebar_label: Behaviors
---

Some behaviors have properties to adjust how they behave. These can also be used as templates to create custom behaviors when none of the built-in behaviors do what you want.

See [Configuration Overview](index.md) for instructions on how to change these settings.

See the [zmk/app/dts/behaviors/](https://github.com/zmkfirmware/zmk/tree/main/app/dts/behaviors) folder for all default behaviors.

## Common

### Kconfig

| Config                            | Type | Description                                                                          | Default |
| --------------------------------- | ---- | ------------------------------------------------------------------------------------ | ------- |
| `CONFIG_ZMK_BEHAVIORS_QUEUE_SIZE` | int  | Maximum number of behaviors to allow queueing from a macro or other complex behavior | 64      |

## Caps Word

Creates a custom behavior that behaves similar to a caps lock but deactivates when any key not in a continue list is pressed.

See the [caps word behavior](../behaviors/caps-word.md) documentation for more details and examples.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-caps-word.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-caps-word.yaml)

Applies to: `compatible = "zmk,behavior-caps-word"`

| Property         | Type  | Description                                                        | Default                         |
| ---------------- | ----- | ------------------------------------------------------------------ | ------------------------------- |
| `#binding-cells` | int   | Must be `<0>`                                                      |                                 |
| `continue-list`  | array | List of [key codes](/docs/codes) which do not deactivate caps lock | `<UNDERSCORE BACKSPACE DELETE>` |
| `mods`           | int   | A bit field of modifiers to apply                                  | `<MOD_LSFT>`                    |

`continue-list` is treated as if it always includes alphanumeric characters (A-Z, 0-9).

See [dt-bindings/zmk/modifiers.h](https://github.com/zmkfirmware/zmk/blob/main/app/include/dt-bindings/zmk/modifiers.h) for a list of modifiers.

You can use the following nodes to tweak the default behaviors:

| Node         | Behavior                               |
| ------------ | -------------------------------------- |
| `&caps_word` | [Caps Word](../behaviors/caps-word.md) |

## Hold-Tap

Creates a custom behavior that triggers one behavior when a key is held or a different one when the key is tapped.

See the [hold-tap behavior](../behaviors/hold-tap.mdx) documentation for more details and examples.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-hold-tap.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-hold-tap.yaml)

Applies to: `compatible = "zmk,behavior-hold-tap"`

| Property                      | Type          | Description                                                                                                    | Default            |
| ----------------------------- | ------------- | -------------------------------------------------------------------------------------------------------------- | ------------------ |
| `#binding-cells`              | int           | Must be `<2>`                                                                                                  |                    |
| `bindings`                    | phandle array | A list of two behaviors (without parameters): one for hold and one for tap                                     |                    |
| `flavor`                      | string        | Adjusts how the behavior chooses between hold and tap                                                          | `"hold-preferred"` |
| `tapping-term-ms`             | int           | How long in milliseconds the key must be held to trigger a hold                                                |                    |
| `quick-tap-ms`                | int           | Tap twice within this period (in milliseconds) to trigger a tap, even when held                                | -1 (disabled)      |
| `require-prior-idle-ms`       | int           | Triggers a tap immediately if any non-modifier key was pressed within `require-prior-idle-ms` of the hold-tap. | -1 (disabled)      |
| `retro-tap`                   | bool          | Triggers the tap behavior on release if no other key was pressed during a hold                                 | false              |
| `hold-while-undecided`        | bool          | Triggers the hold behavior immediately on press and releases before a tap                                      | false              |
| `hold-while-undecided-linger` | bool          | Continues to hold the hold behavior until after the tap is released                                            | false              |
| `hold-trigger-key-positions`  | array         | If set, pressing the hold-tap and then any key position _not_ in the list triggers a tap.                      |                    |

This behavior forwards the first parameter it receives to the parameter of the first behavior specified in `bindings`, and second parameter to the parameter of the second behavior.

The `flavor` property may be one of:

- `"hold-preferred"`
- `"balanced"`
- `"tap-preferred"`
- `"tap-unless-interrupted"`

See the [hold-tap behavior documentation](../behaviors/hold-tap.mdx) for an explanation of each flavor.

`hold-trigger-key-positions` is an array of zero-based key position indices.

You can use the following nodes to tweak the default behaviors:

| Node  | Behavior                                      |
| ----- | --------------------------------------------- |
| `&lt` | [Layer-tap](../behaviors/layers.md#layer-tap) |
| `&mt` | [Mod-tap](../behaviors/mod-tap.md)            |

## Key Repeat

Creates a custom behavior that repeats the whatever key code was last sent.

See the [key repeat behavior](../behaviors/key-repeat.md) documentation for more details and examples.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-key-repeat.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-key-repeat.yaml)

Applies to: `compatible = "zmk,behavior-key-repeat"`

| Property         | Type  | Description                      | Default           |
| ---------------- | ----- | -------------------------------- | ----------------- |
| `#binding-cells` | int   | Must be `<0>`                    |                   |
| `usage-pages`    | array | List of HID usage pages to track | `<HID_USAGE_KEY>` |

For the `usage-pages` property, use the `HID_USAGE_*` defines from [dt-bindings/zmk/hid_usage_pages.h](https://github.com/zmkfirmware/zmk/blob/main/app/include/dt-bindings/zmk/hid_usage_pages.h).

You can use the following nodes to tweak the default behaviors:

| Node          | Behavior                                 |
| ------------- | ---------------------------------------- |
| `&key_repeat` | [Key repeat](../behaviors/key-repeat.md) |

## Held Key Repeat

When `CONFIG_ZMK_KEY_REPEAT` is enabled, regular keyboard keys bound with `&kp` can repeat while held. Repeat is disabled by default and must be enabled with the toggle behavior. The initial key press is sent immediately, followed by repeats after an initial delay of 20 ms by default. The delay can be set to 10, 20, or 50 ms. The repeat interval starts at 30 ms and can be adjusted from 10 ms to 100 ms in 5 ms steps. Modifiers, lock keys, and non-keyboard HID usages are not auto-repeated.

The `zmk,behavior-key-repeat-rate` behavior adjusts the interval: parameter `0` makes repetition faster and parameter `1` makes it slower. This behavior only affects firmware-generated repeats; it does not change the host operating system's key repeat settings.

The `zmk,behavior-key-repeat-toggle` behavior enables repeat with parameter `1` and disables it with parameter `0`. On the Keychron K1 Ultra ANSI keymap, hold Fn and tap `↑` to enable repeat, `↓` to disable it, `←` to speed up, or `→` to slow down. Tap Fn+Z/X/C to set the initial delay to 10/20/50 ms. Fn+Enter pauses repeating for five seconds, then resumes it if repeat is still enabled. Pressing Fn+Enter again during the pause restarts the five-second countdown. While paused or disabled, currently held repeat keys are released to the host so its own key-repeat feature does not continue repeating them. The selected timings are held in memory and reset to defaults after reboot.

The `zmk,behavior-key-repeat-delay` behavior sets the initial repeat delay in milliseconds. Supported parameters are `10`, `20`, and `50`.

### Enable/Disable Behavior

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-key-repeat-toggle.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-key-repeat-toggle.yaml)

Declare the behavior with `#binding-cells = <1>`, then bind `&repeat_toggle 1` to enable auto-repeat or `&repeat_toggle 0` to disable it. Disabling auto-repeat does not disable normal key presses.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-key-repeat-rate.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-key-repeat-rate.yaml)

Applies to: `compatible = "zmk,behavior-key-repeat-rate"`

| Property         | Type | Description   | Default |
| ---------------- | ---- | ------------- | ------- |
| `#binding-cells` | int  | Must be `<1>` |         |

Example:

```dts
repeat_rate: repeat_rate {
    compatible = "zmk,behavior-key-repeat-rate";
    #binding-cells = <1>;
};
```

## Macro

Creates a custom behavior which triggers a sequence of other behaviors.

See the [macro behavior](../behaviors/macros.md) documentation for more details and examples.

### Kconfig

| Config                             | Type | Description                            | Default |
| ---------------------------------- | ---- | -------------------------------------- | ------- |
| `CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS` | int  | Default value for `wait-ms` in macros. | 15      |
| `CONFIG_ZMK_MACRO_DEFAULT_TAP_MS`  | int  | Default value for `tap-ms` in macros.  | 30      |

### Devicetree

Definition files:

- [zmk/app/dts/bindings/behaviors/zmk,behavior-macro.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-macro.yaml)
- [zmk/app/dts/bindings/behaviors/zmk,behavior-macro-one-param.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-macro-one-param.yaml)
- [zmk/app/dts/bindings/behaviors/zmk,behavior-macro-two-param.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-macro-two-param.yaml)

| Property         | Type          | Description                                                                                                                                                                                          | Default                            |
| ---------------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| `compatible`     | string        | Macro type, **must be _one_ of**:<ul><li>`"zmk,behavior-macro"`</li><li>`"zmk,behavior-macro-one-param"`</li><li>`"zmk,behavior-macro-two-param"`</li></ul>                                          |                                    |
| `#binding-cells` | int           | Must be <ul><li>`<0>` if `compatible = "zmk,behavior-macro"`</li><li>`<1>` if `compatible = "zmk,behavior-macro-one-param"`</li><li>`<2>` if `compatible = "zmk,behavior-macro-two-param"`</li></ul> |                                    |
| `bindings`       | phandle array | List of behaviors to trigger                                                                                                                                                                         |                                    |
| `wait-ms`        | int           | The default time to wait (in milliseconds) before triggering the next behavior.                                                                                                                      | `CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS` |
| `tap-ms`         | int           | The default time to wait (in milliseconds) between the press and release events of a tapped behavior.                                                                                                | `CONFIG_ZMK_MACRO_DEFAULT_TAP_MS`  |

With `compatible = "zmk,behavior-macro-one-param"` or `compatible = "zmk,behavior-macro-two-param"`, this behavior forwards the parameters it receives according to the `&macro_param_*` control behaviors noted below.

### Macro Control Behaviors

The following macro-specific behaviors can be added at any point in the `bindings` list to change how the macro triggers subsequent behaviors.

| Behavior                   | Description                                                                                                          |
| -------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| `&macro_tap`               | Switches to tap mode                                                                                                 |
| `&macro_press`             | Switches to press mode                                                                                               |
| `&macro_release`           | Switches to release mode                                                                                             |
| `&macro_pause_for_release` | Pauses the macro until the macro key itself is released                                                              |
| `&macro_wait_time TIME`    | Changes the time to wait (in milliseconds) before triggering the next behavior.                                      |
| `&macro_tap_time TIME`     | Changes the time to wait (in milliseconds) between the press and release events of a tapped behavior.                |
| `&macro_param_1to1`        | Forward the first parameter received by the macro to the first parameter of the next (non-macro control) behavior.   |
| `&macro_param_1to2`        | Forward the first parameter received by the macro to the second parameter of the next (non-macro control) behavior.  |
| `&macro_param_2to1`        | Forward the second parameter received by the macro to the first parameter of the next (non-macro control) behavior.  |
| `&macro_param_2to2`        | Forward the second parameter received by the macro to the second parameter of the next (non-macro control) behavior. |

## Mod-Morph

Creates a custom behavior that triggers one of two behaviors depending on whether certain modifiers are held.

See the [mod-morph behavior](../behaviors/mod-morph.md) documentation for more details and examples.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-mod-morph.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-mod-morph.yaml)

Applies to: `compatible = "zmk,behavior-mod-morph"`

| Property         | Type          | Description                                                                       |
| ---------------- | ------------- | --------------------------------------------------------------------------------- |
| `#binding-cells` | int           | Must be `<0>`                                                                     |
| `bindings`       | phandle array | A list of two behaviors: one for normal press and one for mod morphed press       |
| `mods`           | int           | A bit field of modifiers. The morph behavior is used if any of these are pressed. |

See [dt-bindings/zmk/modifiers.h](https://github.com/zmkfirmware/zmk/blob/main/app/include/dt-bindings/zmk/modifiers.h) for a list of modifiers.

You can use the following nodes to tweak the default behaviors:

| Node     | Behavior                                  |
| -------- | ----------------------------------------- |
| `&gresc` | [Grave escape](../behaviors/mod-morph.md) |

## Sensor Rotation

Creates a custom behavior which sends a tap of other behaviors when a sensor is rotated.
Has two variants: with `compatible = "zmk,behavior-sensor-rotate"` it accepts no parameters when used, whereas with `compatible = "zmk,behavior-sensor-rotate-var"` it accepts two parameters.

See the [sensor rotation behavior](../behaviors/sensor-rotate.md) documentation for more details and examples.

### Devicetree

Definition files:

- [zmk/app/dts/bindings/behaviors/zmk,behavior-sensor-rotate.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-sensor-rotate.yaml)
- [zmk/app/dts/bindings/behaviors/zmk,behavior-sensor-rotate-var.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-sensor-rotate-var.yaml)

| Property                | Type          | Description                                                                                                                                                                        | Default |
| ----------------------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------- |
| `compatible`            | string        | Sensor rotation type, **must be _one_ of**: <ul><li>`"zmk,behavior-sensor-rotate"`</li><li>`"zmk,behavior-sensor-rotate-var"`</li></ul>                                            |         |
| `#sensor-binding-cells` | int           | Must be <ul><li>`<0>` if `compatible = "zmk,behavior-sensor-rotate"`</li><li>`<2>` if `compatible = "zmk,behavior-sensor-rotate-var"`</li></ul>                                    |         |
| `bindings`              | phandle array | A list of two behaviors to trigger for each rotation direction, must include parameters for `"zmk,behavior-sensor-rotate"` and exclude them for `"zmk,behavior-sensor-rotate-var"` |         |
| `tap-ms`                | int           | The tap duration (between press and release events) in milliseconds for behaviors in `bindings`                                                                                    | 5       |

With `compatible = "zmk,behavior-sensor-rotate-var"`, this behavior forwards the first parameter it receives to the parameter of the first behavior specified in `bindings`, and second parameter to the parameter of the second behavior.

## Sticky Key

Creates a custom behavior that triggers a behavior and keeps it pressed it until another key is pressed and released.

See the [sticky key behavior](../behaviors/sticky-key.md) and [sticky layer behavior](../behaviors/sticky-layer.md) documentation for more details and examples.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-sticky-key.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-sticky-key.yaml)

Applies to: `compatible = "zmk,behavior-sticky-key"`

| Property           | Type          | Description                                                              | Default |
| ------------------ | ------------- | ------------------------------------------------------------------------ | ------- |
| `#binding-cells`   | int           | Must be `<1>`                                                            |         |
| `bindings`         | phandle array | A behavior (without parameters) to trigger                               |         |
| `release-after-ms` | int           | Releases the key after this many milliseconds if no other key is pressed | 1000    |
| `quick-release`    | bool          | Release the sticky key on the next key press instead of release          | false   |
| `lazy`             | bool          | Wait until the next key press to activate the sticky key behavior        | false   |
| `ignore-modifiers` | bool          | If enabled, pressing a modifier key does not cancel the sticky key       | true    |

This behavior forwards the one parameter it receives to the parameter of the behavior specified in `bindings`.

You can use the following nodes to tweak the default behaviors:

| Node  | Behavior                                     |
| ----- | -------------------------------------------- |
| `&sk` | [Sticky key](../behaviors/sticky-key.md)     |
| `&sl` | [Sticky layer](../behaviors/sticky-layer.md) |

## Tap Dance

Creates a custom behavior that triggers a different behavior corresponding to the number of times the key is tapped.

See the [tap dance behavior](../behaviors/tap-dance.mdx) documentation for more details and examples.

### Devicetree

Definition file: [zmk/app/dts/bindings/behaviors/zmk,behavior-tap-dance.yaml](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-tap-dance.yaml)

Applies to: `compatible = "zmk,behavior-tap-dance"`

| Property          | Type          | Description                                                                                  | Default |
| ----------------- | ------------- | -------------------------------------------------------------------------------------------- | ------- |
| `#binding-cells`  | int           | Must be `<0>`                                                                                |         |
| `bindings`        | phandle array | A list of behaviors from which to select                                                     |         |
| `tapping-term-ms` | int           | The maximum time (in milliseconds) between taps before an item from `bindings` is triggered. | 200     |
