// 
// 
// 

#include "leds.h"
#include <avr/wdt.h>



/// Funktionen f�r die LEDs
boolean ledStateGreen = false; // Status der LED f�r LED_Green (gr�ne LED)
boolean ledStateRed = false; // Status der LED f�r LED_Red (rote LED)

void setupLeds()
{
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_RED, OUTPUT);
}

// setzt die gr�ne LED an
void turnGreenLedOn()
{
	ledStateGreen = true;
	digitalWrite(LED_GREEN, ledStateGreen);
}

// setzt die gr�ne LED aus
void turnGreenLedOff()
{
	ledStateGreen = false;
	digitalWrite(LED_GREEN, ledStateGreen);
}

// wechselt den Status der gr�nen LED
void toogleGreenLed()
{
	ledStateGreen = !ledStateGreen;
	digitalWrite(LED_GREEN, ledStateGreen);
}

// stellt die rote LED an
void turnRedLedOn()
{
	ledStateRed = true;
	digitalWrite(LED_RED, ledStateRed);
}

// stellt die rote LED aus
void turnRedLedOff()
{
	ledStateRed = false;
	digitalWrite(LED_RED, ledStateRed);
}

// wechselt den Status der rote LED
void toogleRedLed()
{
	ledStateRed = !ledStateRed;
	digitalWrite(LED_RED, ledStateRed);
}

// l�sst die gr�ne Led kurzzeitig blinken
void blinkGreenLedShort()
{
	for (byte i = 0; i < 5; i++)
	{
		turnGreenLedOn();
		delay(200);
		turnGreenLedOff();
		delay(200);
		wdt_reset();
	}
}

// l�st die rote Led kurzzeitig blinken
void blinkRedLedShort()
{
	for (byte i = 0; i < 5; i++)
	{
		turnRedLedOn();
		delay(200);
		turnRedLedOff();
		delay(200);
		wdt_reset();
	}
}

// l�sst beide LEDs kurzzeitig blinken
void blinkBothLedsShort()
{
	for (byte i = 0; i < 5; i++)
	{
		turnRedLedOn();
		turnGreenLedOn();
		delay(200);
		turnRedLedOff();
		turnGreenLedOff();
		delay(200);
		wdt_reset();
	}
}