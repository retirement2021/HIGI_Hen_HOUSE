HIGI Hen House

An automated chicken coop controller for the ESP32-S3. It opens and closes the coop door and switches the coop light in step with local sunrise and sunset, logs inside and outside temperature and humidity, and shuts itself down safely if the door or motor misbehaves.

Built for UK time zones (automatic GMT/BST changeover). Wi-Fi is only needed for NTP time sync, and a battery-backed RTC keeps the schedule running without it.

Author: HIGI-Styx · Built February to October 2026 · Current firmware: V2.18 

Features
Every customisable value sits in one block split into two at the top of the sketch, under USER SETTINGS and USER MAGIC NUMBERS. The defaults in the sketch are the author's own coop values, so review each one before use.

Sunrise/sunset door and light schedule, calculated on the ESP32 from your latitude and longitude. Separate offsets for GMT (winter) and BST (summer).

Rooster crow control: optional fixed morning opening time, set separately for winter and summer.

1.Cold-weather door delay: if the outside temperature is below a set threshold, the morning opening is delayed. 
2.The temperature is checked just before the door opens, and a "Cold Delay" event is logged.
3.Cold LED: a blue LED above the door lights 24/7 while the temp is below the set threshold. see 1.


1.1 Motor timeout protection stops the motor if the door fails to reach a limit switch in time.
2.1 Obstruction protection: if a hen blocks the closing door, the door reopens, waits, then retries. After the maximum number of retries it raises a fault.
1.1 & 2.1 are the only two adjustments that need to be set to your door times. Found in **USER MAGIC NUMBERS**


Fault lockout with a buzzer and red LED. Cleared with a long button press.

Chicken-homing LEDs: red LEDs above the door flash faster in stages as closing time approaches, then alternate slowly all night once the door is fully closed.

Two temperature/humidity sensors: SHT31 inside, SHT30 outside, with persistent daily logs.

Event log of up to 200 date-stamped events.

Startup homing: if the door is not at either limit switch on power-up, it closes until the lower limit is reached, then positions the door depending on the time

Hardware watchdog (35 s) 

Automatic DST handling via a local-time timezone rule.

Manual control of the door and light from the rotary encoder.


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


Libraries

Install these through the Arduino Library Manager:

U8g2
RTClib
Sensirion I2C SHT3x (SensirionI2cSht3x)
AiEsp32RotaryEncoder

WiFi, Wire, Preferences, time and esp_task_wdt come with the ESP32 Arduino core.

Setup
Install the ESP32 board package in the Arduino IDE and select ESP32S3 Dev Module.
Install the libraries above.
Create a secrets.h file next to the sketch with your Wi-Fi details and your coordinates. 
Adjust the settings 
Upload the sketch.

Known limits
The bedtime flash times (STAGE0..3_FLASH_MS, NIGHT_FLASH_MS) must be multiples of 100 ms, because of a known software issue.

There are 7 pages: Main, Today's Schedule, Manual, Environment, Events, Cold & Daytime, Wi-Fi.

Action	Result
Short press	Next page
Rotate (Events / Environment pages)	Scroll
Rotate (Manual page)	Move the door open or closed (needs a few detents to avoid accidental moves)
Hold 0.7-2 s (Manual page)	Toggle the coop light. Turning it on starts a 10-minute auto-off timer.
Hold 3 s	Shows a "Hold for RESET" popup
Hold 5 s	Resets faults and clears the lockout while the button is still held
Release between 3 and 5 s	"Reset Cancelled"

The display sleeps after one minute and wakes on any input.

Faults

The red LED and buzzer signal a fault. Faults are written to the Events page. 

The system locks out after repeated limit-switch failures.
A fault can be cleared with the 5 s hold above. 

Fault messages include: Door blocked, OPEN timeout, LIMIT CONFLICT, MOTOR CONFLICT, HOME FAIL, OPEN LIMIT FAIL, CLOSE LIMIT FAIL and Safety timeout - obstruction unresolved.

Sensor problems (SHT31 IN FAULT, SHT30 OUT FAULT) are logged after 10 consecutive failed reads, and RTC problems are logged as RTC Invalid or RTC Battery Low.

Evolving & running in a working coop since April 2026.
