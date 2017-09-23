#pragma once

#include <Arduino.h>

#include "watchdog.h"

#define LED_GREEN 18	// Pin f�r gr�ne LED 
#define LED_RED 19		// Pin f�r rote LED

#define TIME_FAST 150
#define TIME_SLOW 500

// initialisiert die Pins f�r die LEDs
void setupLeds(); 

void turnGreenLedOn();
void turnGreenLedOff();

void turnRedLedOn();
void turnRedLedOff();

void toogleRedLed();
void toogleGreenLed();

void blinkRedLedShort(boolean fast);
void blinkGreenLedShort(boolean fast);
void blinkBothLedsShort(boolean fast);