#include "leds.h"
#include "watchdog.h"

boolean ledStateGreen = LED_STATE_OFF;	// Status der LED f�r LED_Green (gr�ne LED)
boolean ledStateRed = LED_STATE_OFF;	// Status der LED f�r LED_Red (rote LED)

void setupLeds()
{
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_RED, OUTPUT);

	// LEDs ausschalten
	turnGreenLedOff();
	turnRedLedOff();
}

// l�sst die gr�ne LED leuchten
void turnGreenLedOn()
{
	ledStateGreen = LED_STATE_ON;
	digitalWrite(LED_GREEN, ledStateGreen);
}

// l�sst die gr�ne LED nicht mehr leuchten
void turnGreenLedOff()
{
	ledStateGreen = LED_STATE_OFF;
	digitalWrite(LED_GREEN, ledStateGreen);
}

// wechselt den Status der gr�nen LED
void toogleGreenLed()
{
	ledStateGreen = !ledStateGreen;
	digitalWrite(LED_GREEN, ledStateGreen);
}

// l�sst die rote LED leuchten
void turnRedLedOn()
{
	ledStateRed = LED_STATE_ON;
	digitalWrite(LED_RED, ledStateRed);
}

// l�sst die rote LED nicht mehr leuchten
void turnRedLedOff()
{
	ledStateRed = LED_STATE_OFF;
	digitalWrite(LED_RED, ledStateRed);
}

// wechselt den Status der rote LED
void toogleRedLed()
{
	ledStateRed = !ledStateRed;
	digitalWrite(LED_RED, ledStateRed);
}

// l�sst die gr�ne LED kurzzeitig blinken
void blinkGreenLedShort(boolean fast)
{
	uint32_t time = fast ? TIME_FAST : TIME_SLOW;
	for (uint8_t i = 0; i < 3; i++)
	{
		turnGreenLedOn();
		delay(time);
		turnGreenLedOff();
		delay(time);
		wdt_yield();
	}
}

// l�sst die rote LED kurzzeitig blinken
void blinkRedLedShort(boolean fast)
{
	uint32_t time = fast ? TIME_FAST : TIME_SLOW;
	for (uint8_t i = 0; i < 3; i++)
	{
		turnRedLedOn();
		delay(time);
		turnRedLedOff();
		delay(time);
		wdt_yield();
	}
}

// l�sst beide LEDs kurzzeitig blinken
void blinkBothLedsShort(boolean fast)
{
	uint32_t time = fast ? TIME_FAST : TIME_SLOW;
	for (uint8_t i = 0; i < 3; i++)
	{
		turnRedLedOn();
		turnGreenLedOn();
		delay(time);
		turnRedLedOff();
		turnGreenLedOff();
		delay(time);
		wdt_yield();
	}
}