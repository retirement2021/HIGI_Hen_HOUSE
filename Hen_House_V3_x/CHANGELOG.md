# Hen House firmware changelog

Version history for the ESP32-S3 chicken coop controller. Newest version first.

The version shown on the OLED, the dashboard header and the boot line (`BOOT <version>`) comes from `systemVersion` in the sketch and always matches the newest heading below.

## Build settings (Arduino IDE)

Not a firmware version. Every folder in this series is built with these exact Tools menu settings. If the IDE has been used for another project, restore these before compiling.

Board: **ESP32S3 Dev Module**, package **esp32:esp32 3.3.12**.

| Tools item | Value |
|---|---|
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Disabled |
| USB Firmware MSC On Boot | Disabled |
| USB DFU On Boot | Disabled |
| Upload Mode | UART0 / Hardware CDC |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| Core Debug Level | None |
| PSRAM | Disabled |
| Arduino Runs On | Core 1 |
| Events Run On | Core 1 |
| Erase All Flash Before Sketch Upload | Disabled |
| JTAG Adapter | Disabled |
| Zigbee Mode | Disabled |

The same settings as a single FQBN, for arduino-cli:

```
esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,DebugLevel=none,PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default
```

The serial Port is not part of the FQBN and is selected separately in the IDE.

## V3.63

- Dashboard: the title card now uses the same sunset colours as CoopCam, so both dashboards match through the sunset phase. Every other phase already matched.
- Dashboard: the inside and outside temperature cards now show the time at which each daily minimum and maximum was recorded.
- The Min/Max lines are now a shared four-column grid (time, label, value, degree), so the time, label, number and degree sign line up on both rows in every state, including negative temperatures and the no-data placeholder.
- The times come from the values already tracked for the OLED and saved in NVS, so there is no new sampling, storage or NVS key.
- The dashboard API has four new read-only fields: `inMinTime`, `inMaxTime`, `outMinTime`, `outMaxTime`.
- Sketch header: clarified that Wi-Fi is optional and needs only a weak signal for NTP time sync, while the dashboard and OTA updates need a stronger, steadier signal.

### V3.63 amendment — humidity card palette (16 Nov 2026)

The inside and outside humidity cards now use the CoopCam moisture palette for
both their border and their body tint, instead of borrowing the temperature
border of their side of the coop. The version number is unchanged. The source
before this edit is kept as `Dashboard.ino.pre-hum-2026-11-16.bak`.

- Before: `setTempCard()` copied the inside/outside temperature border onto
  `cardInHum` / `cardOutHum`, and their tint stayed fixed at `#12304d`. The
  humidity cards therefore showed temperature, not moisture.
- New `setHumCard()` and per-card band state, the same shape as CoopCam's:
  `HUM_HYST` **3 %RH**, six border/tint pairs (tan, light cyan, teal, blue,
  indigo, violet), and neutral grey with `--` when the reading is missing.
- Inside Humidity uses the base edges `[30, 40, 70, 80, 90]`.
- Outside Humidity uses the same palette shifted **+15 %RH**:
  `[45, 55, 85, 92, 96]`, so ordinary UK damp (55–85 %) reads teal instead of
  blue. The last edge is 96 rather than 100 so the violet band can still appear
  in fog or rain (with hysteresis it lights from 99 %).
- The temperature cards, their bands and the Coop Log accent (which still
  follows the outside temperature) are unchanged. `blank()` resets both humidity
  cards to grey along with everything else.
- Live check at `192.168.1.90`: inside 69 % → teal `#35bdb5` / `#0b3f45`,
  outside 93 % → indigo `#5555c8` / `#292a50`. During the first minute after
  boot, while the outside sensor had no reading yet, the card correctly showed
  the grey `#5a6578` / `#222a38` fallback and `--`. Card geometry unchanged
  (81.8 × 112.3 px, no overflow).
- Build: **1,138,592 bytes (36 %)** of program space, **76,368 bytes (23 %)** of
  RAM, no warnings.
- Flashed over OTA on 16 Nov 2026, together with the sunset title palette
  already recorded at the top of the V3.63 list.

