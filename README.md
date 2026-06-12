
Chicken Coop Automation Controller


 Fully functional with additional LED's X2 above the Coop door. Two differant flashing patterns 1st at sunset followed by 2nd pattern when the door is fully closed. 
V1.4 added DHT22 coop temp and humidity with it's own pages permenant 90 day log retained even after power fail

An advanced automated chicken coop controller built around the ESP32-S3 platform.
This project automates a vertical drop-door chicken coop using sunrise/sunset calculations, WiFi time sync, RTC backup, safety monitoring, fault logging, OLED display menus, and full manual override controls.

Designed to be reliable, inexpensive, and easy to build using commonly available parts from AliExpress.

Features.
Automatic Door Control.
Opens the coop door automatically after sunrise.
Closes automatically after sunset.
Adjustable sunrise/sunset offsets.
Uses calculated sunrise/sunset times with DST support.
Automatic daily reset at midnight.

Safety Features...
Door close-open-close safety cycle if a obstuction is encountered to release trapped chickens.
Motor timeout protection to prevent burnout.
Full safety timeout system.
Fault detection and lockout mode.
Reed switch conflict detection.
Manual fault reset system.
Fault buzzer and flashing warning LED.

Lighting Automation...
Coop light turns on before sunset.
Light turns off automatically after door close.
Manual light override.
Manual light auto-timeout.
OLED Interface.
1.3" OLED I2C display.
Multiple display pages:
Main status page.
Manual control page.
Event history page.
WiFi diagnostics page.
System time page.
Event Logging.
Stores timestamped system events.
Fault diagnostics history.
Scrollable event viewer.

Manual Controls...
Rotary encoder navigation.
Manual door open/close.
Manual light control.
Wake/sleep display.
Fault reset controls.

Connectivity...
WiFi NTP time synchronization.
Automatic WiFi reconnect.
RTC backup using DS3231.
WiFi signal quality display.

Hardware Used...
Main Controller.
ESP32-S3 N16R8 (external antenna recommended).

Display...
1.3" SH1106 OLED I2C display.

RTC...
DS3231 RTC module.

Input..
KY-040 rotary encoder.
Door Motor..

JGB37-520 geared DC motor.
22RPM gearbox.

Vertical drop-door mechanism.
Pulley and cord system.

Motor Driver.
L298N motor controller.
(TB6612FNG also works).

Sensors...
2x Normally Open reed limit switches.
Magnet-actuated.

Lighting...
3.3V relay module.

Alerts...
Active buzzer.
Green status LED.
Red fault LED.

Power...
12V 100W / 8A PSU.
DC-DC buck converter (12V → 5V).

GPIO Pin Mapping....

Function	GPIO.
Motor Open	41.
Motor Close	42.
Open Limit Switch	15.
Close Limit Switch	16.
Coop Light Relay	17.
Buzzer	3.
Green Status LED	6.
Red Fault LED	7.
Red Door indicator LED 13. V1.2
Red Door indicator LED 14. V1.2
Encoder CLK	4.
Encoder DT	5.
Encoder SW	18.
I2C SDA	8.
I2C SCL	9.

Create a file named:

secrets.h

Add the following:

#pragma once

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Your location coordinates
double latitude = 51.0000;
double longitude = -3.0000;

Replace the coordinates with your own location.

You can get your latitude and longitude from Google Maps.

How It Works...
Sunrise/Sunset Automation

The system calculates sunrise and sunset times daily using:

Latitude
Longitude
Daylight Saving Time

WiFi is required for accurate NTP synchronization.

The DS3231 RTC keeps time during WiFi outages.

User Adjustable Settings...

Near the top of the sketch:

// Door opens AFTER sunrise
int sunriseOpenOffset = 10;

// Door closes AFTER sunset
int sunsetCloseOffset = 30;

// Coop light comes ON before sunset.
int lightOnOffset = 1;

// Coop light goes OFF after sunset.
int lightOffMinutes = 35;

You can easily adjust:

Door open timing
Door close timing
Light timing
OLED sleep timing
Buzzer timing
Motor timeout values
Fault System
Version numbers
Door indicator timing

The controller includes several safety checks:

Fault	Description
OPEN timeout	Door failed to open.
CLOSE timeout	Door failed to close.
LIMIT CONFLICT	Both switches active.
HOME FAIL	Startup homing failed.
Safety timeout	Safety cycle exceeded limit.

After repeated failures:

System enters LOCKOUT mode.
Buzzer sounds.
Red LED flashes rapidly.

Manual reset:

Hold encoder button for 5 seconds.
Rotary Encoder Controls.

Short Press...
Wake display.
Change pages.

Hold ~3 Seconds...
Manual light ON/OFF.

Hold 5 Seconds...
Reset faults and lockouts.

Rotate Encoder.
MANUAL PAGE ONLY.
Counter-clockwise → Open door.
Clockwise → Close door.
Startup Recovery.

On boot:

System checks door position.
If unknown, it attempts homing.
Door closes until close limit is reached.
Prevents accidental position errors after power loss.
Wiring Notes.
Reed Switches.

Use:

Normally Open switches.
Connected to GND.
Internal pullups enabled in software.
Motor Driver.

Ensure:

Proper flyback protection.
Shared common ground.
Adequate PSU current capacity.
Relay Board.

Use:

Logic-level 3.3V compatible relay module.
Recommended Improvements

Possible future upgrades:

Battery backup
Solar charging
Web dashboard
OTA firmware updates
Temperature monitoring
Egg counter
Predator sensors
Telegram or MQTT notifications
Installation
1. Install Arduino IDE

Download:

Arduino IDE 2.x
2. Install ESP32S3 Dev Module Board Package

Add ESP32s3 board support:

ESP32 by Espressif Systems
3. Install Libraries

Install all required libraries.

4. Create secrets.h

Add WiFi credentials and coordinates.

5. Select Board ESP32S3 Dev Module

Choose:

ESP32S3 Dev Module
6. Upload Firmware

Compile and upload the sketch.

OLED Pages
Main Page
Door status
Light status
WiFi signal indicator
Manual Control
Manual open/close
Manual light control
Events
Scrollable event history
System WiFi
IP address
Signal quality
System Time
Current time
Sunrise/sunset times
Power Consumption

Typical system:

ESP32-S3
OLED display
RTC
Relay
Motor controller idle

Runs comfortably from:

12V 8A supply

Motor startup current depends on:

Door weight
Pulley friction
Motor gearing
Notes
Slower door motors generally work better, 22 rpm and 32mm pulley wheel
Ensure the door slides freely
Avoid excessive motor load
Test timeout values carefully
Magnets must align properly with reed switches
Use shielded wiring for long switch runs if needed
Example Use Case

Typical evening sequence:

Coop light turns on before sunset
Door closes after sunset
Safety reopen cycle runs if obstuction is encountered
Door recloses securely
Light turns off later
System resets overnight

Morning:

Door opens after sunrise
Chickens released automatically
Repository Structure
/ChickenCoopController
│
├── ChickenCoopController.ino
├── secrets.h
├── README.md
└── images/
License

MIT License

Feel free to modify, improve, and share.
