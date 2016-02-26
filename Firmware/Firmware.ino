// Kugelmatik V3
// Firmware
//  Henrik Kunzelmann 2015
//  Rainer Wieland

// Hardware
//  AVR-NET-I/O mit ATmega32 und ENC28J60
//  MCP23S17
//  L293DNE

// Defines
#define BUILD_VERSION 14
#define ENABLE_LOG false // aktiviert den Log der im EEPROM Speicher erzeugt wird


#define BLINK_LED_ASYNC false // gibt an ob die LED async blinken soll, also unabh�ngig davon was die Firmware gerade macht
#define WRITE_INTERSTRUCTION_POINTER false // gibt an ob der aktuelle Instruction Pointer gespeichert werden soll (experimentell)
#define ENABLE_TIMER1 (BLINK_LED_ASYNC  || WRITE_INTERSTRUCTION_POINTER)

#define ENABLE_WATCH_DOG true		// gibt an ob der WatchDog aktiviert sein soll, der Chip wird resetet wenn er sich aufh�ngt
#define ENABLE_WATCH_DOG_SAVE false // WIP, WIRD NICHT UNTERST�TZT; gibt an ob nach einem Watch Dog Reset die Stepper Daten aus dem EEPROM gelesen werden sollen

#define ENABLE_INTERRUPTS true // gibt an ob die Interrupts immer an sein sollen


#define _DISABLE_INTERRUPTS (!ENABLE_INTERRUPTS && (!ENABLE_TIMER1 || (ENABLE_WATCH_DOG && ENABLE_WATCH_DOG_SAVE)))

#define LAN_ID 0x11 // ID des Boards im LAN, wird benutzt um die Mac-Adresse zu generieren
const static byte ethernetMac[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, LAN_ID }; // Mac-Adresse des Boards


#define TICK_TIME 2000
#define HOME_TIME 3500
#define FIX_TIME 3500

#define ALLOW_STOP_BUSY true		// gibt an ob der Client "busy"-Befehle beenden darf (z.B. Home)
#define ALLOW_STOP_MOVE true		// gibt an ob der Client die Bewegung stoppen darf
#define RECEIVE_PACKETS_BUSY true	// gibt an ob der Client bei "busy"-Befehle Pakete empf�ngt

#define MCP_COUNT 8			// Anzahl der MCP Chips
#define STEPPER_COUNT 4		// Anzahl der Stepper pro MCP Chip

#define CLUSTER_WIDTH 5		// Anzahl Stepper in der Breite (X)
#define CLUSTER_HEIGHT 6	// Anzahl Stepper in der H�he (Y)

#define STEP_MODE 1			// 1 = Half Step, 2 = Full Step, 3 = Both
#define MAX_STEPS 8000		// Maximale Anzahl an Steps die die Firmware maximal machen darf (nach unten)
#define FIX_STEPS 20000		// Steps die ein Stepper macht um einen Stepper zu fixen
#define USE_BREAK false		// Wenn true, dann bremsen die Schrittmotoren

#define BLINK_PACKET false // Wenn true, dann blinkt die gr�ne Led wenn ein Kugelmatik Paket verarbeitet wird

#define PROTOCOL_PORT 14804 // Port f�r das Protokoll �ber UDP

#define ETHERNET_BUFFER_SIZE 550 // Gr��e des Ethernet Buffers in Bytes

// Includes
#include <avr/pgmspace.h>
#include <avr/wdt.h> 
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <limits.h>
#include <EtherCard.h>
#include <EEPROM.h>
#include <I2C.h>
#include <MCP23017.h>
#include "cluster_led.h"
#include "constants.h"

struct StepperData
{
	int32_t LastRevision;	// letzte Revision der Daten
	int32_t CurrentSteps;	// derzeitige Schritte die der Motor gemacht hat
	int32_t GotoSteps;		// Schritte zu der der Motor gehen soll
	byte CurrentStepIndex;	// derzeitiger Stepper Wert Index, siehe stepsStepper
	int32_t TickCount;		// derzeitige Tick Anzahl, wenn kleiner als 0 dann wird ein Schritt gemacht und die Variable auf WaitTime gesetzt
	byte WaitTime;			// Wert f�r TickCount nach jedem Schritt
};

struct MCPData
{
	StepperData Steppers[STEPPER_COUNT]; // Schrittmotoren pro MCP
	byte MCPAddress;
	MCP23017 MCP;
}
mcps[MCP_COUNT];

// Anweisung die der Schrittmotor machen soll
#define STEPPER_STEP_COUNT 8 // es gibt 8 Anweisungen f�r den Schrittmotor, siehe stepsStepper
const byte stepsStepper[STEPPER_STEP_COUNT] = { 0x05, 0x04, 0x06, 0x02, 0x0A, 0x08, 0x09, 0x01 };