### V3.63 amendment — bottom-row height and strike (21 Nov 2026)

The CoopCam page grew after the Lens Heater tile replaced the Clock tile. The
Hen House bottom row is now the height-matcher, so both dashboards finish at
the same point again.

- `.r4` has a 90 px minimum, with 99 px at 420–429 px and 92 px at 430 px and
  above, following the step in CoopCam's top row.
- The four bottom tiles use `justify-content:space-between`, with each label in
  a full-width box carrying an absolutely positioned strike line. The line adds
  no height and matches CoopCam's bottom row.
- Checked at 390 px: both pages render 622 px tall. At 1200 px both render
  649 px.

### V3.63 amendment — humidity Min/Max on dashboard (21 Nov 2026)

- Inside and outside humidity cards now show stacked `Min` and `Max` values
  under the live reading, using the same pale-blue/orange colours as the
  temperature Min/Max rows. No timestamps are shown, because the cards are only
  a quarter of the page wide.
- The dashboard JSON gains `inHumMin`, `inHumMax`, `outHumMin` and `outHumMax`.
  These values were already tracked for the OLED and NVS, so this adds no new
  sampling or storage.
- Validity uses the recorded min/max time strings rather than `dashMin()`,
  which would wrongly blank a legitimate 99–100 %RH reading.
- Humidity card borders and tints are unchanged and still come from
  `setHumCard()`.
- Height for the two extra rows was taken from the temperature row. The
  `.temps` cards and midstack are now 165 px with small responsive steps; both
  dashboards measure within 1 px of each other from 360 px to 1200 px.
- Build: **1,220,062 bytes (38 %)** of program space, **78,284 bytes (23 %)** of
  RAM, no warnings.

## V3.62

- Dashboard trend arrows: made the temperature and humidity arrows more responsive.
- They now compare the newest reading with the oldest of the last 3 samples (about 26 minutes inside, 18 minutes outside) instead of the last 7 (about 78 and 54 minutes).
- Deadbands reduced from 0.5 C / 1.0 %RH to 0.2 C / 0.5 %RH.
- No change to sampling, the API, the dashboard JavaScript or NVS storage.

## V3.61

- Display: the screen no longer redraws itself continuously on the Manual, Events and Environment pages.
- Inside sensor: invalid readings now write a complete log line (attempts and failure count) and raise "SHT31 Fault" after 10 failures in a row.
- Inside sensor: a reading older than 3 hours is now treated as missing, the same way the outside sensor already behaves. The Environment "Now" page shows "--.- C" and the dashboard shows "--" instead of a stale number, and a one-off "SHT31 Stale" event is logged.
- Outside sensor: failed readings now feed the outside trend history, write a complete log line, and raise "SHT30 OUT Fault" after 10 failures in a row.
- Outside sensor: removed a duplicated block that stored the same valid reading twice.
- Cold delay: a delayed morning opening is now cleared when the day changes, when the door is already open, or when a fresh morning decision is made; the dashboard no longer shows a delay that is no longer pending.
- Door: after the third obstruction retry, the door now makes one final forced close instead of staying open. It stops when the motor times out, then raises "Door blocked".
- Coop light: the scheduled fade now updates every 100 ms instead of once a minute, so the sunset fade is smooth.
- Coop light: the manual bedtime hold keeps full brightness for its first minute, then fades over the final minutes.
- Coop light: turning the light off now uses one shared helper, so the on/off state and event log stay consistent.
- Coop light: PWM is only set up when dimming is enabled; otherwise the pin is switched directly on/off.
- Dead-code cleanup, no behaviour change: removed five unused functions (drawTemperature(), oledPrint(), updateFaultBuzzer(), automationAllowed() and canAutoAct()), plus unused and write-only variables left over from earlier versions (old night-LED and physical-button flags, unused event display buffers, buzzerState, lastBuzzerAlert, currentDST, and coldDelayDay/coldDelayLogged bookkeeping).
- Dead-code cleanup: removed the unused historyTimeout and EVENT_SCROLL_TIMEOUT_MS; the event view never had a working 10-second timeout.
- Dead-code cleanup: removed the unreachable CLOSE LIMIT FAIL branch after confirming that no triggerFault() call passes a reason containing "CLOSE". The live OPEN retry path is unchanged.
- Dead-code cleanup: removed an unused sunriseStart local and an unused esp_system.h include in Dashboard.ino; removed stale commented-out code and fixed trailing backslashes in comments.
- Cosmetic clean-up, no behaviour change: removed two stale commented-out debug lines from the WiFi connect/lost handlers.
- Cosmetic clean-up: removed the duplicate RemoteLog.h and HumTrend.h includes from Dashboard.ino; the main sketch includes both before Dashboard is compiled.
- Flash use reduced by about 236 bytes and RAM by about 16 bytes.
- NVS storage is intentionally unchanged in this version.
- Version history moved from the top of the sketch into this CHANGELOG.md file.

