#pragma once

#include <Arduino.h>
#include <NewMCP23017.h>

#include "util.h"
#include "config.h"
#include "network.h"

#define MCP_COUNT 8				// Anzahl der MCP Chips
#define MCP_STEPPER_COUNT 4		// Anzahl der Stepper pro MCP Chip

#define IGNORE_MCP_FAULTS 0
#define BLINK_MCP_FAULTS 0

#define CLUSTER_WIDTH 5		// Anzahl Stepper in der Breite (X)
#define CLUSTER_HEIGHT 6	// Anzahl Stepper in der H�he (Y)

#define CLUSTER_SIZE (CLUSTER_WIDTH * CLUSTER_HEIGHT)

enum StepperDirection : uint8_t
{
	DirectionNone,
	DirectionUp,
	DirectionDown
};

struct StepperData
{
	int32_t LastRevision;		// letzte Revision der Daten
	int16_t CurrentSteps;		// derzeitige Schritte die der Motor gemacht hat (= H�he)
	int16_t GotoSteps;			// Schritte zu der der Motor gehen soll (= zu welche H�he die Kugel fahren soll)
	uint8_t CurrentStepIndex;	// derzeitiger Stepper Wert Index, siehe stepsStepper
	uint8_t TickCount;			// derzeitige Tick Anzahl, wenn kleiner als 0 dann wird ein Schritt gemacht und die Variable auf WaitTime gesetzt
	uint8_t WaitTime;			// Wert f�r TickCount nach jedem Schritt
	uint16_t BrakeTicks;		// Anzahl der Ticks seit letzter Bewegung
	StepperDirection Direction; // Richtung der Kugel
	uint16_t TurnWaitTime;      // Wartezeit f�r eine �nderung der Bewegungsrichtung
};

struct MCPData
{
	StepperData steppers[MCP_STEPPER_COUNT]; // Schrittmotoren pro MCP
	MCP23017 mcpChip;
	boolean isOK;
	uint16_t lastGPIOValue;
};

// Anweisung die der Schrittmotor machen soll
#define STEPPER_STEP_COUNT 8 // es gibt 8 Anweisungen f�r den Schrittmotor, siehe stepsStepper
const uint8_t stepsStepper[STEPPER_STEP_COUNT] = { 0x05, 0x04, 0x06, 0x02, 0x0A, 0x08, 0x09, 0x01 };

// Gibt die Position des MCPs an
const uint8_t mcpPosition[CLUSTER_SIZE] = { 6, 6, 5, 5, 4, 6, 6, 5, 5, 4, 7, 7, 1, 2, 2, 7, 7, 1, 2, 2, 0, 0, 1, 3, 3, 0, 0, 1, 3, 3 };

// Gibt die Position des Steppers an
const uint8_t stepperPosition[CLUSTER_SIZE] = { 2, 3, 2, 3, 1, 1, 0, 1, 0, 0, 1, 0, 3, 2, 3, 2, 3, 2, 1, 0, 2, 3, 1, 2, 3, 1, 0, 0, 1, 0 };

extern MCPData mcps[MCP_COUNT];

void initAllMCPs();			// initialisiert alle MCPs
void initMCP(uint8_t index);	// initialisiert einen MCP

StepperData* getStepper(uint8_t x, uint8_t y);
StepperData* getStepper(int32_t index);

// pr�ft ob die H�he eine besondere Bedeutung hat und nicht minStepDelta benutzt wird
boolean isSpecialHeight(int32_t height);

// �berpr�ft einen Stepper auf richtige Werte
void checkStepper(StepperData* stepper);

// setzt den Schrittmotor auf Standard Werte zur�ck (H�he = 0)
void resetStepper(StepperData* stepper);
// setzt den Schrittmotor auf eine bestimmte H�he welche nicht gepr�ft wird ob sie gr��er als MaxSteps ist
void forceStepper(StepperData* stepper, int32_t revision, int16_t height);

// setzt den Schrittmotor auf eine bestimmte H�he und Wartezeit
void setStepper(StepperData* stepper, int32_t revision, int16_t height, uint8_t waitTime);

// setzt einen Schrittmotoren auf eine bestimmte H�he
void setStepper(int32_t revision, uint8_t x, uint8_t y, int16_t height, uint8_t waitTime); 

// setzt alle Schrittmotoren auf eine bestimmte H�he
void setAllSteps(int32_t revision, int16_t height, uint8_t waitTime);

// stoppt alle Schrittmotoren (setzt GotoSteps auf die aktuelle H�he)
void stopMove();

// spricht die Schrittmotoren an und l�sst sie drehen
void updateSteppers(boolean isUsedByBusyCommand);