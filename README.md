HIGI Hen House
An automated chicken coop controller for the ESP32-S3. It opens and closes the coop door and switches the coop light in step with local sunrise and sunset, logs inside and outside temperature and humidity, and shuts itself down safely if the door or motor misbehaves.
Built for UK time zones (automatic GMT/BST changeover). Wi-Fi is only needed for NTP time sync, and a battery-backed RTC keeps the schedule running without it.
Author: HIGI-Styx · Built February to October 2026 · Current firmware: V2.18 
Features
    • Sunrise/sunset door and light schedule, calculated on the ESP32 from your latitude and longitude. Separate offsets for GMT (winter) and BST (summer). 
    • Rooster crow control: optional fixed morning opening time, set separately for winter and summer. 
    • Cold-weather door delay: if the outside temperature is below a set threshold, the morning opening can be delayed. The temperature is checked just before the door opens, and a "Cold Delay" event is logged. 
    • Cold LED: a blue LED above the door lights 24/7 while it is cold outside. 
    • Obstruction protection: if a hen blocks the closing door, the door reopens, waits, then retries. After the maximum number of retries it raises a fault. 
    • Motor timeout protection stops the motor if the door fails to reach a limit switch in time. 
    • Fault lockout with a buzzer and red LED. Cleared with a long button press. 
    • Chicken-homing LEDs: red LEDs above the door flash faster in stages as closing time approaches, then alternate slowly all night once the door is fully closed. 
    • Dual temperature/humidity sensors: SHT31 inside, SHT30 outside, with persistent daily logs. 
    • Event log of up to 200 date-stamped events. 
    • Startup homing: if the door is not at either limit switch on power-up, it closes until the lower limit is reached then positions it’s self correctly
    • Hardware watchdog (35 s) 
    • Automatic DST handling via a local-time timezone rule. 
    • Manual control of the door and light from the rotary encoder. 
Hardware
Part	Notes
ESP32-S3 N16R8 board	Arduino IDE board: ESP32S3 Dev Module
1.3" OLED, SH1106, 128x64, I2C	Driven with U8g2
DS3231 RTC	I2C, with backup battery
SHT31 sensor (inside)	I2C address 0x45 (link VIN to the ADDR pin)
SHT30 probe (outside)	I2C address 0x44 (fixed)
KY-040 rotary encoder	Page navigation and manual control
Motor driver	L298N or TB6612FNG, for the door motor
2x reed switches	Door open and closed limit switches, normally open, wired to GND
Logic-level relay module	Coop light (3.3 V or 5 V coil)
3 V active buzzer	Fault alert
LEDs	1 green (status), 1 red (fault), 2 red (night/homing), 1 blue (cold)
Momentary push button	Recommended: wired between ESP32 RST and GND
Pin map
Function	GPIO
Motor OPEN	41
Motor CLOSE	42
Limit switch, door open (top)	15
Limit switch, door closed (bottom)	16
Coop light relay	17
Buzzer	3
Status LED (green)	6
Fault LED (red)	7
Night LED 1 (red)	11
Night LED 2 (red)	12
Cold LED (blue)	13
Encoder CLK / DT / SW	4 / 5 / 18
I2C SDA / SCL	8 / 9
The OLED, RTC and both SHT sensors share the same I2C pins. Power them from 3.3 V.
LED resistor values and wiring notes are in the comments next to each pin definition in the sketch. Wiring diagrams, photos, 3D printing files and a complete parts list are in the project repository: https://github.com/retirement2021/HIGI_Hen_HOUSE
Warning: the coop light relay may switch mains voltage. Use appropriately rated parts, enclose all mains wiring, and have it checked by someone qualified if you are unsure.
Libraries
Install these through the Arduino Library Manager:
    • U8g2 
    • RTClib 
    • Sensirion I2C SHT3x (SensirionI2cSht3x) 
    • AiEsp32RotaryEncoder 
WiFi, Wire, Preferences, time and esp_task_wdt come with the ESP32 Arduino core.
Setup
    1. Install the ESP32 board package in the Arduino IDE and select ESP32S3 Dev Module. 
    2. Install the libraries above. 
    3. Create a secrets.h file next to the sketch with your Wi-Fi details and your coordinates. The sketch uses the names WIFI_SSID and WIFI_PASSWORD for the credentials and latitude / longitude for the sun calculation, so match the declarations in your own secrets.h. Do not commit this file. Add secrets.h to your .gitignore. 
    4. Adjust the settings (see below). 
    5. Upload the sketch. 
