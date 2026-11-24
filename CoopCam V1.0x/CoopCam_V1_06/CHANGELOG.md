# CoopCam firmware changelog

Version history for the ESP32-S3 chicken-coop camera and IR lamp controller.
Newest version first.

The version shown in the dashboard header and in the serial boot line comes from
`FW_VERSION` in the main sketch, and always matches the newest heading below.

## V1.06

Version cut from V1.05. V1.05 stays untouched as the full rollback.

- `FW_VERSION` bumped to V1.06 and the main sketch renamed to match the folder.
- Added optional GPIO6 lens heater for a 12 V / 7 W pad or band. The YX-MOS is
  wired as a low-side switch: + passes through, − is switched.
- `HEATER_ENABLED` lives in `USER SETTINGS` only and defaults to `false`. While
  false, GPIO6 is held OFF and the dashboard tile is visible but inactive.
- Heater manual timer is adjustable with `HEATER_MANUAL_MINUTES` and defaults to
  30 minutes. The heater tile is the single button: tap to start, tap to cancel.
- Automatic heat uses SHT4x dew point only, while the camera is ON and the air
  is at or below `HEATER_AUTO_MAX_AMBIENT_C` (default 6.0 °C). ON when
  `temp - dew <= HEATER_DEW_MARGIN_ON_C`, OFF when
  `temp - dew >= HEATER_DEW_MARGIN_OFF_C`, otherwise hold the previous state.
- Added `HEATER_MAX_AMBIENT_C` as a hard high-temperature cut-out. Default
  cut-out is 18.0 °C. It applies to automatic and manual heat.
- Any missing, stale or invalid SHT4x sample forces the heater OFF and cancels
  manual mode. The heater cannot run without a fresh sensor reading.
- Heater state changes are logged to the timestamped Serial Log / RemoteLog.
- JSON status gains `heater`, `heaterEnabled`, `heaterManual`, `heaterMin`,
  `heaterReason`, `dewPoint` and `heaterGap`. While the heater is enabled,
  `next` is simplified to `Heat ON` or `Heat OFF`.
- Dashboard: the Lens Heater tile replaces the Clock tile in the bottom row.
  It keeps the normal rectangular card shape, mirrors the temperature tile's
  border colour while Sunrise/Sunset mirrors the humidity tile, shows `N/A` when
  disabled, and puts its pill at the bottom like
  the Automatic and Serial Log tiles. Manual heating shows only the minutes left
  in the pill, with no extra text line that would stretch the row. All four
  bottom tiles now share one strike-line position. Sunrise/Sunset and Serial Log
  stay in place, so the row still has four cards.
- Dashboard: Coop Camera and IR Illuminator each use one centred ON/OFF toggle.
  Gold shows the current load state; tapping flips it using the existing
  camera/lamp override endpoints.
- Dashboard: the IR Brightness card has a dynamic tooltip. When IR is OFF it
  says brightness can only be adjusted with the lamp ON; when IR is ON it shows
  the 1% per press, temporary-until-reboot guidance.

## Build settings (Arduino IDE)

Not a firmware version. Every folder in this series is built with these settings.

Board: **ESP32S3 Dev Module**, package **esp32:esp32 3.3.12**.

| Tools item | Value |
|---|---|
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| PSRAM | Disabled |
| USB CDC On Boot | Enabled (so Serial works over USB) |

Libraries: **RTClib** by Adafruit (pulls in Adafruit BusIO), **Sensirion I2C SHT4x**
by Sensirion (pulls in Sensirion Core).

## How the folders work

One folder per version, same as the Hen House sketches:

```
CoopCam_V<version>/          <- current build
    CoopCam_V<version>.ino   <- main sketch, name matches the folder
    Dashboard.ino            <- dashboard page
    RemoteLog.h              <- serial-log ring buffer (V1.03 onwards)
    secrets.h
    CHANGELOG.md             <- copy of this file as it stood at that version

CoopCam_Storage/
    A_CoopCam V1's/          <- older versions, moved here when superseded
```