// geben die Position des MCPs und des Steppers f�r jede Kugel an
// erster Index ist die linke untere Kugel, siehe "Cluster_Kugelpositionen.pdf" in "Handb�cher"
const byte mcpPosition[CLUSTER_WIDTH * CLUSTER_HEIGHT] = { 6, 6, 5, 5, 4, 6, 6, 5, 5, 4, 7, 7, 1, 2, 2, 7, 7, 1, 2, 2, 0, 0, 1, 3, 3, 0, 0, 1, 3, 3 };
const byte stepperPosition[CLUSTER_WIDTH * CLUSTER_HEIGHT] = { 2, 3, 2, 3, 1, 1, 0, 1, 0, 0, 1, 0, 3, 2, 3, 2, 3, 2, 1, 0, 2, 3, 1, 2, 3, 1, 0, 0, 1, 0 };

// Buffer f�r die Ethernet Pakete
byte Ethernet::buffer[ETHERNET_BUFFER_SIZE];

#define HEADER_SIZE 9 // Gr��e des Paket-Headers in Bytes


int32_t configRevision = 0; // die letzte Revision des Config-Packets
int32_t setDataRevision = 0; // die letzte Revision des SetData-Packets

byte stepMode = STEP_MODE;
int32_t tickTime = TICK_TIME;
boolean useBreak = USE_BREAK;

byte currentBusyCommand = BUSY_NONE;
boolean stopBusyCommand = false;
byte lastError = ERROR_NONE;

// setzt den Chip zur�ck
void softReset()
{
	wdt_enable(WDTO_2S);	// Watch Dog aktivieren damit der Chip zur�ck gesetzt wird
	while (true);			// in Endlosschleife gehen damit der Watch Dog den Chip resetet
}

// folgende Funktionen schreiben Bytes oder Strings in den EEPROM-Speicher um einfache Troubleshooting-Infos zu bringen
#define LOG_BEGIN 32 // ab wann der Log im EEPROM-Speicher beginnen soll (in Bytes)
#define LOG_SIZE 512 // Gr��e des Logs in Bytes, wird diese Grenze �berschritten dann gehen alle weiteren Zeichen wieder an den Anfang (LOG_BEGIN)

int32_t logPosition = 0;

#if ENABLE_LOG
char _intToStringBuffer[12];
#endif

void updateEEPROM(int32_t position, byte val) {
	if (EEPROM.read(position) != val)
		EEPROM.write(position, val);
}

// schreibt einen Byte in den Log
void printLog(byte val)
{
#if ENABLE_LOG
	EEPROM.update(LOG_BEGIN + logPosition, val);
	logPosition++;
	if (logPosition >= LOG_SIZE)
		logPosition = 0;
#endif
}

// schreibt einen nullterminierten String in den Log ('\0')
void printLogString(char* str)
{
	for (int32_t i = 0; str[i] != '\0'; i++)
		printLog(str[i]);
	printLog(' ');
}

// schreibt aus einem char-Array len Bytes in den Log
void printLogCharArray(const char* array, int32_t len)
{
	for (int32_t i = 0; i < len; i++)
		printLog(array[i]);
	printLog(' ');
}

// schreibt einen Integer-Wert in den Log
void printLogInt(int value) {
#if ENABLE_LOG
	int count = sprintf(_intToStringBuffer, "%d", value);
	if (count < 0)
		return blinkRedLedShort();

	printLogCharArray(_intToStringBuffer, count);
#endif
}

// bringt den Chip in den Fehler-Modus und blockiert ihn 
void error(byte code, char* message)
{
	printLogString(message);
	while (true) {
		toogleRedLed();
		delay(500);
		wdt_reset();
	}
}

// initialisiert einen MCP
void initMCP(byte index)
{
	MCPData* data = &mcps[index];
	data->MCPAddress = index;

	for (byte i = 0; i < STEPPER_COUNT; i++)
	{
		StepperData* stepper = &data->Steppers[i];
		stepper->LastRevision = 0;
		stepper->CurrentSteps = 0;
		stepper->GotoSteps = 0;
		stepper->CurrentStepIndex = 0;
		stepper->TickCount = 0;
		stepper->WaitTime = 0;
	}

	// MCP initialisieren
	data->MCP.begin(index);

	// alle Pins (0xFFFF) auf OUTPUT stellen
	data->MCP.setPinDirOUT(0xFFFF);
}

// initialisiert alle MCPs
void initAllMCPs()
{
	for (byte i = 0; i < MCP_COUNT; i++)
		initMCP(i);
}

// gibt true zur�ck wenn revision neuer ist als lastRevision
boolean checkRevision(int32_t lastRevision, int32_t revision)
{
	if (lastRevision >= 0 && revision < 0) // Overflow handeln
		return true;
	return revision > lastRevision;
}

