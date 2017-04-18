#pragma once

#include <Arduino.h>

#include <limits.h>
#include <EtherCard.h>

#include "util.h"
#include "constants.h"
#include "stepper.h"
#include "tick.h"
#include "config.h"
#include "watchdog.h"
#include "PacketBuffer.h"

#define LAN_ID 0x11					// ID des Boards im LAN, wird benutzt um die Mac-Adresse zu generieren
#define PROTOCOL_PORT 14804			// Port f�r das Protokoll �ber UDP
#define ETHERNET_BUFFER_SIZE 300	// Gr��e des Ethernet Buffers in Bytes	
#define HEADER_SIZE 9				// Gr��e des Paket-Headers in Bytes

extern int32_t loopTime;
extern int32_t networkTime;
extern int32_t maxNetworkTime;
extern int32_t stepperTime;

boolean checkRevision(int32_t lastRevision, int32_t revision);

void initNetwork();
boolean loopNetwork();

void sendPacket();
void writeHeader(bool guarenteed, byte packetType, int32_t revision);
bool readPosition(PacketBuffer* packet, byte* x, byte* y);

void sendAckPacket(int32_t revision);
void sendData(int32_t revision);
void sendInfo(int32_t revision, boolean wantConfig2);

void onPacketReceive(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char* data, uint16_t len);
void handlePacket(uint8_t packetType, int32_t revision);

void runBusy(uint8_t type, int32_t steps, uint32_t delay);