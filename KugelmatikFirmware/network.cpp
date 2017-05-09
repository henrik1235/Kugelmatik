#include "network.h"

int32_t loopTime = 0;
int32_t networkTime = 0;
int32_t maxNetworkTime = 0;
int32_t stepperTime = 0;

const static uint8_t ethernetMac[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, LAN_ID }; // Mac-Adresse des Boards
uint8_t Ethernet::buffer[ETHERNET_BUFFER_SIZE];	

int32_t configRevision = 0;		// die letzte Revision des Config-Packets
int32_t setDataRevision = 0;	// die letzte Revision des SetData-Packets
int32_t clearErrorRevision = 0; // die letzte Revision des ClearError-Packets

uint8_t currentBusyCommand = BUSY_NONE;
boolean stopBusyCommand = false;

PacketBuffer* packet;
uint8_t dummyBuffer[1];

// gibt true zur�ck wenn revision neuer ist als lastRevision
boolean checkRevision(int32_t lastRevision, int32_t revision)
{
	if (lastRevision >= 0 && revision < 0) // Overflow handeln
		return true;
	return revision > lastRevision;
}

void initNetwork()
{
	Serial.println(F("initNetwork()"));
	Serial.println(F("ether.begin()"));

	uint8_t rev = ether.begin(ETHERNET_BUFFER_SIZE, ethernetMac, 28);
	if (rev == 0)
	{
		error("init", "ethernet begin failed", true);
		return; // wird niemals passieren da error() in eine Endloschleife geht
	}

	Serial.println(F("Waiting for link..."));

	// warten bis Ethernet Kabel verbunden ist
	while (!ether.isLinkUp())
	{
		toogleGreenLed();
		delay(300);
	}

	turnGreenLedOn();

	uint8_t lanID = ether.mymac[5];
	delay(lanID * 20); // Init verz�gern damit das Netzwerk nicht �berlastet wird

	// Hostname generieren (die 00 wird durch die lanID in hex ersetzt)
	char hostName[] = "Kugelmatik-00";
	hostName[11] = getHexChar(lanID >> 4);
	hostName[12] = getHexChar(lanID);

	Serial.println(F("Link up..."));
	Serial.print(F("ether.dhcpSetup(), using hostname: "));
	Serial.println(hostName);

	if (!ether.dhcpSetup(hostName, true)) 
	{
		error("init", "dhcp failed", false);
		return;
	}

	// PacketBuffer benutzt sp�ter den gleichen Buffer wie die Ethernetklasse
	// daher wird hier nur ein dummyBuffer gesetzt
	packet = new PacketBuffer(dummyBuffer, sizeof(dummyBuffer));
	
	Serial.println(F("Network boot done!"));
	ether.udpServerListenOnPort(&onPacketReceive, PROTOCOL_PORT);
}

boolean loopNetwork() {
	startTime(TIMER_NETWORK);
	wdt_yield();

	// Paket abfragen
	ether.packetLoop(ether.packetReceive());

	boolean linkUp = ether.isLinkUp();
	if (!linkUp) 	// wenn Netzwerk Verbindung getrennt wurde
		stopMove(); // stoppen

	networkTime = endTime(TIMER_NETWORK);
	if (networkTime > maxNetworkTime || maxNetworkTime > networkTime * 4) // maxNetworkTime Windup vermeiden
		maxNetworkTime = networkTime;
	return linkUp;
}

void sendPacket()
{
	if (packet->getError())
		return;
	ether.makeUdpReply((char*)packet->getBuffer(), packet->getPosition(), PROTOCOL_PORT);
}

void writeHeader(boolean guarenteed, uint8_t packetType, int32_t revision)
{
	packet->resetPosition();
	packet->setSize(packet->getBufferSize()); // Size �berschreiben, da die Size vom Lesen gesetzt wird
	packet->write('K');
	packet->write('K');
	packet->write('S');
	packet->write(guarenteed);
	packet->write(packetType);
	packet->write(revision);
}


boolean readPosition(PacketBuffer* packet, uint8_t* x, uint8_t* y)
{
	uint8_t pos = packet->readUint8();
	uint8_t xvalue = (pos >> 4) & 0xF;
	uint8_t yvalue = pos & 0xF;

	if (xvalue < 0 || xvalue >= CLUSTER_WIDTH)
	{
		protocolError(ERROR_X_INVALID);
		return false;
	}
	if (yvalue < 0 || yvalue >= CLUSTER_HEIGHT)
	{
		protocolError(ERROR_Y_INVALID);
		return false;
	}

	*x = xvalue;
	*y = yvalue;
	return true;
}

