#include "boot_anim.h"

#include <math.h>

namespace abf {
namespace boot {

namespace {

float clamp01(float u) { return u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u); }
float lin(float t, float a, float b) { return clamp01((t - a) / (b - a)); }
float lerp(float a, float b, float u) { return a + (b - a) * u; }

}  // namespace

float easeOutCubic(float u) {
  u = clamp01(u);
  const float v = 1.0f - u;
  return 1.0f - v * v * v;
}

float easeInCubic(float u) {
  u = clamp01(u);
  return u * u * u;
}

float busX(float t) {
  if (t < kEnter0) return kBusStart;
  if (t < kDepart0) return lerp(kBusStart, kBusStop, easeOutCubic(lin(t, kEnter0, kEnter1)));
  return lerp(kBusStop, kBusOut, easeInCubic(lin(t, kDepart0, kDepart1)));
}

float revealEdge(float t) { return t < kEnter0 ? 0.0f : busX(t) - kRevealGap; }

float doorOpen(float t) {
  if (t < kDoorOpen0) return 0.0f;
  if (t < kDoorClose0) return easeOutCubic(lin(t, kDoorOpen0, kDoorOpen1));
  return 1.0f - easeOutCubic(lin(t, kDoorClose0, kDoorClose1));
}

Frame frameAt(float t) {
  if (t > kEnd) t = kEnd;
  Frame f;
  f.busFront = busX(t);
  f.door = doorOpen(t);
  f.reveal = revealEdge(t);
  Lady &l = f.lady;
  if (t < kStand) {
    l.mode = LadyMode::Sit;
    // Winkt dem Bus, sobald er in Sicht ist
    l.waving = f.busFront > 450.0f && t < kStand - 0.1f;
    l.wave = l.waving ? sinf(t * 14.0f) * 5.0f : 0.0f;
  } else if (t < kWalk0) {
    l.mode = LadyMode::Stand;
    const float hop = sinf(lin(t, kStand, kWalk0) * 3.14159265f) * 3.0f;
    l.yHip = 313.0f - 18.0f - hop;
  } else if (t > kWalk1 + 0.05f) {
    l.mode = LadyMode::Hidden;  // eingestiegen
  } else {
    l.mode = LadyMode::Walk;
    const float u = lin(t, kWalk0, kWalk1);
    const float e = u * u * (3.0f - 2.0f * u);
    l.x = lerp(660.0f, 646.0f, e);
    l.scale = lerp(1.0f, 0.74f, e);
    l.legPhase = t * 11.0f;
    l.yHip = lerp(313.0f, 264.0f, e) - 18.0f * l.scale - fabsf(sinf(l.legPhase)) * 1.5f;
  }
  return f;
}

bool shouldPlay(bool enabled, bool coldStart, bool setupScreen, uint8_t backlight) {
  return enabled && coldStart && !setupScreen && backlight > 0;
}

bool finished(float t, bool boardReady) { return t >= kEnd || (boardReady && t >= kDepart1); }

}  // namespace boot
}  // namespace abf
