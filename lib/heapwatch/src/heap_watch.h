// Überwachung des internen Heaps: Warnung bei Knappheit, kontrollierter Neustart, wenn der Speicher
// dauerhaft kritisch ist (sonst scheitern TLS-Abrufe still und die Tafel zeigt nur noch alte Daten).
// Zeitbasis millis(); Überlauf ist durch vorzeichenlose Differenzen abgedeckt.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace abf {

enum class HeapLevel { Ok, Low, Critical };

const char *heapLevelName(HeapLevel l);  // Schlüssel für /api/status

class HeapWatch {
 public:
  struct Limits {
    size_t lowFree = 40 * 1024;          // darunter: Warnung
    size_t lowBlock = 24 * 1024;
    size_t criticalFree = 20 * 1024;     // darunter: kritisch
    size_t criticalBlock = 10 * 1024;    // TLS braucht zusammenhängende Puffer
    size_t hysteresis = 8 * 1024;        // erst so viel über der Schwelle zurück auf den besseren Zustand
    uint32_t restartAfterMs = 10 * 60 * 1000;  // so lange am Stück kritisch → Neustart
  };

  enum class Event { None, BecameLow, BecameCritical, Recovered, RestartDue };

  HeapWatch() : HeapWatch(Limits()) {}
  explicit HeapWatch(const Limits &limits) : lim_(limits) {}

  // Regelmäßig aufrufen (z. B. alle 5 s). RestartDue kommt einmal pro kritischer Phase.
  Event update(size_t freeBytes, size_t largestBlock, uint32_t now);

  HeapLevel level() const { return level_; }

 private:
  HeapLevel classify(size_t freeBytes, size_t largestBlock) const;

  Limits lim_;
  HeapLevel level_ = HeapLevel::Ok;
  uint32_t criticalSince_ = 0;
  bool restartFired_ = false;
};

}  // namespace abf
