#include "log_ring.h"

namespace abf {

LogRing::LogRing(char *buf, size_t capacity) : buf_(buf), cap_(capacity) {}

void LogRing::append(const char *line, size_t len) {
  if (cap_ < 2) return;
  if (len > cap_ - 1) len = cap_ - 1;
  while (size_ + len + 1 > cap_) dropOldest();

  size_t pos = (head_ + size_) % cap_;
  for (size_t i = 0; i < len; ++i) {
    const char c = line[i];
    buf_[pos] = (c == '\n' || c == '\r') ? ' ' : c;
    pos = (pos + 1) % cap_;
  }
  buf_[pos] = '\n';
  size_ += len + 1;
  ++next_;
}

void LogRing::dropOldest() {
  size_t n = 0;
  while (n < size_ && at(n) != '\n') ++n;
  n = n < size_ ? n + 1 : size_;
  head_ = (head_ + n) % cap_;
  size_ -= n;
  ++first_;
}

LogRing::ReadResult LogRing::readSince(uint32_t since) const {
  ReadResult r{std::string(), since, false};
  if (since >= lastSeq()) {
    // Auch bei since > lastSeq (z. B. Gerät neu gestartet) nichts liefern; UI setzt dann zurück.
    if (since > lastSeq()) r.last = lastSeq();
    return r;
  }
  r.gap = since + 1 < first_;

  uint32_t seq = first_;
  size_t offset = 0;
  // Bereits bekannte Zeilen überspringen
  while (seq <= since && offset < size_) {
    while (at(offset) != '\n') ++offset;
    ++offset;
    ++seq;
  }
  r.text.reserve(size_ - offset);
  for (; offset < size_; ++offset) r.text.push_back(at(offset));
  r.last = lastSeq();
  return r;
}

}  // namespace abf