- The main `.ino` must match the folder name or the Arduino IDE will not open it.
  Secondary tabs (`Dashboard.ino`) keep their names.
- Every folder carries its own `secrets.h`, so an archived version is complete
  and can be opened and uploaded on its own.
- `FW_VERSION` only changes in the same step that a new folder is copied. No
  folder, no new number.
- A cut happens only when you ask for one. The version number is never bumped on
  the assistant's own initiative, however large the change.
- The top level keeps the current build and the one before it; anything older is
  moved into `CoopCam_Storage` under its major version.

## V1.05

Working copy opened from V1.04 as a version cut, then the dashboard rebuilt into
the approved four-row layout. V1.04 stays untouched beside it as the rollback.

- `FW_VERSION` bumped to V1.05 and the sketch and folder renamed. That is the
  only change in `CoopCam_V1_05.ino`; everything else below is in
  `Dashboard.ino`.
- **Rows reordered.** Watch Camera moves up into row 2 beside IR Brightness, and
  the Automatic tile drops to the four-across row with Time, Sunrise and Serial
  Log. The live-stream panel still opens directly under the Watch Camera card,
  and the log panel under row 3.
- **Watch Camera loses its icon.** It is now the same shape as IR Brightness
  (`card duo`): title, hint, and the OPEN pill pinned to the card floor, 11 px up,
  the same as the ON/OFF buttons. The hint carries the stream state as text
  (`Camera off`, `Camera on`, `Stream: OPEN`, `Waking camera`).
- **The four-across row is shorter, 88 px.** Automatic, Time, Sunrise and Serial
  Log lose their icons too and are text-only tiles. Card titles use the standard
  weight rather than bold, and the Time tile keeps its clock as the one large
  value in the row.
- **The Coop High strip is gone.** In its place, at the bottom, are two cards the
  same width as the pair above: one for temperature, one for humidity. Each
  shows its reading with the trend arrow, then `min` with the time, then `max`
  with the time.
- **Banded borders from the Hen House.** Temperature uses the indoor band palette
  with edges **0 / 5 / 10 / 20 / 25 / 30 °C** and **0.3 °C** hysteresis;
  humidity uses the moisture palette with edges **30 / 40 / 70 / 80 / 90 %RH**
  and **3 %RH** hysteresis. A reading has to pass an edge by the hysteresis
  before the colour changes, so a value sitting on a boundary cannot flicker.
  No reading, or the sensor disabled, falls back to neutral grey and `--`.
- Like every other card, the tint underneath follows the border colour, so the
  two readings can never be mistaken for each other at a glance.
- **Coop Camera and IR Illuminator are top-aligned.** Titles sit at 52 px and the
  separators at 88.9 px on both cards; the buttons float to the bottom. Before,
  the two cards drifted apart whenever one gained a schedule line.
- Both camera-off windows are shown as separate `OFF` lines, and the second line
  collapses when that window is disabled.
- `status.json` is unchanged: `tempMax`, `tempMaxAt`, `humMin` and `humMinAt`
  have been in it since the V1.04 amendment, and the new cards paint all four.
- `%ROWCLS%` is no longer used on the page, because the Watch Camera tile is not
  in the four-across row. `handleRoot()` still substitutes it and the result is
  harmless. `%SENSORHIDE%` now hides the whole temperature/humidity row when
  `SENSOR_ENABLED` is false.
- Measured at 375 px: header 73.7 px, rows **196.9 / 88 / 97.4 / 64.3 px**,
  main **594.2 px**, page **618.2 px**. It fits a 375 × 667 phone without
  scrolling, with no clipping, no horizontal overflow and no console errors.
  Each climate card is 170.5 × 64.3 px.
- Build: **1,100,464 bytes (34 %)** of program space, **61,800 bytes (18 %)** of
  RAM, no warnings. Flashed over OTA to `192.168.1.106` on 17 Nov 2026 and
  verified live: 11.9 °C gave the green `#3fcf3a` border, 92 %RH the violet
  `#8a4fd8`, with both camera-off windows on screen.

