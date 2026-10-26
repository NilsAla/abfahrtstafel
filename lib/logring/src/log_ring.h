// Zeilenbasierter Ringpuffer für /api/log. Älteste Zeilen fallen raus, wenn der Platz nicht reicht.
// Nicht threadsicher – Aufrufer sperrt (siehe src/log/).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace abf {

class LogRing {
 public:
  // buf muss so lange leben wie der LogRing. Kapazität in Bytes inkl. Zeilenumbrüchen.
  LogRing(char *buf, size_t capacity);

  // Hängt eine Zeile an (ohne abschließendes '\n'). Zu lange Zeilen werden gekürzt,
  // eingebettete Zeilenumbrüche durch Leerzeichen ersetzt.
  void append(const char *line, size_t len);

  struct ReadResult {
    std::string text;  // Zeilen mit '\n' getrennt
    uint32_t last;     // Sequenznummer der letzten gelieferten Zeile (= since, wenn nichts Neues)
    bool gap;          // Zeilen zwischen since und der ersten gelieferten sind bereits verloren
  };

  // Alle Zeilen mit Sequenznummer > since. Sequenznummern beginnen bei 1.
  ReadResult readSince(uint32_t since) const;

  uint32_t firstSeq() const { return first_; }     // älteste vorhandene Zeile
  uint32_t lastSeq() const { return next_ - 1; }   // 0 = noch nichts geloggt
  size_t used() const { return size_; }

 private:
  void dropOldest();
  char at(size_t offset) const { return buf_[(head_ + offset) % cap_]; }

  char *buf_;
  size_t cap_;
  size_t head_ = 0;  // Index der ältesten Zeile
  size_t size_ = 0;  // belegte Bytes
  uint32_t first_ = 1;
  uint32_t next_ = 1;
};

}  // namespace abf
