#pragma once
// Clean POD/opaque API to the ForgeView scope firmware engine.
// This is the ONLY header the Rack-facing module includes — it deliberately
// exposes no Arduino/firmware types so it can coexist with rack.hpp.
#include <cstdint>
#include <string>

namespace fvengine {

struct Engine; // opaque

Engine *createEngine();
void destroyEngine(Engine *);

// Feed one input sample and advance engine time by dt seconds.
//   cv1Volts / cv2Volts : the two CV inputs, already mapped to 0..5 V.
//   clkHigh             : external clock/trigger input level (shot-mode trigger).
void feedSample(Engine *, float dt, float cv1Volts, float cv2Volts, bool clkHigh);

// Copy the 128x64 monochrome framebuffer (1bpp, row-major, MSB-first = 1024 bytes).
void getFramebuffer(Engine *, uint8_t out[1024]);

// Encoder rotation in detents (+clockwise / -counter-clockwise).
void encoderTurn(Engine *, int detents);
// Encoder push-button level; the engine detects the press/release edge.
void encoderButton(Engine *, bool pressed);

// ── Curated state bridge (Rack context menu + patch persistence) ─────────────
int modeCount();
std::string modeName(int index); // 0-based
int mode(Engine *);              // 1..4 (firmware convention)
void setMode(Engine *, int mode);

int param1(Engine *);
void setParam1(Engine *, int v);
int param1Max(Engine *); // current-mode max for param1 (extended timebase range)
int param2(Engine *);
void setParam2(Engine *, int v);

float verticalScale(Engine *);
void setVerticalScale(Engine *, float v);

} // namespace fvengine