void sendAckPacket(int32_t revision)
{
	writeHeader(false, PacketAck, revision);
	sendPacket();
}

void sendData(int32_t revision)
{
	writeHeader(false, PacketGetData, revision);

	for (uint8_t x = 0; x < CLUSTER_WIDTH; x++) {
		for (uint8_t y = 0; y < CLUSTER_HEIGHT; y++)
		{
			StepperData* stepper = getStepper(x, y);

			packet->write((uint16_t)max(0, stepper->CurrentSteps));
			packet->write(stepper->WaitTime);
		}
	}

	sendPacket();
}

void sendInfo(int32_t revision) {
	int32_t highestRevision = INT_MIN;
	for (int32_t i = 0; i < CLUSTER_SIZE; i++) {
		StepperData* stepper = getStepper(i);

		if (stepper->LastRevision > highestRevision)
			highestRevision = stepper->LastRevision;
	}

	if (configRevision > highestRevision)
		highestRevision = configRevision;

	if (setDataRevision > highestRevision)
		highestRevision = setDataRevision;

	writeHeader(false, PacketInfo, revision);

	packet->write((uint8_t)BUILD_VERSION);
	packet->write(currentBusyCommand);
	packet->write(highestRevision);
	packet->write(lastError);
	packet->write((uint64_t)freeRam());

	packet->write((uint16_t)sizeof(Config));
	packet->write((uint8_t*)&config, sizeof(Config));

	uint8_t mcpStatus = 0;
	for (uint8_t i = 0; i < MCP_COUNT; i++)
		if (mcps[i].isOK)
			mcpStatus |= (1 << i);
	packet->write(mcpStatus);

	packet->write(loopTime);
	packet->write(networkTime);
	packet->write(maxNetworkTime);
	packet->write(stepperTime);
	packet->write((int32_t)(millis() / 1000));

	sendPacket();
}

void onPacketReceive(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char* data, uint16_t len)
{
	if (data == NULL)
		return internalError();

	// alle Kugelmatik V3 Pakete
	// sind mindestens HEADER_SIZE Bytes lang
	// und fangen mit "KKS" an
	if (len < HEADER_SIZE || data[0] != 'K' || data[1] != 'K' || data[2] != 'S')
		return;

	// Fehler aus dem PacketBuffer l�schen
	if (packet->getError())
		Serial.println(F("packet->getError() == true"));

	// data ist unser Etherner Buffer (verschoben um die UDP Header L�nge)
	// wir nutzen den selben Buffer zum Lesen und zum Schreiben
	packet->setBuffer((uint8_t*)data, ETHERNET_BUFFER_SIZE - 28); // 28 Bytes f�r IP + UDP Header abziehen
	packet->setSize(len);
	packet->seek(3); 

	boolean isGuaranteed = packet->readBoolean();

	// erstes Byte nach dem Magicstring gibt den Paket-Typ an
	uint8_t packetType = packet->readUint8();

	// fortlaufende ID f�r die Pakete werden nach dem Magicstring und dem Paket-Typ geschickt
	int32_t revision = packet->readInt32();

	// wenn ein busy-Befehl l�uft dann werden nur Ping, Info und Stop verarbeitet
	if (currentBusyCommand != BUSY_NONE && packetType != PacketPing && packetType != PacketInfo && packetType != PacketStop)
		return;

	handlePacket(packetType, revision);

	// isGuaranteed gibt an ob der Sender eine Antwort erwartet
	if (isGuaranteed)
		sendAckPacket(revision);
}

