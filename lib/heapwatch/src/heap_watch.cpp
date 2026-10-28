#include "heap_watch.h"

namespace abf {

const char *heapLevelName(HeapLevel l) {
  switch (l) {
    case HeapLevel::Ok:       return "ok";
    case HeapLevel::Low:      return "low";
    case HeapLevel::Critical: return "critical";
  }
  return "unknown";
}

HeapLevel HeapWatch::classify(size_t freeBytes, size_t largestBlock) const {
  // Hysterese: Aus einem schlechteren Zustand geht es erst mit Abstand zur Schwelle wieder heraus
  const size_t hc = level_ == HeapLevel::Critical ? lim_.hysteresis : 0;
  if (freeBytes < lim_.criticalFree + hc || largestBlock < lim_.criticalBlock + hc) return HeapLevel::Critical;
  const size_t hl = level_ != HeapLevel::Ok ? lim_.hysteresis : 0;
  if (freeBytes < lim_.lowFree + hl || largestBlock < lim_.lowBlock + hl) return HeapLevel::Low;
  return HeapLevel::Ok;
}

HeapWatch::Event HeapWatch::update(size_t freeBytes, size_t largestBlock, uint32_t now) {
  const HeapLevel next = classify(freeBytes, largestBlock);
  const HeapLevel prev = level_;
  level_ = next;

  if (next == HeapLevel::Critical) {
    if (prev != HeapLevel::Critical) {
      criticalSince_ = now;
      restartFired_ = false;
      return Event::BecameCritical;
    }
    if (!restartFired_ && now - criticalSince_ >= lim_.restartAfterMs) {
      restartFired_ = true;
      return Event::RestartDue;
    }
    return Event::None;
  }
  if (next == HeapLevel::Low && prev == HeapLevel::Ok) return Event::BecameLow;
  if (next == HeapLevel::Ok && prev != HeapLevel::Ok) return Event::Recovered;
  return Event::None;
}

}  // namespace abf