### V1.05 amendment — matched to the Hen House dashboard (17 Nov 2026)

The version number is unchanged. This pass makes the CoopCam page and the Hen
House page a matched pair, so the two side by side read as one family. Every
change is in `Dashboard.ino`; the server, camera-off windows and override
behaviour are untouched.

- **Title card.** Header padding 12 → **14 px**, so the title card is 77.7 px,
  exactly the Hen House height.
- **Footer gap.** `margin-top` 8 → **18 px**, matching the Hen House, so the
  “Real Time Schedule” line sits on the same 605.3 px line on both.
- **Top-row floor.** The Camera/IR row now carries `.row.bigrow`:
  `min-height` **184 px**, **187.5 px** at 400–419 px, **200 px** from 420 px.
  With the content below, the row lands at 200.0 px against the Hen House 200.0.
- **Top-row text matched** to Coop Lighting and Door Position: card title
  **14 px** (was 13), ON/OFF **14 px** (was 13). Schedule labels (11.7 px) and
  times (14.3 px / 700) were already identical.
- **Card order matched.** Both top cards now read icon → title → ON/OFF buttons
  → schedule, with the buttons where the Hen House has its status pill. The
  IR Illuminator schedule became a two-row `col` like the camera's, so the two
  divider lines are level at **139 px**.
- **Humidity palette replaced** with the approved bands: edges
  **40 / 60 / 80 / 91 / 97 %RH**, **1.5 %RH** hysteresis, amber `#f59e0b`,
  lime `#84cc16`, green `#10b981`, teal `#06b6d4`, blue `#3b82f6`,
  indigo `#6366f1`. Previously 30/40/70/80/90 %RH with 3 %RH and a tan→violet
  ramp.
- Measured live at the desktop width: title card **77.7 px**, top row
  **200.0 px**, divider line **139.0 px**, footer line **605.3 px**, page
  **635.3 px** — all equal to the Hen House within 0.2 px.
- Build: **1,100,848 bytes (34 %)** of program space, **61,800 bytes (18 %)** of
  RAM, no warnings. Flashed over OTA to `192.168.1.106` on 17 Nov 2026 and
  verified live: V1.05, both camera-off windows intact, 95 %RH on the blue
  `#3b82f6` band, cards in the new order.

### V1.05 amendment — climate cards, timestamp first (17 Nov 2026)

The version number is unchanged. The two ceiling-air cards now show the day's
Min and Max the way the Hen House temperature cards do.

- **Order inside each card:** the current reading stays on top, in white with its
  label and trend arrow; the two extreme rows sit underneath.
- **Each Min/Max row reads timestamp, then label, then value** — for example
  `02:08 Min 12.8°` — instead of `min 12.8° · 02:08`.
- **Min value is blue `#8fd0ff` and Max value is orange `#ffb86b`**, the exact
  two colours the Hen House temperature cards use. The current reading is left
  uncoloured.
- **Geometry unchanged:** each climate card is still 64.3 px and the page still
  635.3 px, so the pair alignment with the Hen House is untouched. The new rows
  use the same 9.5 px type and the same 3 px spacing as the old min/max lines.
- Build: **1,101,616 bytes (35 %)** of program space, **61,800 bytes (18 %)** of
  RAM, no warnings. Flashed over OTA to `192.168.1.106` on 17 Nov 2026 and
  verified live: V1.05, both camera-off windows intact, Min `rgb(143,208,255)`,
  Max `rgb(255,184,107)`, card 64.3 px, page 635.3 px.

## V1.04

Working copy opened from V1.03, adding the ceiling temperature and humidity
sensor.

- `FW_VERSION` bumped to V1.04 and the sketch renamed, as the first step of the
  version cut. V1.03 stays at the top level as the rollback.
- New **SHT41** on the DS3231's I2C bus at `0x44`: GPIO8 SDA, GPIO9 SCL, 3.3 V,
  GND. The boot scan now finds two devices, `0x68` and `0x44`, which doubles as
  a free “is the sensor cable still good” check.
