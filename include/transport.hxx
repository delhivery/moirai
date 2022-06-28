#ifndef TRANSPORT
#define TRANSPORT

#include <cstdint>

enum class Process : uint8_t { IN = 0, OUT = 1, XFER = 2 };

enum class Movement : uint8_t { GLO = 0, LOC = 1 };

enum class Vehicle : uint8_t { SFC = 0, AIR = 1 };

#endif
