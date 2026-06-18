#pragma once

#include <WebServer.h>
#include <WiFiUdp.h>

extern WebServer server;
extern WiFiUdp udp;

void setupRoutes();
void handleRoot();
void handleData();
void readUdpPackets();