- Uses Sensirion's own **Sensirion I2C SHT4x** library, not the Adafruit
  wrapper. Medium repeatability by default (4.5 ms per measurement).
- `SENSOR_READ_MS` in USER SETTINGS is the ping interval, set to **7 minutes**.
  That keeps self-heating negligible and gives 205 samples a day. The card's
  current value is therefore up to seven minutes old; the tooltip shows the
  sample time.
- Values are **RAM only**. There is no NVS wear and a reboot simply starts the
  record again. The min/max and their timestamps also clear at local midnight,
  and the sensor takes a fresh reading then so the new day starts immediately.
- New full-width thin **Coop High** card below the bottom row: `COOP` / `HIGH`
  stacked at the left behind a hairline rule, with temperature and humidity on
  the right. Each reading shows its current value, its trend arrow, and the
  day's corresponding extreme with the time it happened.
- Trend arrows follow the Hen House convention: `↑` rising (warm orange),
  `↓` falling (blue), `↔` steady (grey). At a 7-minute cadence, three samples
  span 14 minutes, so the minimum window is **12 minutes** and the arrow appears
  on the third reading. The deadbands are **0.3 °C** and **1.5 %RH**: wide
  enough that “steady” means the displayed number has not visibly moved.
- The card follows the normal day-phase tile palette. Gold stays reserved for
  a running load; the trend arrows are the only fixed-colour values.
- Missing sensor: the card stays in place and shows `--`, rather than vanishing
  and moving everything below it. If `SENSOR_ENABLED` is false the card is
  hidden entirely.
- Phone fit: the card is **42.4 px** including its border. Three quiet trims
  make room: the header card's padding 14 → 12 px, the footer's top margin
  18 → 8 px, and the Automatic / IR row's minimum height 112 → 104 px. None is
  visible on its own, and all content still fits its card. At 375 × 667 the
  full page now measures **605.6 px** against roughly 615 px usable, so it fits
  with a small margin rather than by a pixel or two.
- `status.json` gains `sensor`, `sensorAt`, `temp`, `hum`, `tempMin`,
  `tempMinAt`, `humMax`, `humMaxAt`, `tempTrend` and `humTrend`. Nulls mean no
  reading yet or a failed read.
- Boot log gains one `SHT41 ceiling` line with the address, serial number and
  read interval. Initial reading is taken at boot, then every seven minutes.
- The extremes are the extremes *of the samples*, not of every second of the
  day. A two-minute damp spike between pings can be missed; that is the trade
  for keeping the heater and self-heating out of the reading.

### V1.04 amendment — ceiling card layout (15 Nov 2026)

Revised after the first V1.04 flash. The version number is deliberately
unchanged: this is an edit to the working copy, not a version cut. The source
as it was flashed is kept beside it as
`CoopCam_V1_04.ino.flashed-2026-11-15.bak` / `Dashboard.ino.flashed-2026-11-15.bak`.

- `SHT41` → `SHT4x` in the boot log, the two card tooltips, the header comments
  and the address constant. The part fitted is an SHT40 waterproof probe;
  `SHT4x` is the family name Sensirion's library uses.
- **Four extremes** instead of two: `tempMax` and `humMin` join `tempMin` and
  `humMax`, each with the time it happened. All four update on the same sample
  as the current reading, so a timestamp is always a real reading time.
- `status.json` gains `tempMax`, `tempMaxAt`, `humMin` and `humMinAt`.
- Card: the `COOP` / `HIGH` label column is gone. Each group is now three lines
  — current value with trend, then `min` with time, then `max` with time — and
  the two groups are centred as one block with a **55 px** gap. Columns are
  `max-content`, so the block re-centres itself as a value gains or loses a
  digit.
- Four quiet trims pay for the third line: card padding 7 → **5 px**, small-line
  leading 1.1 → **1.0**, header padding 12 → **10 px**, footer margin
  8 → **6 px**. Horizontal padding stays 11 px.
