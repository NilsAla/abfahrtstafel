// Systemfunktionen: Reset-Grund, Task-Watchdog, geplanter Neustart, Factory Reset per BOOT-Taster.
#pragma once

#include <stdint.h>

namespace sys {

const char *resetReasonText();

void watchdogBegin(uint32_t timeoutMs);  // überwacht den Loop-Task
void watchdogFeed();

void scheduleRestart(uint32_t delayMs);  // aus jedem Task aufrufbar

// Im Loop aufrufen (≤ 50 ms Abstand): Taster, geplanter Neustart
void loop();

}  // namespace sys
