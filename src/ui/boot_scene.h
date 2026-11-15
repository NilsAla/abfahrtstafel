// Bootanimation auf dem Display (LVGL): Szene aus Flächen, Bewegung aus abf::boot::frameAt().
// Nur aus dem LVGL-Thread aufrufen. Nach bootEnd() zeichnet ui::show() wieder normal.
#pragma once

#include <boot_anim.h>
#include <display_model.h>

namespace ui {

void bootBegin(abf::DisplayTheme theme);  // baut die Szene auf
void bootFrame(const abf::boot::Frame &f);  // verschiebt Bus, Dame, Tür und Wortmarke
void bootEnd();                            // räumt auf, nächstes ui::show() zeichnet neu

}  // namespace ui
