// Überwachung des internen Heaps (Logik in lib/heapwatch): Warnung im Log, Neustart bei Dauer-Knappheit.
#pragma once

#include <heap_watch.h>
#include <stddef.h>

namespace heapmon {

void loop();  // im Loop aufrufen, prüft alle 5 s
abf::HeapLevel level();
size_t largestBlock();  // größter freier Block im internen RAM

}  // namespace heapmon
