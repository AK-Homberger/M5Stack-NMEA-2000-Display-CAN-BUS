# M5Stack-NMEA-2000-Display-CAN-BUS
This repository shows how to use the M5Stack as NMEA 2000 Display and WiFi Gateway. 
The M5Stack is receiving the data dirctly from the CAN bus.

The M5 Stack has a limited number of GPIO pins available. But pins 2 (TX) and 5 (RX) are working.

The M5Stack can also work as WiFi Gateway to send NMEA 0183 messsages (TCP port 2222) via WiFi.
Just set ENABLE_WIFI to "1" to enable. Change SSID/password accordingly.

The only external hardware is the CAN bus transceiver (Waveshare SN65HVD230).
It is connected to 3,3 V, GND, CAN TX (GPIO2) CAN RX (GPIO 5).

With the left two buttrons you can flip through the pages (5 pages currently) with differnt NMEA 2000 data. The right button is changing the backlight of the LCD.

![Display1](https://github.com/AK-Homberger/M5Stack-NMEA-2000-Display-CAN-BUS/blob/master/IMG_1173.JPG)

![Display2](https://github.com/AK-Homberger/M5Stack-NMEA-2000-Display-CAN-BUS/blob/master/IMG_1174.JPG)
