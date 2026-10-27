# UI

Display-Oberfläche mit LVGL 9.3, Querformat 820 × 320, board-unabhängig.

- `lib/dispmodel/` entscheidet, **was** angezeigt wird (`abf::buildView`): Bildschirm, Texte, Farben, Helligkeit.
  Reine Logik, Tests in `test/test_dispmodel/`.
- `ui.cpp` zeichnet eine `abf::View`. Bei jeder Änderung wird der Bildschirm komplett neu aufgebaut
  (höchstens einmal pro Sekunde, meist einmal pro Minute).
- `fonts/` wird mit `scripts/gen_fonts.py` erzeugt (Barlow Semi Condensed, OFL; Symbole aus Font Awesome 5, CC BY 4.0).
- `../display/display_task.cpp` verbindet beides mit Firmware und Board-HAL.
- `../sim/` rendert alle Zustände headless: `pio run -e sim && python3 scripts/render_screens.py` → `docs/screens/`.

Farbstile: **Dunkel** (Nachtblau) und **Nacht** (Bernstein auf Schwarz, kaum Blauanteil). Im Nachtzeitfenster gilt
immer der Nacht-Stil mit der Nacht-Helligkeit; 0 % schaltet das Display aus.
