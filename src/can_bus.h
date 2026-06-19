#pragma once

#include <Arduino.h>
#include <driver/twai.h>

extern bool canStarted;
extern unsigned long canFrameCount;
extern unsigned long canDecodedFrameCount;
extern unsigned long lastCanFrameMs;
extern unsigned long lastCanDecodedMs;

bool startCan();
void readCanFrames();
void handleCanStatus();