## V3.6

- Added trend arrows to the inside and outside temperature cards, using the same method as the humidity arrows.
- The arrow compares the newest sensor reading with the oldest of the last 7 readings. A change of 0.5C or more shows up/down, otherwise steady.
- The arrow is blank until 3 readings have been taken after a reboot, and is cleared by a failed sensor read.
- Both temperature cards now use the same thermometer icon. The bulb and stem take the card's temperature band colour, so they change colour as the temperature rises and falls.
- The Timezone card on the dashboard is replaced by a Cold Delay card. It has a blue border, and the pill shows the cold delay setpoint (outsideTempTriggerC).
- The pill turns blue when the blue cold LED condition is true or a cold door delay is pending, amber OVERRIDE when forceOpenDespiteCold is on, and grey OFF when the outside sensor is disabled.
- The GMT/BST value is no longer sent to the dashboard.

## V3.5

- Added gradual PWM dimming to the coop light. The light now fades smoothly to near-zero over the last 2 minutes of its scheduled ON window, instead of cutting off abruptly.
- The coop light on LIGHT_PIN (GPIO 17) is now driven by PWM through an XY-MOS MOSFET trigger module, replacing the simple relay on/off.
- The fade is derived from the existing scheduled light-OFF time, so changing lightOnOffsetBST/GMT or lightOffMinutesBST/GMT automatically moves the fade with it.
- Two fade curves are selectable in USER SETTINGS: linear, or perceptual (gamma-corrected), which holds brightness longer and eases off gently like a real sunset.
- Fade duration, near-zero floor and gamma are all adjustable in the USER SETTINGS block, alongside the light offsets.
- All light control now goes through a single setLightDuty() helper, so manual overrides and the auto schedule share the same brightness path.
- setLightDuty() falls back to plain on/off if enableLightDimming is set false, preserving the old behaviour.
- Migrated to the ESP32 Arduino core 3.x LEDC API (ledcAttach / ledcWrite by pin), replacing the removed ledcSetup / ledcAttachPin functions.

## V3.04

- Log lines are now stamped with the clock time as [HH:MM:SS] instead of uptime. The time is local (GMT/BST) and comes from the same system clock as getTime().
- Lines logged before the clock is valid (early in setup, until NTP or the RTC seeds it) show [--:--:--]. Uptime is still shown on the dashboard.
- The stamp is only added to the log buffer shown on the dashboard, the USB serial monitor output is unchanged.

## V3.4

- The dashboard can now close the coop door, during the bedtime window only. This is the first dashboard control. Everything else on the dashboard is still read-only.
- The door pill turns gold and glows when closing is allowed. Tap it to close the door. When it is not gold, tapping does nothing.
- Closing is allowed only while the bedtime warning is active (the coop light has come on and the door has not yet closed)
- The page only sends the request. The board checks every condition again before moving the door and refuses (HTTP 409) if any has changed since the page last updated.
- A dashboard close uses the same manual close routine as the manual button. It logs "Manual CLOSE", stops the bedtime countdown,then starts the alternating night flash at the bottom limit.
- The coop light stays on for MANUAL_LIGHT_HOLD_MS after a dashboard close, then switches off.
- Added a trend arrow to the inside and outside humidity cards: up, steady or down.
- The board samples the humidity each time a sensor reading is taken (every 13 minutes inside, 9 minutes outside) and keeps the last 7.
- A change of 1.0 %RH or more across the window shows up or down, anything smaller shows steady. HUM_TREND_DEADBAND in Dashboard.ino sets the 1.0.
- A failed or missing reading clears that sensor's history, so an old arrow cannot stay on screen. History is held in RAM and restarts after a reboot.
- The trend is calculated on the board, so it survives page reloads and is the same on every device.

