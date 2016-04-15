#ifndef _UTIL_h
#define _UTIL_h

#include "arduino.h"
#include <avr/wdt.h> 

#include "leds.h"
#include "constants.h"

extern byte lastError;

void softReset(); // setzt den Chip zur�ck

// bringt den Chip in den Fehler-Modus und blockiert ihn 
void error(const char* tag, const char* message, bool blinkFast);
void internalError();
void protocolError(uint8_t error);

void usdelay(uint16_t us);

// gibt den freien SRAM zur�ck, siehe http://playground.arduino.cc/Code/AvailableMemory
int freeRam();
#endif