// setzt einen Stepper auf eine bestimmte H�he
void setStepper(int32_t revision, byte x, byte y, uint16_t height, byte waitTime)
{
	if (x < 0 || x >= CLUSTER_WIDTH)
	{
		lastError = ERROR_X_INVALID;
		return blinkRedLedShort();
	}
	if (y < 0 || y >= CLUSTER_HEIGHT)
	{
		lastError = ERROR_Y_INVALID;
		return blinkRedLedShort();
	}
	if (height > MAX_STEPS)
	{
		lastError = ERROR_INVALID_HEIGHT;
		return blinkRedLedShort();
	}

	byte index = y * CLUSTER_WIDTH + x;

	MCPData* data = &mcps[mcpPosition[index]];
	StepperData* stepper = &data->Steppers[stepperPosition[index]];
	if (checkRevision(stepper->LastRevision, revision))
	{
		stepper->LastRevision = revision;
		stepper->GotoSteps = height;
		stepper->TickCount = 0;
		stepper->WaitTime = waitTime;
	}
}

// setzt alle Schrittmotoren auf eine bestimmte H�he
void setAllSteps(int32_t revision, uint16_t gotoSteps, byte waitTime)
{
	if (gotoSteps > MAX_STEPS)
	{
		lastError = ERROR_INVALID_HEIGHT;
		return blinkRedLedShort();
	}

	for (byte i = 0; i < MCP_COUNT; i++) {
		for (byte j = 0; j < STEPPER_COUNT; j++) {
			StepperData* stepper = &mcps[i].Steppers[j];
			if (checkRevision(stepper->LastRevision, revision))
			{
				stepper->LastRevision = revision;
				stepper->GotoSteps = gotoSteps;
				stepper->TickCount = 0;
				stepper->WaitTime = waitTime;
			}
		}
	}
}

// setzt alle Werte aller Stepper so, dass sie aufh�ren sich zu bewegen
void stopMove() {
	for (byte i = 0; i < MCP_COUNT; i++) {
		for (byte j = 0; j < STEPPER_COUNT; j++) {
			StepperData* stepper = &mcps[i].Steppers[j];

			// stoppen
			stepper->GotoSteps = stepper->CurrentSteps;
		}
	}
}

#define STEPPER_DATA_START 128 // Bytes, Adresse im EEPROM an dem Stepper Data anfangen soll

// speichert die CurrentSteps der Stepper im EEPROM
void saveStepData() {
	updateEEPROM(STEPPER_DATA_START, 1); // speichern, dass Daten vorhanden sind

	int32_t dataPointer = STEPPER_DATA_START + 1;
	for (byte i = 0; i < MCP_COUNT; i++) {
		for (byte j = 0; j < STEPPER_COUNT; j++) {
			StepperData* stepper = &mcps[i].Steppers[j];
			updateEEPROM(dataPointer++, stepper->CurrentSteps & 0xFF);
			updateEEPROM(dataPointer++, (stepper->CurrentSteps >> 8) & 0xFF);
		}
	}
}

// versucht die CurrentSteps der Stepper aus dem EEPROM zu lesen
boolean tryLoadStepData() {
	if (EEPROM.read(STEPPER_DATA_START) != 1)
		return false; // keine Daten vorhanden

	int32_t dataPointer = STEPPER_DATA_START + 1;
	for (byte i = 0; i < MCP_COUNT; i++) {
		for (byte j = 0; j < STEPPER_COUNT; j++) {
			StepperData* stepper = &mcps[i].Steppers[j];
			stepper->CurrentSteps |= EEPROM.read(dataPointer++);
			stepper->CurrentSteps |= EEPROM.read(dataPointer++) << 8;
		}
	}
	EEPROM.write(STEPPER_DATA_START, 0);
	return true;
}

// liest einen unsigned short aus einem char-Array angefangen ab offset Bytes
uint16_t readUInt16(const char* data, int32_t offset)
{
	uint16_t val = 0;
	memcpy(&val, data + offset, sizeof(uint16_t));
	return val;
}

// liest einen int aus einem char-Array angefangen ab offset Bytes
int32_t readInt32(const char* data, int32_t offset)
{
	int32_t val = 0;
	memcpy(&val, data + offset, sizeof(int32_t));
	return val;
}

// schreibt einen unsigned short in einen char-Array angefangen ab offset Bytes
void writeUInt16(char* data, int32_t offset, const uint16_t val)
{
	memcpy(data + offset, &val, sizeof(uint16_t));
}


// schreibt einen int in einen char-Array angefangen ab offset Bytes
void writeInt32(char* data, int32_t offset, const int32_t val)
{
	memcpy(data + offset, &val, sizeof(int32_t));
}

void sendAckPacket(int32_t  revision)
{
	char packet[] = { 'K', 'K', 'S', 0, PacketAck, 0xDE, 0xAD, 0xBE, 0xEF };
	writeInt32(packet, 5, revision);
	ether.makeUdpReply(packet, sizeof(packet), PROTOCOL_PORT);
}

