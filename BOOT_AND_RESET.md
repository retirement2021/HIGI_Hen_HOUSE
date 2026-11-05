# Boot and Reset Messages

At startup, the ESP32 logs a line like:

```text
[HH:MM:SS] BOOT V3.6 reset=SW(3) prevLoopStage=7
```

The timestamp is local time. If the clock is not available yet, it appears as `[--:--:--]`.

## Reading the boot line

- **`BOOT V3.6`** — firmware version that started.
- **`reset=SW(3)`** — reset reason. `SW` is the readable reset name; `(3)` is its numeric code.
- **`prevLoopStage=7`** — the last loop stage recorded before the reset. This can help narrow down where the controller was working, but it does **not** prove that stage caused the reset.

For example, `reset=SW(3) prevLoopStage=7` means the ESP32 reported a software reset. Stage 7 was the last recorded loop stage before setup began.

## Reset reasons

The sketch converts the ESP32 reset-reason code into a name:

| Name | Meaning |
|---|---|
| `POWERON` | Power-on reset |
| `EXT` | External reset, such as the reset pin |
| `SW` | Software-requested reset |
| `PANIC` | Software panic or fatal error |
| `INT_WDT` | Interrupt watchdog reset |
| `TASK_WDT` | Task watchdog reset |
| `WDT` | Other watchdog reset |
| `DEEPSLEEP` | Reset after waking from deep sleep |
| `BROWNOUT` | Supply voltage fell below the safe level |
| `OTHER` | Reset reason not listed above |

Use the **name** to interpret the reset. The numeric value is included for diagnostics and can depend on the ESP32 platform definitions.

## Previous loop-stage numbers

The sketch records a stage number as it enters each part of `loop()`:

| Stage | Activity |
|---:|---|
| 1 | Environment history rollover |
| 2 | Inside sensor update |
| 3 | Outside sensor update |
| 4 | Rotary encoder and manual switch handling |
| 5 | Event and environment page input |
| 6 | Door movement and limit-switch update |
| 7 | Door safety and obstruction-recovery cycle |
| 8 | Motor safety check |
| 9 | Buzzer pattern |
| 10 | Status LEDs |
| 11 | Night LEDs |
| 12 | Post-close lighting hold |
| 13 | Wi-Fi connection handling |
| 14 | Display refresh |
| 15 | Manual-light timeout, OLED sleep, and once-a-minute tasks |
| 16 | Dashboard web server |
| 17 | OTA handler |

### Interpreting the stage

`prevLoopStage` is a diagnostic clue, not a fault diagnosis. It records the last stage value written by the sketch; the reset might have happened during that stage or later, before another stage value was recorded.

Stage 15 contains several different tasks, so that value alone cannot identify which one was running. On a normal power-on, the saved stage may also be meaningless because it is held in RTC memory that is not initialised on power-up.

## Additional restart events

After the boot line, the event log may show:

- **`Watchdog Reset s<n>`** — the reset was reported as a task-watchdog reset; `<n>` is the saved previous loop stage.
- **`Software Reset`** — an additional event for a software reset.
- **`System Restart`** — an additional event for other reset reasons.

A normal power-on does not add one of these restart events.