- The card is **48.0 px** tall and the page **605.2 px** at 375 px wide, no
  scroll. Checked at 360, 375, 390 and 400 px with normal values, and at 320 px
  with worst-case values (−18.7 °C, 100 %RH): nothing clips. The page as a whole
  still scrolls at 320 px, exactly as it did in the original V1.04.
- Build: **1,098,232 bytes (34 %)** of program space, **61,800 bytes (18 %)**
  of RAM, no warnings.
- Boot log no longer prints `Watch camera : (card hidden)` when the tile is in
  fact on screen. It now says `no stream URL, tile shown` when
  `SHOW_WATCH_CARD_ANYWAY` is keeping it visible, and `no stream URL, card
  hidden` only when the tile really is hidden.
- The empty trend slot takes no layout space (`:empty` → `display:none`), so
  the block is centred from the first sample rather than only once the arrows
  appear. Without it the reserved 3 px flex gap pushed the ink 1.5 px left
  during the first ~14 minutes after boot.

### V1.04 amendment — four-row dashboard (16 Nov 2026)

The dashboard was rebuilt to the approved balanced layout. The version number
stays V1.04: this is a working-copy edit, not a version cut. The source as it was
flashed on 15 Nov is kept as the `*.flashed-2026-11-15.bak` files, and the
pre-rewrite working copy as `*.pre-rows-2026-11-16.bak`.

- **Four rows, one height ladder.** Every card is **170 px** wide. Row 1 (Coop
  Camera / IR Illuminator) is **160 px**, then **120 / 90 / 65 px**. Each row is
  roughly a quarter shorter than the one above, so the page tapers instead of
  reading as four equal bands.
- **Removed:** the Time card, the Sunrise / Sunset card and the full-width Coop
  High strip. The ceiling air now lives inside the Temperature and Humidity
  cards.
- Header tag is now **Camera • IR Lamp • Coop Climate**.
- **Temperature** and **Humidity** are full climate cards: label, current value
  with trend, then two Hen House-style rows (`time`, `Min`/`Max`, value, unit).
  Min values are blue, Max values orange.
- **Temperature bands** copy the Hen House scale: `TEMP_EDGES`
  `[0, 5, 10, 20, 25, 30]`, `TEMP_HYST` **0.3 °C**, seven border/tint pairs.
  16.1 °C reads `#3fcf3a` / `#164022`.
- **Humidity bands** are new: edges `[30, 40, 70, 80, 90]`, `HUM_HYST`
  **3 %RH**, six pairs from `#b98a5a` (dry) through `#35bdb5` (comfortable) to
  `#8a4fd8` (saturated). Hysteresis uses the same climb/fall rule as
  temperature, so a value sitting on an edge does not flicker.
- Both climate cards fall back to neutral grey with `--` dashes when the sensor
  is missing or disabled, and keep their place in the grid.
- **Buttons:** steel means idle. Only the **ON** button lights gold, and only
  while that load is running. The **OFF** button never lights — the old
  `camOffBtn` / `lampOffBtn` gold toggles are gone, so a card that is off stays
  entirely steel. The card border still follows the load.
- Rows 3 and 4 turn horizontal: label and hint on the left, action pill on the
  right. The Watch Camera card loses its icon and now shows stream state as its
  hint (`Camera off`, `Camera on`, `Stream: OPEN`, `Waking camera`).
- **IR Brightness keeps ADJUST as a gate.** `−` and `+` stay hidden until
  ADJUST is tapped; the pill turns gold while armed and the hint changes to
  `1% per press`. The card is a fixed 90 px, so arming cannot make the row grow,
  and the gate re-locks itself whenever the lamp goes off.
- The Watch Camera panel now sits directly below row 3, and the Serial Log
  panel below row 4.
- `%ROWCLS%` now resolves to ` solo` instead of `r3` when the camera is absent,
  so row 3 becomes a single full-width IR Brightness card. `%SENSORHIDE%` is
  gone, because the sensor cards never hide.