## V3.3

- Added a remote log. Everything printed with Log.print / Log.println / Log.printf,  it is still sent to Serial, so the USB serial monitor works as before.
- Each log line is stamped with uptime as h:mm:ss. The log is emptied by a restart.
- All Serial.print calls in the main sketch, Dashboard.ino and OTA.ino are now Log.print. Serial.begin is unchanged.
- A "Coop Log" tile. Tapping it opens a collapsible log card. Clear only clears the screen, the board keeps its buffer.
- The log page only requests data while the card is open. The data comes from /api/log and the dashboard has no separate log page.
- The log shows "device restarted" after a reboot and "some lines were missed" if the page falls too far behind the buffer.
- The log is paused with the rest of the dashboard while the door is moving, and catches up afterwards.
- Added a boot line at startup: BOOT <version> reset=<reason> prevLoopStage=<n>. The reason is one of POWERON, EXT, SW, PANIC, INT_WDT, TASK_WDT, WDT, DEEPSLEEP, BROWNOUT or OTHER.
- Added a colour change to the temperature cards to reflect the current temperatures

## V3.2

- Rearranged the cards on the dashboard.
- Added today's minimum and maximum temperature to the inside and outside temperature cards.
- Added an outside humidity card.

## V3.1

- Redesigned the dashboard, which looked blocky and unprofessional, to give it a cleaner, more professional look.
- Added a temperature Difference card (inside minus outside temperature) and a Timezone card (GMT or BST).

## V3.0

- Added a read-only web dashboard, viewable on a PC or phone on the home network. The address is printed to Serial as "Dashboard: http://<ip>" each time WiFi connects.
- Dashboard shows inside and outside temperature with today's min and max, the difference between them, inside humidity, door position, coop light, DAY/NIGHT mode, today's door and light times, RTC status, WiFi signal and uptime.
- Dashboard header shows the firmware version taken from systemVersion, so it follows the version number automatically.
- Dashboard code lives in its own tab (Dashboard.ino). The main sketch only calls dashboardSetup(), dashboardOnWifiConnected(), dashboardOnWifiLost() and dashboardLoop() (loopStage 16).
- The web server stops when WiFi is lost and restarts when it reconnects. The existing WiFi retry and NTP logic is unchanged.
- The web server is paused while the door is moving, so a stalled browser connection cannot delay the limit switch checks. The page holds its last values briefly and shows Offline until the door stops.
- A failed or missing reading is shown as "--" on the dashboard instead of breaking the page, and the outside temperature is hidden once its reading is older than OUTSIDE_TEMP_STALE_MS.
- A door fault or system lockout shows as a red FAULT on the dashboard.
- Dashboard uptime uses the ESP32 hardware timer, so it does not reset at the 49.7 day millis() rollover.
- Set the WiFi hostname to HenHouse-V3 so the board shows a name on the router.
- Partition scheme changed to a 16MB layout with two 3MB app slots (room for future OTA updates). "Erase All Flash Before Sketch Upload" is now Disabled so the NVS history survives a USB upload.

## V2.22

- Environmental history rollover now saves the active calendar date in NVS, preventing loss of daily date tracking after a restart or power interruption.
- Daily history is archived using the saved calendar date rather than subtracting 24 hours, avoiding incorrect dates during BST/GMT clock changes.
- Rollover now runs before environmental sensor updates, ensuring the first readings of a new day are recorded against the correct daily statistics.
- Added protection against environmental history rollover when the system clock moves backwards.

## V2.21

