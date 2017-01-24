#pragma once

#include <Arduino.h>
#include <avr/wdt.h> 

#include "leds.h"
#include "constants.h"

extern byte lastError;

void softReset(); // setzt den Chip zur�ck

// bringt den Chip in den Fehler-Modus und blockiert ihn 
void error(const char* tag, const char* message, bool blinkFast);

// interner unbekannter Fehler
void internalError();

// interner Fehler mit einem bestimmten Fehlercode
void internalError(uint8_t error);

// Fehler im Protokoll (empfanges Paket) mit bestimmten Fehlercode
void protocolError(uint8_t error);

// gibt die Zahl (0 - 15) in Hexadezimal (0 - F) zur�ck.
char getHexChar(int x);

#define TIMER_COUNT 3 // Anzahl der Timer f�r startTime und endTime

// startet den Timer mit dem angegeben index
void startTime(uint8_t index);

// beendet den Timer und gibt die verstrichende Zeit zur�ck
int32_t endTime(uint8_t index);

// gibt den freien SRAM in Bytes zur�ck, siehe http://playground.arduino.cc/Code/AvailableMemory
int freeRam();