- Page height at 375 px is **593.2 px**: header 62.2, rows 160 / 120 / 90 / 65,
  footer 18. No clipping, no horizontal overflow, no console errors. Checked at
  375 px against mocked values and live at `192.168.1.106`, where 8.7 °C / 81 %
  RH produced the `#4fd0e8` and `#5555c8` borders.
- Build: **1,095,168 bytes (34 %)** of program space, **61,800 bytes (18 %)** of
  RAM, no warnings. Flashed over OTA on 16 Nov 2026.

### V1.04 amendment — centred reading, equal Min/Max gaps (16 Nov 2026)

Two CSS-only refinements to the row-2 climate cards. Still V1.04: working-copy
edits, not a version cut. Pre-edit copies are
`Dashboard.ino.pre-equal-2026-11-16.bak` (before both changes) and
`Dashboard.ino.pre-mm-2026-11-16.bak` (before the Min/Max rework).

- **The current reading is now dead centre.** In `.vl` the number, unit and
  trend arrow were centred as one block, which pushed the digits **15.6 px**
  left of centre on `8.7` and **20 px** on `81`, and further still on longer
  values. `.vl` is now a three-column grid (`1fr auto 1fr`) with the number in
  the middle column and the unit plus arrow in a `.suf` cell hanging off its
  right, so the digits sit **0 px** off centre at every width tested (`8.7`,
  `81`, `-1.0`, `10.0`, `100`). Trade-off: the unit and arrow follow the
  number's right edge instead of holding a fixed position.
- **Min/Max rows now have equal gaps.** Previously the 9 px cell gap was not
  what the eye saw: the reading was right-aligned inside a 2.4 em box, so the
  spare width piled up after it. Measured time → label vs label → reading:
  `12 / 16.5` px on the Min row and `10.8 / 15.4` on the Max row; the humidity
  card was worse at `12 / 26.5` and `10.8 / 25.2`.
- **Fix:** the reading and its unit share one fixed-width cell (`.vu`,
  `2.6 em`, left-aligned), the separate unit column is gone, and the label cell
  stays a centred `2.2 em`. Gaps are now **`12 / 12`** on Min rows and
  **`10.8 / 10.8`** on Max rows, on both cards.
- The `°` follows its number, so on a one-digit reading it sits about 7 px
  further left than on a two-digit one. Humidity stays two digits (`100` also
  fits the 2.6 em cell), so that card does not move.
- Residual: "Min" is narrower than "Max", so centring leaves 3.1 px of padding
  each side versus 1.8 px — the two rows differ by about 1.3 px. Inherent to
  centred labels and not visible at phone size.
- Verified against the extracted page at 375 px: card 170.5 px wide, 120 px
  tall, no overflow, no console errors.
- Build: **1,096,240 bytes (34 %)** of program space, **61,800 bytes (18 %)** of
  RAM. Flashed over OTA to `192.168.1.106` on 16 Nov 2026; device returned on
  V1.04 with the sensor `ok`.

## V1.03

Working copy opened from V1.02, now with the dashboard serial log.

- `FW_VERSION` bumped to V1.03 and the sketch renamed, as the first step of any
  version cut. The code is byte-identical to V1.02 apart from the version string.
- The dashboard tab is renamed from `Dashboard_PWM_Ramp.ino` to `Dashboard.ino`.
  Same file, same contents, no code change; the old name was a leftover from the
  first single-file build.
- V1.02 is frozen and stays at the top level until V1.04 is cut.
- `SHOW_WATCH_CARD_ANYWAY` is still `true`, so the Watch Camera card is visible
  while `CAMERA_VIEW_URL` is empty and the camera hardware is not fitted yet.
- Dashboard: the bottom row is split in half — **Watch Camera** on the left and
  **Serial Log** on the right. Both are ordinary card height; nothing is
  permanently enlarged to make room for the log.
- **Serial Log** tile: a `Show` pill reveals a full-width log panel below the
  row, Hen House style. The pill turns gold and reads `Hide` while the panel is
  open. Pause, Clear, auto-scroll, plus "device restarted" and "some lines were
  missed" notices. The log is only fetched while the panel is open, so a closed
  log costs no traffic.