Settings
Every customisable value sits in one block at the top of the sketch, under USER SETTINGS and USER MAGIC NUMBERS. The defaults in the sketch are the author's own coop values, so review each one before use.
Setting	Purpose	Default in sketch
useFixedOpenTimeGMT / useFixedOpenTime	true = fixed opening time, false = sunrise offset (winter / summer)	true
fixedDoorOpenHourGMT : Minute	Fixed winter opening time	08:00
fixedDoorOpenHour : Minute	Fixed summer opening time	07:55
sunriseOpenOffset* / sunsetCloseOffset*	Minutes after (+) or before (-) sunrise/sunset	see sketch
lightOnOffset* / lightOffMinutes*	Coop light on/off relative to sunset	see sketch
enableOutsideSHT	Set false if there is no outdoor sensor	true
outsideTempTriggerC	Cold threshold in °C	-2
doorOpenDelayIfColdMinutes	Minutes to delay opening when cold	35
forceOpenDespiteCold	Ignore the cold delay	false
MOTOR_TIMEOUT	Maximum door travel time. Set it about 1-2 s above the "DOOR OPEN" time in the Events page	17 s
MOTOR_TIMEOUT_RECOVERY	Longer timeout used after an obstruction	20 s
SAFETY_WAIT_AFTER_REOPEN_MS	Pause with the door open before an obstruction retry	180 s
MAX_OBSTRUCTION_RETRIES	Closing attempts before a fault	3
manualLightTimeout	Auto-off for a manually switched-on light	10 min
oledTimeout	Display sleep	1 min
buzzerRepeatInterval	Fault buzzer repeat	15 s
SHT_READ_INTERVAL_MS / OUTSIDE_SHT_READ_INTERVAL_MS	Inside / outside sensor read interval	13 min / 9 min
ENVIRONMENT_LOG_DAYS / _OUTSIDE	Days of temperature history kept	10 / 31
Display page order can also be changed in the DisplayPage enum.
Known limits
    • The bedtime flash times (STAGE0..3_FLASH_MS, NIGHT_FLASH_MS) must be multiples of 100 ms, because of a known software issue. 
    • Logging too many environment days makes the display pages erratic. Safe values are 7-14 days inside and up to 31 days outside. 
Using the display and encoder
There are 7 pages: Main, Today's Schedule, Manual, Environment, Events, Cold & Daytime, Wi-Fi.
Action	Result
Short press	Next page
Rotate (Events / Environment pages)	Scroll
Rotate (Manual page)	Move the door open or closed (needs a few detents to avoid accidental moves)
Hold 0.7-2 s (Manual page)	Toggle the coop light. Turning it on starts a 10-minute auto-off timer, turning it off locks it off
Hold 3 s	Shows a "Hold for RESET" popup
Hold 5 s	Resets faults and clears the lockout while the button is still held
Release between 3 and 5 s	"Reset Cancelled"
The display sleeps after one minute and wakes on any input.
Faults
The red LED and buzzer signal a fault. Faults are written to the Events page. The system locks out after repeated limit-switch failures. A fault can be cleared with the 5 s hold above. Fault messages include: Door blocked, OPEN timeout, LIMIT CONFLICT, MOTOR CONFLICT, HOME FAIL, OPEN LIMIT FAIL, CLOSE LIMIT FAIL and Safety timeout - obstruction unresolved.
Sensor problems (SHT31 IN FAULT, SHT30 OUT FAULT) are logged after 10 consecutive failed reads, and RTC problems are logged as RTC Invalid or RTC Battery Low.
Version history
V2.18
    • Ignore button presses shorter than 30 ms, which fixes pages being skipped by contact bounce or noise on the encoder switch. 
    • The 5 s reset now fires while the button is still held, instead of waiting for release. 
    • Notices such as "Hold for RESET", "LOCKED OUT", "Already OPEN" and "Reset Cancelled" now appear as a 2 s popup. Previously they were stored but never drawn. 
V2.17 DST validation. The sketch runs on local time with a DST-aware timezone rule. Added a GMT/BST flag to the Today's Schedule title. The real-world test is the clock change in March 2027.
V2.16 Fixed timestamp position shift caused by single-digit temperatures on the environment pages.
V2.15 One-month soak test in the working coop. Reduced NVS save writes for long-term stability.
V2.14 Standardised NVS logging to resolve saving errors.
V2.13 System Time page replaced by "Cold & Daytime".
V2.12 Inside and outside environment pages merged.
V2.10 Removed the clear-environment-logs-on-upload option, which was the likely cause of NVS logging failures.
V2.04 Outside environment pages rewritten to match the inside pages. The outside sensor is polled immediately before the door opens.
V2.02 Cold delay logic evaluates temperature just before opening and logs "Cold Delay".
V2.01 Added the SHT30 outside sensor (0x44). Switched to the Sensirion library for dual sensors. SHT31 moved to 0x45. Added the cold-weather door delay and cold LED.
V2.0 Replaced the DHT22 with an SHT31 temperature sensor
Status
Evolving and running in a working coop since April 2026.
