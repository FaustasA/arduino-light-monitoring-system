# arduino-light-monitoring-system

## Overview
The project is light monitoring system using Arduino Uno. It measures light with a photoresistor and indicates the detected light level using three LEDs. When the light level drops below the configured threshold, a sound is generated from the buzzer.

## Contains
- Three light conditions: DARK, MEDIUM and BRIGHT.
- Visual indication using red, gree, and blue LEDs.
- Adjustable light threshold using a button.
- Buzzer activation when low light level detected.
- Settings preserved when disconnected or lost power.
- Long button press to enable or disable the buzzer.
- Direct register access for LED control.

## Components
- Arduino Uno R3.
- Breadboard.
- Wires.
- Photoresistor.
- Button
- Three LEDs (red, green, blue).
- Three 220 ohm resistors and one 1K ohm resistor.
- Buzzer.

## Build steps
- Connect the physical parts and wires as shown in the schematic.
- Connect Arduino Uno to your computer using the cable.
- Using the Arduino IDE compile the code provided and use Serial 9600.

## Interrupt Handling
Two interrupts are used:
* INT0: Detects a button press and sets a flag for processing in loop.
* Timer1 interrupt: Maintains a 1 ms time counter and schedules periodic operations.

## Timer configurations
Timer1 is configured in CTC mode with prescaler of 64. Timer interrups happen every 1ms.

## EEPROM
The system saves its configuration in EEPROM so that the user settings remain available after disconnection or power shut off.

Stored values:
* Magic number (to verify if stored configuration data or parameters are valid and initialized).
* Version.
* Light threshold.
* Buzzer enabled/disabled.
At startup, the saved data is checked. If the configuration is invalid, default settings are loaded and saved.

## MMIO
The LEDs are controlled through direct access to the controller registers:
* DDRB configures which pins on B port are used as inputs or outputs.
* PORTB configures which pins on B port are receiving HIGH or LOW.


## Wiring
