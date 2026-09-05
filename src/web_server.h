#pragma once

#include <WebServer.h>
#include <WiFiUdp.h>

extern WebServer server;
extern WiFiUDP udp;

void setupRoutes();
void handleRoot();
void handleData();
void serviceTelemetryWebSocket();
size_t buildTelemetryJson(char* json, size_t capacity);
void readUdpPackets();
