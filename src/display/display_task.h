// Display-Task: sammelt den Gerätezustand, baut daraus die Bildbeschreibung (lib/dispmodel)
// und lässt sie von der UI-Schicht (src/ui) mit LVGL zeichnen. Ausgabe über das Board-HAL.
#pragma once

namespace display {

// Startet die Task, wenn das Board ein Display meldet. Ohne Display (DevKit, M0.5 noch offen)
// passiert nichts weiter.
void begin();

bool active();

}  // namespace display