- The log pane never wraps: one logged line stays one line, and long lines
  scroll sideways. This keeps the boot summary's aligned columns intact and
  means the boot messages no longer have to be abbreviated to fit.
- The Serial Log tile and its panel follow the sunrise/sunset title colours,
  like the header and the Watch Camera card.
- The bottom row is now four equal cards: **Watch Camera**, **Time**, **Sunrise / Sunset**,
  **Serial Log**. On phones narrower than 360 px they fall back to two by two,
  and when the Watch Camera card is hidden the other three take the row.
- New **Time** card: `HH:MM` with the date underneath. The page ticks the clock
  itself once a second and resyncs on every poll, so the minute changes on time
  instead of five seconds late. Seconds are not shown: the card is too narrow
  for them and a five-second poll would make them stutter.
- The Time card reads icon, then the time, then the date, then the zone
  (`GMT` / `BST`). The zone used to be a pill, but a pill on this card read as a
  button, so it is now plain text in the same italic font as the date, and both
  are centred in the card.
- The Time card is built from the same blocks as the Sunrise / Sunset card: the
  clock takes the title band, and the date and zone are rows of the same
  schedule block. The divider therefore lands on the neighbouring card's
  divider to the pixel, and the date and zone sit on the Rise and Set baselines
  instead of on positions nudged into place with margins.
- Rise / Set, and the camera's OFF rows, stop just inside the card border
  instead of running hard into it: 5 px on the bottom-row cards, 10 px on the
  wider Coop Camera card. The ON / OFF capsules are left flush so the camera
  and IR Illuminator buttons still line up across the pair.
- The four bottom-row cards get their own vertical body gradient with a lit top
  edge. The shared diagonal tint is fine on the big cards, but at 94 px wide it
  read as one flat colour, which made the Watch, Time and Sun cards look
  unshaded. Measured luminance ramp is now about 50 levels top to bottom.
- When the row falls back to two by two on the narrowest phones, both rows are
  forced to the same height instead of the first row sitting shorter.
- New **Sunrise / Sunset** card: sunrise and sunset, in the same two-row layout as the
  camera's OFF windows.
- The card is titled **Sunrise / Sunset**, not `Sun`, which on a phone read as
  "Sunday".
- The sunset phase moved from copper to **sunset rose**: border `#F27A8C` over
  body `#4E2430`. The old `#EB8C4A` border sat on a `#5B2F17` body, and it was
  that brown body tint behind the whole card that made the screen read as
  copper. Rose is also far from both the amber ON cards and the violet
  night/pre-dawn phases, so sunset stays distinguishable at a glance.
- The **Watch Camera** tile is now a `OPEN` / `HIDE` switch like the Serial Log,
  opening a full-width panel below the row with the stream in an iframe. The
  stream element is created only while the panel is open, so closing it stops
  playback and gives the camera's preview slot back. A `New tab` pill opens the
  same URL directly, for cameras that refuse to be framed. When the relay is
  off, the panel opens at once and runs the wake countdown in place instead of
  navigating away.
- Automatic and IR Brightness now follow the day phase too. Only the two load
  cards keep gold while ON, so gold means exactly one thing on the page: this
  load is running.
- The outline icons (Automatic, Watch Camera, Time, Sunrise / Sunset, Serial Log) are a fixed
  near-white at one stroke weight instead of taking the card colour, matching
  the illustrated camera and bulb icons, which never changed colour either.
- The ON/OFF buttons on the Coop Camera and IR Illuminator cards are proper
  rounded-end capsules now, the same shape as every other pill on the page.
- The Automatic card keeps the `AUTO`/`MANUAL` pill under its title, with the
  hint on its own full-width line. In the old single-row layout the wider
  `MANUAL` pill squeezed the hint into a 22 px column.
- Automatic and IR Brightness sit a little taller (112 px minimum), and the four
  bottom-row cards a little shorter, so the page reads as two compact rows.
