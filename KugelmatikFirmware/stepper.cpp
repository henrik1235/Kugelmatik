#include "stepper.h"

MCPData mcps[MCP_COUNT];

void initAllMCPs()
{
	Serial.println(F("initAllMCPs()"));

	for (uint8_t i = 0; i < MCP_COUNT; i++)
		initMCP(i);
}

void initMCP(uint8_t index)
{
	Serial.print(F("Init mcp number "));
	Serial.println(index);


	MCPData* data = &mcps[index];

	for (byte i = 0; i < MCP_STEPPER_COUNT; i++)
	{
		StepperData* stepper = &data->Steppers[i];
		memset(stepper, 0, sizeof(StepperData));
	}

	data->MCP = new MCP23017();

	// MCP initialisieren
	data->MCP->begin(index);

	// alle Pins (0xFFFF) auf OUTPUT stellen
	data->MCP->setPinDirOUT(0xFFFFu);

	data->isOK = (data->MCP->writeGPIOS(0) == 0);
	data->lastGPIOValue = 0;

#if !IGNORE_MCP_FAULTS
	if (!data->isOK)
	{
		Serial.print(F("MCP Fault with mcp number "));
		Serial.println(index);

		internalError(ERROR_MCP_FAULT_1 + index);

		turnRedLedOn();
		for (byte i = 0; i <= index; i++)
		{
			turnGreenLedOn();
			delay(TIME_SLOW);
			turnGreenLedOff();
			delay(TIME_SLOW);
		}
		turnRedLedOff();
	}
#else
	Serial.println(F("IGNORE_MCP_FAULTS is set to true, skipping MCP boot test"));
#endif
}

StepperData* getStepper(byte x, byte y)
{
	return getStepper(y * CLUSTER_WIDTH + x);
}

StepperData* getStepper(int index)
{
	MCPData* data = mcps + mcpPosition[index];
	return data->Steppers + stepperPosition[index];
}

boolean isSpecialHeight(uint16_t height)
{
	return height == 0 || height == config.maxSteps || height % 100 == 0;
}

void checkStepper(StepperData* stepper)
{
	if (stepper->CurrentSteps < 0 || stepper->CurrentSteps > config.maxSteps) {
		internalError(ERROR_INTERNAL_LOOP_VALUES_WRONG);
		stepper->CurrentSteps = 0;
	}
	if (stepper->GotoSteps < 0 || stepper->GotoSteps > config.maxSteps) {
		internalError(ERROR_INTERNAL_LOOP_VALUES_WRONG);
		stepper->GotoSteps = 0;
	}
	if (stepper->CurrentStepIndex >= STEPPER_STEP_COUNT) {
		internalError(ERROR_INTERNAL_LOOP_VALUES_WRONG);
		stepper->CurrentStepIndex = 0;
	}
}

void resetStepper(StepperData* stepper)
{
	stepper->CurrentSteps = 0;
	stepper->GotoSteps = 0;
	stepper->TickCount = 0;
	stepper->WaitTime = 0;
	stepper->BrakeTicks = 0;
}

void forceStepper(StepperData* stepper, int32_t revision, int16_t height)
{
	resetStepper(stepper);
	stepper->LastRevision = revision;
	stepper->GotoSteps = height;
}

void setStepper(StepperData* stepper, int32_t revision, uint16_t height, byte waitTime)
{
	if (checkRevision(stepper->LastRevision, revision))
	{
		stepper->LastRevision = revision;

		int16_t diff = abs(stepper->CurrentSteps - (int16_t)height);
		if (isSpecialHeight(height) || diff >= config.minStepDelta)
		{
			stepper->GotoSteps = height;
			stepper->TickCount = 0;
			stepper->WaitTime = waitTime;
		}
	}
}


void setStepper(int32_t revision, byte x, byte y, uint16_t height, byte waitTime)
{
	if (x < 0 || x >= CLUSTER_WIDTH)
		return protocolError(ERROR_X_INVALID);
	if (y < 0 || y >= CLUSTER_HEIGHT)
		return protocolError(ERROR_Y_INVALID);
	if (height > config.maxSteps)
		return protocolError(ERROR_INVALID_HEIGHT);

	setStepper(getStepper(x, y), revision, height, waitTime);
}

