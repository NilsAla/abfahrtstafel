// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "board.h"

#include <Arduino.h>

namespace board {

void initButtons() { pinMode(kBootButtonPin, INPUT_PULLUP); }

bool bootButtonPressed() { return digitalRead(kBootButtonPin) == LOW; }

// Platzhalter bis M0.5 (Treiber für ST7701/RGB-Panel). Ohne Display läuft die Firmware wie bisher.
bool displayBegin() { return false; }

void displayFlush(int32_t, int32_t, int32_t, int32_t, const uint16_t *) {}

void setBacklight(uint8_t) {}

}  // namespace board