void sendData(int32_t  revision)
{
	char data[HEADER_SIZE + CLUSTER_WIDTH * CLUSTER_HEIGHT * 3];
	data[0] = 'K';
	data[1] = 'K';
	data[2] = 'S';
	data[3] = 0;
	data[4] = PacketGetData; // Paket-Type GetData
	writeInt32(data, 5, revision);

	int32_t offsetData = HEADER_SIZE;
	for (byte x = 0; x < CLUSTER_WIDTH; x++) {
		for (byte y = 0; y < CLUSTER_HEIGHT; y++)
		{
			byte index = y * CLUSTER_WIDTH + x;
			MCPData* mcp = &mcps[mcpPosition[index]];
			StepperData* stepper = &mcp->Steppers[stepperPosition[index]];
			writeUInt16(data, offsetData, (uint16_t)stepper->CurrentSteps);

			offsetData += 2;

			data[offsetData++] = stepper->WaitTime;
		}
	}

	if (offsetData > sizeof(data))
	{
		lastError = ERROR_BUFFER_OVERFLOW;
		blinkRedLedShort();
		return;
	}
	ether.makeUdpReply(data, sizeof(data), PROTOCOL_PORT);
}

void onPacketReceive(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char* data, uint16_t len)
{
	// alle Kugelmatik V3 Pakete
	// sind mindestens HEADER_SIZE Bytes lang
	// und fangen mit "KKS" an
	if (len < HEADER_SIZE || data[0] != 'K' || data[1] != 'K' || data[2] != 'S')
		return;

	boolean isGuaranteed = data[3] > 0;

	// erstes Byte nach dem Magicstring gibt den Paket-Typ an
	char packetType = data[4];

	// fortlaufende ID f�r die Pakete werden nach dem Magicstring und dem Paket-Typ geschickt
	int32_t  revision = readInt32(data, 5);

	// wenn ein busy-Befehl l�uft dann werden nur Ping, Info und Stop verarbeitet
	if (currentBusyCommand != BUSY_NONE && packetType != PacketPing && packetType != PacketInfo && packetType != PacketStop)
		return;

	// isGuaranteed gibt an ob der Sender eine Antwort erwartet
	if (isGuaranteed)
		sendAckPacket(revision);

	int32_t offset = HEADER_SIZE;
	switch (packetType)
	{
	case PacketPing:
		ether.makeUdpReply((char*)data, len, PROTOCOL_PORT); // das Ping-Packet funktioniert gleichzeitig auch als Echo-Funktion
		break;
	case PacketStepper:
		if (len < HEADER_SIZE + 4)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			byte x = (data[offset] >> 4) & 0xF;
			byte y = data[offset] & 0xF;
			offset += 1;

			uint16_t height = readUInt16(data, offset);
			offset += 2;

			byte waitTime = data[offset++];
			setStepper(revision, x, y, height, waitTime);
		}
		break;
	case PacketSteppers:
		if (len < HEADER_SIZE + 5)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			byte length = data[offset];
			offset += 1;

			uint16_t  height = readUInt16(data, offset);
			offset += 2;

			byte waitTime = data[offset++];

			if (len < offset + length)
			{
				lastError = ERROR_PACKET_TOO_SHORT;
				return blinkGreenLedShort();
			}

			for (byte i = 0; i < length; i++)
			{
				byte x = data[offset] >> 4;
				byte y = data[offset] & 0xF;
				setStepper(revision, x, y, height, waitTime);

				offset += 1;
			}
		}
		break;
	case PacketSteppersArray:
		if (len < HEADER_SIZE + 1)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			byte length = data[offset];
			offset += 1;

			if (len < offset + length * 4)
			{
				lastError = ERROR_PACKET_TOO_SHORT;
				return blinkGreenLedShort();
			}

			for (byte i = 0; i < length; i++)
			{
				byte x = data[offset] >> 4;
				byte y = data[offset] & 0xF;
				offset += 1;

				uint16_t  height = readUInt16(data, offset);
				offset += 2;

				byte waitTime = data[offset++];

				setStepper(revision, x, y, height, waitTime);
			}
		}
		break;
	case PacketSteppersRectangle:
		if (len < HEADER_SIZE + 2 + 3)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			byte minX = data[offset] >> 4;
			byte minY = data[offset] & 0xF;
			offset += 1;

			byte maxX = data[offset] >> 4;
			byte maxY = data[offset] & 0xF;
			offset += 1;

			uint16_t height = readUInt16(data, offset);
			offset += 2;

			byte waitTime = data[offset++];

			if (minX < 0 || minY < 0 || maxX >= CLUSTER_WIDTH || maxY >= CLUSTER_HEIGHT)
			{
				lastError = ERROR_INVALID_VALUE;
				return blinkGreenLedShort();
			}
			if (minX > maxX || minY > maxY)
			{
				lastError = ERROR_INVALID_VALUE;
				return blinkGreenLedShort();
			}

			for (byte x = minX; x <= maxX; x++) {
				for (byte y = minY; y <= maxY; y++) {
					setStepper(revision, x, y, height, waitTime);
				}
			}
		}
		break;
	case PacketSteppersRectangleArray:
		if (len < HEADER_SIZE + 2)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			byte minX = data[offset] >> 4;
			byte minY = data[offset] & 0xF;
			offset += 1;

			byte maxX = data[offset] >> 4;
			byte maxY = data[offset] & 0xF;
			offset += 1;

			if (minX < 0 || minX >= CLUSTER_WIDTH || minY < 0 || minY >= CLUSTER_HEIGHT)
			{
				lastError = ERROR_INVALID_VALUE;
				return blinkGreenLedShort();
			}
			if (minX > maxX || minY > maxY)
			{
				lastError = ERROR_INVALID_VALUE;
				return blinkGreenLedShort();
			}

			byte area = (maxX - minX + 1) * (maxY - minY + 1); // +1, da max die letzte Kugel nicht beinhaltet
			if (len < offset + 3 * area)
			{
				lastError = ERROR_PACKET_TOO_SHORT;
				return blinkGreenLedShort();
			}

			// beide for-Schleifen m�ssen mit dem Client �bereinstimmen sonst stimmen die Positionen nicht		
			for (byte x = minX; x <= maxX; x++) {
				for (byte y = minY; y <= maxY; y++)
				{
					uint16_t height = readUInt16(data, offset);
					offset += 2;

					byte waitTime = data[offset++];

					setStepper(revision, x, y, height, offset);
				}
			}
		}
		break;
	case PacketAllSteppers:
		if (len < HEADER_SIZE + 3)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			uint16_t height = readUInt16(data, offset);
			offset += 2;
			byte waitTime = data[offset++];
			setAllSteps(revision, height, waitTime);
		}
		break;
	case PacketAllSteppersArray:
		if (len < HEADER_SIZE + 3 * CLUSTER_WIDTH * CLUSTER_HEIGHT)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			// beide for-Schleifen m�ssen mit dem Client �bereinstimmen sonst stimmen die Positionen nicht		
			for (byte x = 0; x < CLUSTER_WIDTH; x++) {
				for (byte y = 0; y < CLUSTER_HEIGHT; y++)
				{
					uint16_t height = readUInt16(data, offset);
					offset += 2;

					byte waitTime = data[offset++];

					setStepper(revision, x, y, height, waitTime);
				}
			}
		}
		break;
	case PacketHome:
		if (len < HEADER_SIZE + 4)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			// 0xABCD wird benutzt damit man nicht ausversehen das Home-Paket schickt (wenn man z.B. den Paket-Type verwechselt)
			int32_t magic = readInt32(data, offset);
			if (magic != 0xABCD)
			{
				lastError = ERROR_INVALID_MAGIC;
				return blinkBothLedsShort();
			}

			// gotoSteps auf -MAX_STEPS setzen damit alle Kugeln voll nach oben fahren (negative Steps sind eigentlich verboten)
			for (byte i = 0; i < MCP_COUNT; i++) {
				for (byte j = 0; j < STEPPER_COUNT; j++)
				{
					StepperData* stepper = &mcps[i].Steppers[j];
					if (checkRevision(stepper->LastRevision, revision))
					{
						stepper->LastRevision = revision;
						stepper->CurrentSteps = 0;
						stepper->GotoSteps = -MAX_STEPS;
						stepper->CurrentStepIndex = 0;
						stepper->WaitTime = 0;
						stepper->TickCount = 0;
					}
				}
			}

			currentBusyCommand = BUSY_HOME;

			// alle Stepper nach oben Fahren lassen
			turnRedLedOn();
			for (int i = 0; i <= MAX_STEPS; i++)
			{
				if (stopBusyCommand)
					break;

				if (i % 100 == 0)
					toogleGreenLed();
				updateSteppers(true);
				usdelay(HOME_TIME); // langsam bewegen

				wdt_reset();

				// Pakete empfangen
#if RECEIVE_PACKETS_BUSY
				ether.packetLoop(ether.packetReceive());
#endif
			}
			currentBusyCommand = BUSY_NONE;
			stopBusyCommand = false;
			turnGreenLedOff();
			turnRedLedOff();

			// alle Stepper zur�cksetzen
			for (byte i = 0; i < MCP_COUNT; i++) {
				for (byte j = 0; j < STEPPER_COUNT; j++)
				{
					StepperData* stepper = &mcps[i].Steppers[j];
					stepper->GotoSteps = 0;
					stepper->CurrentSteps = 0;
					stepper->CurrentStepIndex = 0;
					stepper->WaitTime = 0;
					stepper->TickCount = 0;
				}
			}
		}
		break;
	case PacketResetRevision:
		for (byte i = 0; i < MCP_COUNT; i++) {
			for (byte j = 0; j < STEPPER_COUNT; j++) {
				StepperData* stepper = &mcps[i].Steppers[j];
				stepper->LastRevision = 0;
			}
		}
		break;
	case PacketFix:
		if (len < HEADER_SIZE + 5)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			// 0xDCBA wird benutzt damit man nicht ausversehen das Fix-Paket schickt (wenn man z.B. den Paket-Type verwechselt)
			int32_t magic = readInt32(data, offset);
			if (magic != 0xDCBA)
			{
				lastError = ERROR_INVALID_MAGIC;
				return blinkBothLedsShort();
			}
			offset += 4;

			byte x = (data[offset] >> 4) & 0xF;
			byte y = data[offset] & 0xF;
			offset += 1;

			if (x < 0 || x >= CLUSTER_WIDTH)
			{
				lastError = ERROR_X_INVALID;
				return blinkGreenLedShort();
			}
			if (y < 0 || y >= CLUSTER_HEIGHT)
			{
				lastError = ERROR_Y_INVALID;
				return blinkGreenLedShort();
			}

			byte index = y * CLUSTER_WIDTH + x;
			MCPData* data = &mcps[mcpPosition[index]];
			StepperData* stepper = &data->Steppers[stepperPosition[index]];
			if (checkRevision(stepper->LastRevision, revision))
			{
				stepper->LastRevision = revision;
				stepper->CurrentSteps = 0;
				stepper->GotoSteps = FIX_STEPS;
				stepper->WaitTime = 0;
				stepper->TickCount = 0;

				turnRedLedOn();
				currentBusyCommand = BUSY_FIX;
				for (int32_t i = 0; i < FIX_STEPS; i++)
				{
					if (stopBusyCommand)
						break;
					if (i % 100 == 0)
						toogleGreenLed();
					updateSteppers(true);
					usdelay(FIX_TIME); // langsam bewegen

					wdt_reset();

					// Pakete empfangen
#if RECEIVE_PACKETS_BUSY
					ether.packetLoop(ether.packetReceive());
#endif
				}

				currentBusyCommand = BUSY_NONE;
				stopBusyCommand = false;
				turnGreenLedOff();
				turnRedLedOff();

				// Stepper zur�cksetzen
				stepper->CurrentSteps = 0;
				stepper->GotoSteps = 0;
				stepper->CurrentStepIndex = 0;
			}
		}
		break;
	case PacketHomeStepper:
		if (len < HEADER_SIZE + 5)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			// 0xDCBA wird benutzt damit man nicht ausversehen das HomeStepper-Paket schickt (wenn man z.B. den Paket-Type verwechselt)
			int32_t magic = readInt32(data, offset);
			if (magic != 0xABCD)
			{
				lastError = ERROR_INVALID_MAGIC;
				return blinkBothLedsShort();
			}
			offset += 4;

			byte x = (data[offset] >> 4) & 0xF;
			byte y = data[offset] & 0xF;
			offset += 1;

			if (x < 0 || x >= CLUSTER_WIDTH)
			{
				lastError = ERROR_X_INVALID;
				return blinkGreenLedShort();
			}
			if (y < 0 || y >= CLUSTER_HEIGHT)
			{
				lastError = ERROR_Y_INVALID;
				return blinkGreenLedShort();
			}

			int index = y * CLUSTER_WIDTH + x;
			MCPData* data = &mcps[mcpPosition[index]];
			StepperData* stepper = &data->Steppers[stepperPosition[index]];
			if (checkRevision(stepper->LastRevision, revision))
			{
				// Stepper so setzen das er komplett hoch f�hrt
				stepper->LastRevision = revision;
				stepper->CurrentSteps = MAX_STEPS;
				stepper->GotoSteps = 0;
				stepper->WaitTime = 0; // WaitTime zur�ck setzen
				stepper->TickCount = 0;

				currentBusyCommand = BUSY_HOME_STEPPER;
				turnRedLedOn();
				for (int32_t i = 0; i <= MAX_STEPS; i++)
				{
					if (stopBusyCommand)
						break;
					if (i % 100 == 0)
						toogleGreenLed();
					updateSteppers(true);
					usdelay(HOME_TIME); // langsam bewegen

					wdt_reset();

					// Pakete empfangen
#if RECEIVE_PACKETS_BUSY
					ether.packetLoop(ether.packetReceive());
#endif
				}
				currentBusyCommand = BUSY_NONE;
				stopBusyCommand = false;
				turnGreenLedOff();
				turnRedLedOff();

				// Stepper zur�cksetzen
				stepper->CurrentSteps = 0;
				stepper->GotoSteps = 0;
				stepper->CurrentStepIndex = 0;
			}
		}
		break;
	case PacketGetData:
	{
		sendData(revision);
	}
	break;
	case PacketInfo:
	{
		char data[HEADER_SIZE + 17];
		data[0] = 'K';
		data[1] = 'K';
		data[2] = 'S';
		data[3] = 0;
		data[4] = PacketInfo; // Paket-Type Info
		writeInt32(data, 5, revision);

		int32_t offsetData = HEADER_SIZE;
		data[offsetData++] = BUILD_VERSION;

		data[offsetData++] = currentBusyCommand;

		int32_t highestRevision = INT_MIN;
		for (byte i = 0; i < MCP_COUNT; i++)
			for (byte j = 0; j < STEPPER_COUNT; j++) {
				StepperData* stepper = &mcps[i].Steppers[j];

				if (stepper->LastRevision > highestRevision)
					highestRevision = stepper->LastRevision;
			}

		if (configRevision > highestRevision)
			highestRevision = configRevision;

		if (setDataRevision > highestRevision)
			highestRevision = setDataRevision;

		writeInt32(data, offsetData, highestRevision);
		offsetData += 4;

		data[offsetData++] = stepMode;

		writeInt32(data, offsetData, tickTime);
		offsetData += 4;

		data[offsetData++] = useBreak ? 1 : 0;

		data[offsetData++] = lastError;

		writeInt32(data, offsetData, freeRam());
		offset += 4;


		if (offsetData > sizeof(data))
		{
			lastError = ERROR_BUFFER_OVERFLOW;
			blinkRedLedShort();
			return;
		}
		ether.makeUdpReply(data, sizeof(data), PROTOCOL_PORT);
	}
	break;
	case PacketConfig:
		if (len < HEADER_SIZE + 5)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			if (!checkRevision(configRevision, revision))
				break;

			configRevision = revision;

			byte cStepMode = data[offset++];
			if (cStepMode < 1 || cStepMode > 3)
			{
				lastError = ERROR_INVALID_CONFIG_VALUE;
				return blinkBothLedsShort();
			}

			int32_t cTickTime = readInt32(data, offset);
			offset += 4;
			if (cTickTime < 50 || cTickTime > 15000)
			{
				lastError = ERROR_INVALID_CONFIG_VALUE;
				return blinkBothLedsShort();
			}

			boolean cUseBreak = data[offset++] > 0;

			stepMode = cStepMode;
			tickTime = cTickTime;
			useBreak = cUseBreak;
		}
		break;
	case PacketBlinkGreen:
		blinkGreenLedShort();
		break;
	case PacketBlinkRed:
		blinkRedLedShort();
		break;
	case PacketStop:
#if ALLOW_STOP_BUSY
		if (currentBusyCommand != BUSY_NONE)
			stopBusyCommand = true;
#endif
#if ALLOW_STOP_MOVE
		stopMove();

		// Client informieren, dass es m�glicherweise neue Daten gibt
		sendData(revision);
#endif
		break;
	case PacketSetData:
		if (len < HEADER_SIZE + CLUSTER_WIDTH * CLUSTER_HEIGHT * 2)
		{
			lastError = ERROR_PACKET_TOO_SHORT;
			return blinkGreenLedShort();
		}
		else
		{
			if (!checkRevision(setDataRevision, revision))
				break;

			setDataRevision = revision;

			for (byte x = 0; x < CLUSTER_WIDTH; x++) {
				for (byte y = 0; y < CLUSTER_HEIGHT; y++)
				{
					byte index = y * CLUSTER_WIDTH + x;
					MCPData* mcp = &mcps[mcpPosition[index]];
					StepperData* stepper = &mcp->Steppers[stepperPosition[index]];

					uint16_t height = readUInt16(data, offset);
					offset += 2;

					if (height > MAX_STEPS) {
						lastError = ERROR_INVALID_HEIGHT;
						return blinkBothLedsShort();
					}

					stepper->CurrentSteps = height;
					stopMove();
				}
			}
		}
		break;
	default:
		lastError = ERROR_UNKOWN_PACKET;
		return blinkRedLedShort();
	}