void setAllSteps(int32_t revision, uint16_t height, byte waitTime)
{
	if (height > config.maxSteps)
		return protocolError(ERROR_INVALID_HEIGHT);
	for (int i = 0; i < CLUSTER_SIZE; i++) 
		setStepper(getStepper(i), revision, height, waitTime);
}


void stopMove() {
	Serial.println(F("stopMove()"));

	for (int i = 0; i < CLUSTER_SIZE; i++) {
		StepperData* stepper = getStepper(i);

		// stoppen
		stepper->GotoSteps = stepper->CurrentSteps;
		stepper->TickCount = 0;
	}
}


void updateSteppers(boolean alwaysUseHalfStep)
{
	startTime(TIMER_STEPPER);
	for (byte i = 0; i < MCP_COUNT; i++)
	{
		uint16_t gpioValue = 0;

		MCPData* mcp = &mcps[i];

		for (byte j = 0; j < MCP_STEPPER_COUNT; j++)
		{
			StepperData* stepper = &mcp->Steppers[j];

			checkStepper(stepper);

			byte stepSize = 0;
			int32_t diff = abs(stepper->CurrentSteps - stepper->GotoSteps);

			if (diff != 0)
			{
				if (config.stepMode == StepBoth) 
					stepSize = min(2, diff);		// schauen ob Full oder Half Step gemacht werden soll
				else if (diff >= config.stepMode)	
					stepSize = config.stepMode;	// Half oder Full Step machen
			}

			if (alwaysUseHalfStep)
				stepSize = min(stepSize, 1);

			bool waitStep = false;
			if (stepper->WaitTime > 0) {
				stepper->TickCount--;
				waitStep = stepper->TickCount >= 0;
			}

			if (stepSize > 0 && !waitStep)
			{
				int8_t stepperIndex = stepper->CurrentStepIndex;
				if (stepperIndex % stepSize > 0) // schauen ob wir noch einen Zwischenschritt machen m�ssen
					stepSize = 1;

				if (stepper->GotoSteps < stepper->CurrentSteps)  // nach oben fahren
				{
					stepper->CurrentSteps -= stepSize;
					stepperIndex -= stepSize;
				}
				else											 // nach unten fahren
				{
					stepper->CurrentSteps += stepSize;
					stepperIndex += stepSize;
				}

				// stepperIndex in den richtigen Bereich bringen (underflow/overflow)
				while (stepperIndex < 0)
					stepperIndex += STEPPER_STEP_COUNT;
				while (stepperIndex >= STEPPER_STEP_COUNT)
					stepperIndex -= STEPPER_STEP_COUNT;

				gpioValue |= stepsStepper[stepperIndex] << (4 * j); // jeder Wert in stepsStepper ist 4 Bit lang

				stepper->CurrentStepIndex = (byte)stepperIndex;
				stepper->TickCount = stepper->WaitTime;
				stepper->BrakeTicks = 0;
			}
			else if (config.brakeMode == BrakeSmart)
			{
				if (stepper->BrakeTicks < config.brakeTicks) {
					gpioValue |= stepsStepper[stepper->CurrentStepIndex] << (4 * j);
					stepper->BrakeTicks++;
				}
			}
			else if (config.brakeMode == BrakeAlways) // Bremse benutzen?
				gpioValue |= stepsStepper[stepper->CurrentStepIndex] << (4 * j);
		}


		if (!mcp->isOK || mcp->lastGPIOValue != gpioValue) {
			boolean wasOK = mcp->isOK;
			mcp->isOK = (mcp->MCP->writeGPIOS(gpioValue) == 0);

			if (mcp->isOK)
				mcp->lastGPIOValue = gpioValue;

#if !IGNORE_MCP_FAULTS
			if (wasOK && !mcp->isOK) {
				Serial.print(F("MCP Fault with mcp number "));
				Serial.println(i);

				internalError(ERROR_MCP_FAULT_1 + i);
			}
#endif
		}

		wdt_yield();
	}
	stepperTime = endTime(TIMER_STEPPER);
}