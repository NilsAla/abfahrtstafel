// Entprellter Taster mit Halte-Erkennung (Factory Reset: BOOT 10 s halten).
// Zeitbasis millis(); Überlauf ist durch vorzeichenlose Differenzen abgedeckt.
#pragma once

#include <stdint.h>

namespace abf {

class HoldButton {
 public:
  enum class Event { None, Pressed, Released, HoldReached };

  HoldButton(uint32_t holdMs, uint32_t debounceMs = 50);

  // Regelmäßig aufrufen (≤ 50 ms Abstand). raw = true heißt gedrückt.
  Event update(bool raw, uint32_t now);

  bool pressed() const { return stable_; }
  // Wie lange der Taster (entprellt) schon gedrückt ist, sonst 0.
  uint32_t heldMs(uint32_t now) const { return stable_ ? now - pressedAt_ : 0; }

 private:
  uint32_t holdMs_;
  uint32_t debounceMs_;
  bool raw_ = false;
  bool stable_ = false;
  bool holdFired_ = false;
  uint32_t rawChangedAt_ = 0;
  uint32_t pressedAt_ = 0;
};

}  // namespace abf