- Motor outputs are now set LOW as the first lines of setup(), before the OLED, NVS and welcome screen, so the motor pins never float during boot or after a watchdog reset.
- Clearing a fault now re-derives the door state from the limit switches. If the door was stopped mid-travel, the morning auto-open check is re-armed.
- Outside temperature is now only trusted for 3 hours after the last valid reading. A failed or disconnected SHT30 no longer holds a stale cold value that would delay the door or keep the Cold LED on.
- A "WiFi Disabled" is now logged once per outage instead of every 30 minutes, to stop a long WiFi outage filling the event log.
- Fixed millis() rollover (49.7 days uptime) in the manual light timeout, post-manual-close light hold and display notice timers, which could expire instantly if set just before rollover.
- Added a no-valid-time alarm: if neither NTP nor the RTC provides a time for 1 hour, the red LED flashes slowly and the buzzer sounds, and "No time since HH:MM" is logged once. Door control is not blocked.

## V2.20

- Manual bedtime close: stops the homing LED sequence, holds both night LEDs solid ON, then starts the alternating night flash when the bottom limit is reached.
- Coop light now stays on for MANUAL_LIGHT_HOLD_MS __ minutes after a manual bedtime close, then switches off.
- A manual close now cancels any in-flight safety recovery and resets the obstruction retry counter.
- Added a manual-lock gate to startOpening()/startClosing() and a re-entry guard that stops an in-flight move before a manual command, preventing spurious Close Obstruction.
- Obstruction reopen no longer clears the bedtime indicators when a manual bedtime close is pending.

## V2.19

- Added catch-up door closing: if the door is not closed after the close time, it now closes on the next once-a-minute check (power cut, reset or fault cleared after dusk).
- Fixed the Manual page rotary encoder using stale position data after leaving and re-entering the page, which could trigger a door move on a single click.
- Watchdog now falls back to esp_task_wdt_reconfigure() when the core has already initialised it, so the 35 second timeout and panic reset apply.
- A "Time failed" is now logged once per outage instead of every minute, to stop it filling the event log.
- Door Obstruction retry counter is now reset when a fault is cleared, so the next obstruction gets its full release retries.

## V2.18

- Added a page change debounce / minimum press filter (30 ms) as some pages could be missed when scrolling between pages.
- Updated the the 5+ second encoder fault clear logic, once 5 seconds have passed it will reset while holding the button in as opposed to resetting on release.

## V2.17

- DST transition validation: confirms the system will change automatically to BST/GMT at the correct seasonal timing signal.
- The sketch now runs on local time with a DST-aware timezone rule, so door and lighting schedules adjust correctly across BST/GMT changeover.
- Added a GMT/BST flag to the title of Today's schedule page.

## V2.16

- Fix timestamp positioning shift caused by single-digit temperature values on Today's Internal and External display pages.

## V2.15

- One month soak test in working coop.
- Modified the NVS data structure to minimize save writes and reducing the target storage duration for long-term stability.

## V2.14

- Standardised NVS logging implementation to resolve saving errors on specific pages.

## V2.13

- Remove the current time and 1 second refresh on System Time page and replace with Cold threshhold temp.
- Change the title of that page to Cold & Daytime.

## V2.12

- Amalgamated inside and outside environmental pages into one, for convenient viewing.

## V2.10

- removed the clear persistent environment logs on the next upload as it seemed the culprit to NVS logging fails.

## 2.04

- Rewrote the Environment Outside pages inline with the Environment Inside pages for consistency.
- Futher refinement to the cold delay logic to poll the external temperature sensor immediately before the door opens, then reset the polling timer.

## V2.02

- Refined cold delay logic to evaluate temperature immediately prior to door opening, logging a "Cold Delay" event when triggered.
- Added a option to clear internal and external persistent environment logs on the next upload.

## V2.01

- Added support for an SHT30 outdoor temperature probe at address 0x44, complete with dedicated display pages and logging.
- Replaced the Adafruit_SHT31 library with SensirionI2CSHT3x to support dual SHT3x sensors on separate I2C addresses.
- Updated the I2C address for the SHT31 coop sensor to 0x45.
- Implemented a cold weather morning door delay when outdoor temperatures fall below a configurable threshold.
- Added a 24/7 Cold (blue) LED indicator that illuminates when ambient temperature drops below the configured setpoint.

## V2.0

- Changed from a DHT22 coop temp sensor to a SHT31 sensor.
