#pragma once

#include <Arduino.h>

#include "watchdog.h"

#define LED_GREEN 2		// Pin f�r gr�ne LED 
#define LED_RED 16		// Pin f�r rote LED

#define LED_STATE_ON false // NodeMCU LEDs leuchten bei LOW
#define LED_STATE_OFF (!LED_STATE_ON)

// Zeit in Millisekunden f�r schnelles Blinken
#define TIME_FAST 150
// Zeit in Millisekunden f�r langsames Blinken
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