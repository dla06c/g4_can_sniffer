#pragma once

#include <WebServer.h>
#include <WiFiUdp.h>

extern WebServer server;
extern WiFiUDP udp;

void setupRoutes();
void handleRoot();
void handleData();
void readUdpPackets();