- New `RemoteLog.h`: an 8 KB RAM ring buffer recording everything printed with
  `Log.print` / `Log.println` / `Log.printf`, each line stamped with local time.
  `Log.*` still forwards to USB serial, so the serial monitor is unchanged.
- All 33 `Serial.*` calls in the sketch now go through `Log.*`. `/api/log`
  serves the buffer and the page asks only for bytes it has not seen yet.
- Boot lines are stamped from the DS3231 directly when it already holds a valid
  time. The RTC is now initialised before the first log line, so a warm boot no
  longer starts with `[--:--:--]`.
- A single `Clock from RTC:` or `Clock set by NTP:` line marks the moment the
  clock becomes usable, so a blank RTC visibly switches from dashes to real
  times after the first NTP sync.
- Boot-time I2C scan: logs every address found on GPIO8/9, or a warning naming
  the four wires to check. The DS3231 should answer at `0x68`.
- The ESP32's own clock is now the authority for all scheduling, and the DS3231
  is persistence only. Previously a bad RTC reading overrode the NTP-corrected
  clock, so the whole sketch could run on a frozen time.
- RTC writes are verified by reading the time back, and re-read a few times so a
  single I2C glitch does not condemn the module. A module that answers but will
  not store a time is written at most once every 10 minutes, then every 6 hours,
  and warns once instead of printing a drift line every 10 seconds. If the
  DS3231's oscillator-enable bit is set, it is cleared and the write retried
  once.
- RTC readings are only trusted if they sit between one day before the firmware
  build date and twenty years after it, so a corrupt module reporting, say, 2051
  can no longer become the system clock. When a write will not stick, the log
  prints the control/status bytes and the seven raw time registers in hex, which
  is what tells a dead module from a counterfeit one.
- The `Clock from RTC:` / `Clock set by NTP:` wording now follows where the time
  actually came from, so a boot that falls back to NTP no longer claims "RTC".
- Boot messages restored to their full text now that the log pane does not wrap:
  `IR lamp : ON 13 min before sunset, OFF 20 min after`, the full `IR button`
  line, and `brightness 95%`. The two rules of dashes are full width again.
- The Watch Camera hint line is gone; its card now shows icon, title and state
  pill, matching the Serial Log tile. The hint text moved to the card's tooltip.

## V1.02

First archived build.

- Dashboard: new **Watch Camera** card, one tap. If the camera relay is off it
  presses Camera ON, counts down `CAMERA_WAKE_SECONDS` (30 s), then opens the
  stream. If the relay is already on it opens straight away.
- The card is inert while `CAMERA_VIEW_URL` is empty: no relay override, no
  countdown, no navigation.
- Temporary preview switch `SHOW_WATCH_CARD_ANYWAY` (default `true`) keeps the
  card visible before the camera hardware arrives. Set it to `false` and an
  empty `CAMERA_VIEW_URL` hides the card again.
- Camera and IR cards: **gold** border when that load is ON, **steel** when OFF.
  The header and Watch Camera card follow the sunrise/sunset day phase. The
  Automatic and IR Brightness cards are steel at all times. Green removed.
- State pills dropped from the Camera and IR cards; both cards shortened to make
  room for future buttons on a phone screen.
- ON/OFF buttons now act in place: the handlers return the fresh status as JSON
  when the request asks for it, so there is no page reload and no white flash.
  Plain navigation still gets the 303 redirect.
- Handlers run `tick()` before replying, so the returned state is the state that
  was just requested rather than the previous second's.
- Watch Camera card sits at the bottom of the page, below IR Brightness.

Not yet tested on hardware - the camera module has not arrived.

## Earlier builds

These predate the archive and are not in `CoopCam_Storage`:

| Folder | Notes |
|---|---|
| `~/Arduino/CoopCam pre pwm/original CoopCam` | First working build. `FW_VERSION` V1.00. |
| `~/Arduino/CoopCam pre pwm/CoopCam v1.01` | PWM-ramp rewrite. The file inside still declares `FW_VERSION` V1.00. |