#if BLINK_PACKET
	turnGreenLedOn();
#endif
}


void setup()
{
	wdt_disable(); // Watch Dog deaktivieren, da er noch aktiviert sein kann
	printLogString("init");

#if ENABLE_TIMER1
	printLogString("timer1");

	noInterrupts();
	TCCR1B |= (1 << CS11);
	TCNT1 = 0;
	TIMSK |= (1 << TOIE1);
	interrupts();
#endif

	// LEDs setzen
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_RED, OUTPUT);

	turnGreenLedOn();
	initAllMCPs();

	tryLoadStepData();

	delay(LAN_ID * 10); // Init verz�gern damit das Netzwerk nicht �berlastet wird

	printLogString("ebegin");
	uint8_t rev = ether.begin(sizeof Ethernet::buffer, ethernetMac, 28);
	if (rev == 0)
	{
		error(1, "ethernet begin failed");
		return; // wird niemals passieren da error() in eine Endloschleife geht
	}

	printLogString("dhcp");
	if (!ether.dhcpSetup())
	{
		error(2, "dhcp failed");
		return;
	}

	// IP in den Log schreiben
	printLogString("ip");
	for (int i = 0; i < 4; i++) // IPv4 -> 4 Bytes
		printLog(ether.myip[i]);

	ether.udpServerListenOnPort(&onPacketReceive, PROTOCOL_PORT);

	turnGreenLedOff();

