# M5Stack NMEA2000 Display for CAN BUS
This repository shows how to use the M5Stack as NMEA 2000 Display and WiFi Gateway. 
The M5Stack is receiving the data directly from the CAN bus.

The project requires the NMEA2000, NMEA2000_esp32 and NMEA0183 libraries from Timo Lappalainen: https://github.com/ttlappalainen

For the M5Stack the board software and library have to be installed: https://docs.m5stack.com/#/en/arduino/arduino_development

The M5Stack has only a limited number of GPIO pins available. The initial version used GPIO 2 (CAN TX) and GPIO 5 (CAN RX). That led to problems with the NMEA2000 bus due to signals on both lines during boot and flash time.

The new version (since version 0.2) is using GPIO 17 (TX) and GPIO 16 (RX) to avoid the problem.

The M5Stack can also work as WiFi Gateway to send NMEA 0183 messsages (TCP port 2222) via WiFi.
Just set ENABLE_WIFI to "1" to enable. Change SSID/password accordingly.

The only external hardware is the CAN bus transceiver (Waveshare SN65HVD230).
It is connected to 3,3 V, GND, CAN TX (GPIO 17) CAN RX (GPIO 16).

With the left two buttrons you can flip through the pages (5 pages currently) with differnt NMEA 2000 data. The right button is changing the backlight of the LCD.

![Display1](https://github.com/AK-Homberger/M5Stack-NMEA-2000-Display-CAN-BUS/blob/master/IMG_1173.JPG)

![Display2](https://github.com/AK-Homberger/M5Stack-NMEA-2000-Display-CAN-BUS/blob/master/IMG_1174.JPG)

## Updates:
- 02.03.2021 - Version 0.3: Added changed source address check.
- 01.03.2021 - Version 0.2: Changed CAN bus GPIOs.
- 23:11.2019 - Version 0.1: Initial version.
