// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "board.h"

#include <Arduino.h>

namespace board {

void initButtons() { pinMode(kBootButtonPin, INPUT_PULLUP); }

bool bootButtonPressed() { return digitalRead(kBootButtonPin) == LOW; }

}  // namespace board