void handlePacket(uint8_t packetType, int32_t revision)
{
	switch (packetType)
	{
	case PacketPing:
		ether.makeUdpReply((char*)packet->getBuffer(), packet->getSize(), PROTOCOL_PORT); // das Ping-Packet funktioniert gleichzeitig auch als Echo-Funktion
		break;
	case PacketStepper:
	{
		uint8_t x, y;
		if (!readPosition(packet, &x, &y))
			return;

		uint16_t height = packet->readUint16();
		uint8_t waitTime = packet->readUint8();

		if (packet->getError())
			return;

		setStepper(revision, x, y, height, waitTime);
		break;
	}
	case PacketSteppers:
	{
		uint8_t length = packet->readUint8();
		uint16_t height = packet->readUint16();
		uint8_t waitTime = packet->readUint8();

		for (uint8_t i = 0; i < length; i++)
		{
			uint8_t x, y;
			readPosition(packet, &x, &y);

			if (packet->getError())
				return;
			setStepper(revision, x, y, height, waitTime);
		}

		break;
	}
	case PacketSteppersArray:
	{
		uint8_t length = packet->readUint8();

		for (uint8_t i = 0; i < length; i++)
		{
			uint8_t x, y;
			if (!readPosition(packet, &x, &y))
				return;

			uint16_t height = packet->readUint16();
			uint8_t waitTime = packet->readUint8();

			if (packet->getError())
				return;

			setStepper(revision, x, y, height, waitTime);
		}
		break;
	}
	case PacketSteppersRectangle:
	{
		uint8_t minX, minY;
		if (!readPosition(packet, &minX, &minY))
			return;

		uint8_t maxX, maxY;
		if (!readPosition(packet, &maxX, &maxY))
			return;

		uint16_t height = packet->readUint16();
		uint8_t waitTime = packet->readUint8();

		if (minX > maxX || minY > maxY)
			return protocolError(ERROR_INVALID_VALUE);

		if (packet->getError())
			return;

		for (uint8_t x = minX; x <= maxX; x++)
			for (uint8_t y = minY; y <= maxY; y++)
				setStepper(revision, x, y, height, waitTime);
		break;
	}
	case PacketSteppersRectangleArray:
	{
		uint8_t minX, minY;
		if (!readPosition(packet, &minX, &minY))
			return;

		uint8_t maxX, maxY;
		if (!readPosition(packet, &maxX, &maxY))
			return;

		if (minX > maxX || minY > maxY)
			return protocolError(ERROR_INVALID_VALUE);

		// beide for-Schleifen m�ssen mit dem Client �bereinstimmen sonst stimmen die Positionen nicht		
		for (uint8_t x = minX; x <= maxX; x++) {
			for (uint8_t y = minY; y <= maxY; y++) {
				uint16_t height = packet->readUint16();
				uint8_t waitTime = packet->readUint8();

				if (packet->getError())
					return;

				setStepper(revision, x, y, height, waitTime);
			}
		}
		break;
	}
	case PacketAllSteppers:
	{
		uint16_t height = packet->readUint16();
		uint8_t waitTime = packet->readUint8();

		if (packet->getError())
			return;
		setAllSteps(revision, height, waitTime);
		break;
	}
	case PacketAllSteppersArray:
	{
		// beide for-Schleifen m�ssen mit dem Client �bereinstimmen sonst stimmen die Positionen nicht		
		for (uint8_t x = 0; x < CLUSTER_WIDTH; x++) {
			for (uint8_t y = 0; y < CLUSTER_HEIGHT; y++) {
				uint16_t height = packet->readUint16();
				uint8_t waitTime = packet->readUint8();

				if (packet->getError())
					return;
				setStepper(revision, x, y, height, waitTime);
			}
		}
		break;
	}
	case PacketHome:
	{
		// 0xABCD wird benutzt damit man nicht ausversehen das Home-Paket schickt (wenn man z.B. den Paket-Type verwechselt)
		int32_t magic = packet->readInt32();

		if (packet->getError())
			return;
		if (magic != 0xABCD)
			return protocolError(ERROR_INVALID_MAGIC);

		stopMove();

		boolean stepperSet = false;

		// Stepper f�r Home Befehl vorbereiten
		for (int32_t i = 0; i < CLUSTER_SIZE; i++) {
			StepperData* stepper = getStepper(i);

			if (checkRevision(stepper->LastRevision, revision)) {
				forceStepper(stepper, revision, -config.homeSteps);
				stepperSet = true;
			}
		}

		if (stepperSet) {
			runBusy(BUSY_HOME, config.homeSteps, config.homeTime);


			// alle Stepper zur�cksetzen
			for (int32_t i = 0; i < CLUSTER_SIZE; i++) {
				StepperData* stepper = getStepper(i);
				if (stepper->LastRevision == revision)
					resetStepper(stepper);
			}
		}

		break;
	}
	case PacketResetRevision:
	{
		configRevision = 0;
		setDataRevision = 0;

		for (int32_t i = 0; i < CLUSTER_SIZE; i++)
			getStepper(i)->LastRevision = 0;
		break;
	}
	case PacketFix:
	{
		// 0xDCBA wird benutzt damit man nicht ausversehen das Fix-Paket schickt (wenn man z.B. den Paket-Type verwechselt)
		int32_t magic = packet->readInt32();
		if (magic != 0xDCBA)
			return protocolError(ERROR_INVALID_MAGIC);

		uint8_t x, y;
		if (!readPosition(packet, &x, &y))
			return;

		if (packet->getError())
			return;

		StepperData* stepper = getStepper(x, y);
		if (checkRevision(stepper->LastRevision, revision))
		{
			stopMove();

			forceStepper(stepper, revision, config.fixSteps);
			runBusy(BUSY_FIX, config.fixSteps, config.fixTime);
			resetStepper(stepper);
		}
		break;
	}
	case PacketHomeStepper:
	{
		// 0xDCBA wird benutzt damit man nicht ausversehen das HomeStepper-Paket schickt (wenn man z.B. den Paket-Type verwechselt)
		int32_t magic = packet->readInt32();
		if (magic != 0xABCD)
			return protocolError(ERROR_INVALID_MAGIC);

		uint8_t x, y;
		if (!readPosition(packet, &x, &y))
			return;

		if (packet->getError())
			return;

		StepperData* stepper = getStepper(x, y);
		if (checkRevision(stepper->LastRevision, revision))
		{
			stopMove();

			forceStepper(stepper, revision, -config.homeSteps);
			runBusy(BUSY_HOME_STEPPER, config.homeSteps, config.homeTime);
			resetStepper(stepper);
		}
		break;
	}
	case PacketGetData:
	{
		sendData(revision);
		break;
	}
	case PacketInfo:
	{
		sendInfo(revision);
		break;
	}
	case PacketBlinkGreen:
		blinkGreenLedShort(false);
		break;
	case PacketBlinkRed:
		blinkRedLedShort(false);
		break;
	case PacketStop:
	{
		if (currentBusyCommand != BUSY_NONE)
			stopBusyCommand = true;
		else
		{
			stopMove();

			// Client informieren, dass es m�glicherweise neue Daten gibt
			sendData(revision);
		}
		break;
	}
	case PacketSetData:
	{
		if (!checkRevision(setDataRevision, revision))
			break;

		setDataRevision = revision;

		Serial.println(F("PacketSetData received"));

		for (uint8_t x = 0; x < CLUSTER_WIDTH; x++) {
			for (uint8_t y = 0; y < CLUSTER_HEIGHT; y++) {
				StepperData* stepper = getStepper(x, y);

				uint16_t height = packet->readUint16();
				if (height > config.maxSteps)
					return protocolError(ERROR_INVALID_HEIGHT);

				if (packet->getError())
					return;

				stepper->CurrentSteps = height;
				stepper->GotoSteps = height;
				stepper->TickCount = 0;
			}
		}
		break;
	}
	case PacketConfig2:
	{
		if (!checkRevision(configRevision, revision))
			break;

		configRevision = revision;

		uint16_t size = packet->readUint16();
		if (size != sizeof(Config))
			return protocolError(ERROR_INVALID_CONFIG_VALUE);

		Config newConfig;
		packet->read((uint8_t*)&newConfig, sizeof(Config));

		if (packet->getError()) 
			return;

		if (!checkConfig(&newConfig))
			return protocolError(ERROR_INVALID_CONFIG_VALUE);

		memcpy(&config, &newConfig, sizeof(Config));
		Serial.println(F("Config2 set by network!"));

		sendInfo(revision);
		break;
	}
	case PacketClearError:
		if (!checkRevision(clearErrorRevision, revision))
			break;

		clearErrorRevision = revision;
		lastError = ERROR_NONE;
		sendInfo(revision);
		break;
	default:
		return protocolError(ERROR_UNKNOWN_PACKET);
	}
}

void runBusy(uint8_t type, int32_t steps, uint32_t delay)
{
	Serial.print(F("runBusy(type = "));
	Serial.print(type);
	Serial.println(F(")"));

	currentBusyCommand = type;
	turnRedLedOn();
	for (int32_t i = 0; i <= steps; i++) {
		if (stopBusyCommand)
			break;

		if (i % 100 == 0)
			toogleGreenLed();

		if (!runTick(delay, true))
			break;
	}

	currentBusyCommand = BUSY_NONE;
	stopBusyCommand = false;
	turnGreenLedOff();
	turnRedLedOff();

	// Timer zur�ck setzen, damit LoopTime und NetworkTime nicht kurzzeitig in die H�he springt
	startTime(TIMER_LOOP);
	startTime(TIMER_NETWORK); 
}