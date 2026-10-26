#include "hold_button.h"

namespace abf {

HoldButton::HoldButton(uint32_t holdMs, uint32_t debounceMs) : holdMs_(holdMs), debounceMs_(debounceMs) {}

HoldButton::Event HoldButton::update(bool raw, uint32_t now) {
  if (raw != raw_) {
    raw_ = raw;
    rawChangedAt_ = now;
  }

  if (raw_ != stable_ && now - rawChangedAt_ >= debounceMs_) {
    stable_ = raw_;
    if (stable_) {
      pressedAt_ = rawChangedAt_;
      holdFired_ = false;
      return Event::Pressed;
    }
    return Event::Released;
  }

  if (stable_ && !holdFired_ && now - pressedAt_ >= holdMs_) {
    holdFired_ = true;
    return Event::HoldReached;
  }
  return Event::None;
}

}  // namespace abf