#if ENABLE_WATCH_DOG
	noInterrupts();
	wdt_enable(WDTO_2S);

#if ENABLE_WATCH_DOG_SAVE
	WDTCSR |= 1 << WDIE;
#endif
	interrupts();
#endif
}

void updateSteppers(boolean alwaysUseHalfStep)
{
	for (byte i = 0; i < MCP_COUNT; i++)
	{
		// Wert f�r den GPIO
		uint16_t gpioValue = 0;

		for (byte j = 0; j < STEPPER_COUNT; j++)
		{
			StepperData* stepper = &mcps[i].Steppers[j];

			byte stepSize = 0;
			int32_t diff = abs(stepper->CurrentSteps - stepper->GotoSteps);
			if (diff != 0)
			{
				if (stepMode == 3) // StepMode Both, schauen ob wir Full oder Half Step gemacht werden soll
					stepSize = min(2, diff);
				else if (diff >= stepMode) // Half oder Full Step machen
					stepSize = stepMode;
			}

			if (alwaysUseHalfStep)
				stepSize = min(stepSize, 1);

			if (stepper->WaitTime > 0 && stepper->TickCount >= 0)
				stepper->TickCount--;

			if (stepSize > 0 && (stepper->WaitTime == 0 || stepper->TickCount < 0))
			{
				int8_t stepperIndex = stepper->CurrentStepIndex;
				if (stepperIndex % stepSize > 0) // schauen ob wir noch einen Zwischenschritt machen m�ssen
					stepSize = 1;

				if (stepper->GotoSteps < stepper->CurrentSteps)  // nach oben fahren
				{
					stepper->CurrentSteps -= stepSize;
					stepperIndex -= stepSize;
				}
				else // nach unten fahren
				{
					stepper->CurrentSteps += stepSize;
					stepperIndex += stepSize;
				}

				// stepperIndex in den richtigen Bereich bringen (underflow/overflow)
				if (stepperIndex < 0)
					stepperIndex = STEPPER_STEP_COUNT - stepSize;
				else if (stepperIndex >= STEPPER_STEP_COUNT)
					stepperIndex = 0;

				gpioValue |= stepsStepper[stepperIndex] << (4 * j); // jeder Wert in stepsStepper ist 4 Bit lang

				stepper->CurrentStepIndex = (byte)stepperIndex;

				stepper->TickCount = stepper->WaitTime;
			}
			else if (useBreak) // Bremse benutzen?
				gpioValue |= stepsStepper[stepper->CurrentStepIndex] << (4 * j);
		}
		mcps[i].MCP.writeGPIOS(gpioValue);
	}
}

