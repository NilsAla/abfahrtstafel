// Bootanimation (docs/branding/BRANDING.md, Abschnitt 7): Zeitachse und Bewegungen als reine Logik, ohne LVGL.
// src/ui/boot_scene.cpp zeichnet daraus die Szene. Koordinaten in Display-Pixeln (820 × 320).
#pragma once

#include <stdint.h>

namespace abf {
namespace boot {

// Zeitachse der Referenz (docs/branding/animation/src/make_gif_*.py), Sekunden
constexpr float kEnter0 = 0.4f, kEnter1 = 2.7f;       // Einfahrt, ease-out cubic
constexpr float kDoorOpen0 = 2.9f, kDoorOpen1 = 3.3f;
constexpr float kStand = 3.7f;                          // Dame steht auf
constexpr float kWalk0 = 3.9f, kWalk1 = 5.2f;
constexpr float kOnceCut = 4.0f;                        // boot-once: hier anhalten …
constexpr float kOnceHold = 1.5f;                       // … und so lange auf dem Logo stehen bleiben
constexpr float kOnceEnd = kOnceCut + kOnceHold;

constexpr float kBusStart = 0.0f, kBusStop = 726.0f;    // x der Busfront
constexpr float kBusLength = 250.0f;
constexpr float kRevealGap = 12.0f;                     // Wortmarke endet 12 px vor der Busfront

enum class LadyMode : uint8_t { Sit, Stand, Walk, Hidden };

struct Lady {
  LadyMode mode = LadyMode::Sit;
  float x = 660.0f;     // Mitte
  float yHip = 296.0f;  // Hüfte
  float scale = 1.0f;
  float legPhase = 0.0f;
  float wave = 0.0f;    // Winken: Versatz der Hand in px, 0 = Arm unten
  bool waving = false;
};

struct Frame {
  float busFront = kBusStart;  // x der Busfront
  float door = 0.0f;           // 0 zu … 1 offen
  float reveal = 0.0f;         // rechte Kante der sichtbaren Wortmarke (0 = nichts)
  Lady lady;
};

float easeOutCubic(float u);
float busX(float t);        // Busfront bei Zeit t (nur Einfahrt und Halt, boot-once)
float revealEdge(float t);  // Kante der Wortmarke; ≤ 0 vor der Einfahrt
float doorOpen(float t);    // 0 … 1

// Zustand der Szene bei Zeit t (boot-once: ab kOnceCut eingefroren)
Frame frameAt(float t);

// Nur bei echtem Kaltstart (Strom, Reset-Taste, USB) und wenn eingeschaltet. Nach Werksreset, Rollback,
// OTA- oder Absturz-Neustart (Software-Reset, Watchdog, Panic) geht es direkt zur Statusanzeige; die
// Einrichtung ohne WLAN hat Vorrang; bei abgeschaltetem Display (Nachtfenster 0 %) lohnt es nicht.
bool shouldPlay(bool enabled, bool coldStart, bool setupScreen, uint8_t backlight);

// Fertig? Regulär nach kOnceEnd; sobald Abfahrten bereitstehen schon früher, aber nie vor Ende der Einfahrt.
bool finished(float t, bool boardReady);

}  // namespace boot
}  // namespace abf
