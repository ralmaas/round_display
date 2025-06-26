# Round_display

Test-code for the 1.28" round display

Test-code for the 1.28" round display.
Have been playing around with LVGL, network, MQTT and NTP....
>>>>>>> ebf5d98accd3994f0d874885a0aee1904ea1dea5

Based on a GitHub by garagetinkering:
	https://github.com/garagetinkering/1.28-inch-blank



This example have two screens

This example have two screens:
>>>>>>> ebf5d98accd3994f0d874885a0aee1904ea1dea5

Screen #1:
	- ARC showing current power usage (kW)
	- Also shown on top line in numeric format
	- Second line is current tile
	- Third line is a timer display
	- Button at bottom will start/stop the timer

Any touch outside the Timer button will enable screen #2

Screen #2:
	- A button that will generate a MQTT-message to HASSIO to start/stop the DAB radio
	- Different temperatures...

This example are using these libraries:

	- lvgl/lvgl@^9.3.0
	- lovyan03/LovyanGFX@^1.1.16
	-	fbiego/ChronosESP32@^1.5.0
	- elims/PsychicMqttClient@^0.2.3

I did test the PubSubClient MQTT-library, but could not get it to work with the display component...

################
<<<<<<< HEAD
https://elkim.no/produkt/klokkedisplay-tft-display-1-28-inch-tft-lcd-display-module-round-rgb-240x240-gc9a01-driver-4-wire-spi-interface-240x240-pcb-for-arduino/
=======
https://elkim.no/produkt/klokkedisplay-tft-display-1-28-inch-tft-lcd-display-module-round-rgb-240x240-gc9a01-driver-4-wire-spi-interface-240x240-pcb-for-arduino/
>>>>>>> ebf5d98accd3994f0d874885a0aee1904ea1dea5