void usdelay(uint16_t us) {
	while (us--) {
		// 4 times * 4 cycles gives 16 cyles = 1 us with 16 MHz clocking
		byte i = 4;
		// this while loop executes with exact 4 cycles:
		while (i--)
			asm volatile("nop");
	}
}

// gibt den freien SRAM zur�ck, siehe http://playground.arduino.cc/Code/AvailableMemory
int freeRam() {
	extern int __heap_start, *__brkval;
	int v;
	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void loop()
{
#if DISABLE_INTERRUPTS
	noInterrupts();
#endif
	unsigned long procStart = micros();

	updateSteppers(false);

	while (true)
	{
		// Packet abfragen
		word plen = ether.packetReceive();
		if (plen > 0) // neues Packet empfangen
			ether.packetLoop(plen);

#if BLINK_PACKET
		turnGreenLedOff();
#endif

		wdt_reset();

		// schauen ob wir die Stepper updaten m�ssen
		unsigned long time = micros();
		if (time < procStart) // overflow von micros() handeln
			break;

		if (time - procStart >= tickTime)
			break;
	}

#if DISABLE_INTERRUPTS
	interrupts();
#endif
}

int16_t tickCount = 0;

ISR(TIMER1_OVF_vect) {
	if (tickCount++ >= 128) {
#if BLINK_LED_ASYNC
		if (tickCount % 128 == 0)
			toogleGreenLed();
#endif
#if WRITE_INTERSTRUCTION_POINTER
		if (tickCount % 128 == 0) {
			byte addr1 = 0;
			byte addr2 = 0;
			// Adresse von Stack nehmen
			asm volatile ("pop %0" : "=w"(addr1));
			asm volatile ("pop %0" : "=w"(addr2));

			// Adresse wieder auf Stack kopieren
			asm volatile ("push %0" ::  "w"(addr2));
			asm volatile ("push %0" ::  "w"(addr1));

			updateEEPROM(1, addr1);
			updateEEPROM(2, addr2);
		}
#endif

		tickCount = 0;
	}
}


// wird aufgerufen wenn der WatchDog aktiviert wurde und der Chip zur�ck gesetzt wird
ISR(WDT_vect) {
	turnRedLedOn();
	wdt_disable();

	printLogString("watchdog");
#if ENABLE_WATCH_DOG_SAVE
	saveStepData();
#endif

	softReset();
}