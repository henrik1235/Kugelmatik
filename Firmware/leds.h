#ifndef _LEDS_h
#define _LEDS_h

#include "arduino.h"

#define LED_GREEN 2		// Port f�r gr�ne LED (Pin 4 SUBD)
#define LED_RED 3		// Port f�r rote LED (Pin 5 SUBD)

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

#endif

