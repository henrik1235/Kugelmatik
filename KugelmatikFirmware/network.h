#pragma once

#include <Arduino.h>

#include <limits.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "util.h"
#include "constants.h"
#include "stepper.h"
#include "tick.h"
#include "config.h"
#include "watchdog.h"
#include "PacketBuffer.h"
#include "serial.h"

#define LAN_ID 0x10					// ID des Boards im Netzwerk, wird benutzt um die Mac-Adresse zu generieren
#define PROTOCOL_PORT 14804			// Port f�r das Protokoll �ber UDP
#define NETWORK_BUFFER_SIZE 350		// Gr��e des Netzwerk Buffers in Bytes	
#define HEADER_SIZE 9				// Gr��e des Paket-Headers in Bytes

#define KUGELMATIK_NETWORK_SSID "Kugelmatik"
#define KUGELMATIK_NETWORK_PASSWORD "12345678"
#define AP_PASSWORD "Kugelmatik"

extern int32_t loopTime;
extern int32_t networkTime;
extern int32_t maxNetworkTime;
extern int32_t stepperTime;

boolean checkRevision(int32_t lastRevision, int32_t revision);

void initNetwork();
boolean loopNetwork();

void sendPacket();
void writeHeader(boolean guarenteed, uint8_t packetType, int32_t revision);
boolean readPosition(uint8_t* x, uint8_t* y);

void sendAckPacket(int32_t revision);
void sendData(int32_t revision);
void sendInfo(int32_t revision);

void onPacketReceive();
void handlePacket(uint8_t packetType, int32_t revision);

void runBusy(uint8_t type, int32_t steps, uint32_t delay);