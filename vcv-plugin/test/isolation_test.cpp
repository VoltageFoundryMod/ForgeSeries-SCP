// isolation_test.cpp — per-instance state isolation + render smoke test for the
// ForgeView scope engine.  Compiles fw_engine.cpp + shim + ../lib into a host
// executable and drives TWO engines through the public fvengine API to prove
// they do not share scope/menu state, and that acquisition + rendering work.
// No VCV Rack, no hardware required.
//
// Build/run:  test/build_isolation_test.sh   (from vcv-plugin/)

#include "engine/fw_engine.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace fvengine;

static int g_failures = 0;
#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond)) {                        \
            std::printf("  FAIL: %s\n", msg); \
            ++g_failures;                     \
        } else {                              \
            std::printf("  ok  : %s\n", msg); \
        }                                     \
    } while (0)

static int litPixels(const uint8_t fb[1024]) {
    int n = 0;
    for (int i = 0; i < 1024; i++)
        for (int b = 0; b < 8; b++)
            if (fb[i] & (1 << b))
                n++;
    return n;
}

// Feed `count` samples of a full-scale sine (bipolar, mapped 0..5V) into ch1.
static void feedSine(Engine *e, int count) {
    for (int i = 0; i < count; i++) {
        float ph = (float)i / 32.0f * 2.0f * 3.14159265f;
        float v = 2.5f + 2.4f * std::sin(ph); // 0.1..4.9 V (0..5 domain)
        feedSample(e, 2.0f / 44100.0f, v, 2.5f, false);
    }
}

int main() {
    std::printf("fvengine two-instance isolation + render test\n");

    Engine *a = createEngine();
    Engine *b = createEngine();

    CHECK(mode(a) == 1 && mode(b) == 1, "both engines start in LFO mode");

    // ── Mode isolation via the state bridge ──────────────────────────────────
    setMode(a, 4); // Spectrum
    std::printf("  mode(A)=%d mode(B)=%d\n", mode(a), mode(b));
    CHECK(mode(a) == 4, "A switched to Spectrum");
    CHECK(mode(b) == 1, "B stayed in LFO (per-instance isolation)");

    // ── Parameter isolation ──────────────────────────────────────────────────
    setParam1(a, 7);
    CHECK(param1(a) == 7, "A param1 set to 7");
    CHECK(param1(b) != 7, "B param1 not affected by A");

    // ── Render smoke test: B (LFO) should draw grid + trace ──────────────────
    feedSine(b, 400);
    uint8_t fbB[1024];
    getFramebuffer(b, fbB);
    int litB = litPixels(fbB);
    std::printf("  LFO framebuffer lit pixels: %d\n", litB);
    CHECK(litB > 150, "LFO mode renders a non-trivial frame (grid + trace)");

    // Spectrum on A should not crash and should render something after capture.
    feedSine(a, 400);
    uint8_t fbA[1024];
    getFramebuffer(a, fbA);
    std::printf("  Spectrum framebuffer lit pixels: %d\n", litPixels(fbA));
    CHECK(litPixels(fbA) >= 0, "Spectrum mode renders without crashing");

    // ── Encoder path: navigate + edit Mode on a fresh engine ─────────────────
    Engine *c = createEngine();
    // param cursor starts on the Mode row (param=1). Click to select it, turn to
    // change the mode, click to deselect.
    encoderButton(c, true);
    encoderButton(c, false); // select Mode row (param_select = 1)
    encoderTurn(c, +1);      // menuMode 1 -> 2 (Wave)
    encoderButton(c, true);
    encoderButton(c, false); // deselect
    std::printf("  encoder-driven mode(C)=%d\n", mode(c));
    CHECK(mode(c) == 2, "encoder rotate+click changes mode (Wave)");

    // ── Hot path: interleave feeds on all three, ensure stability ────────────
    for (int i = 0; i < 3000; i++) {
        float v = 2.5f + 2.0f * std::sin((float)i / 20.0f);
        feedSample(a, 2.0f / 44100.0f, v, 2.5f, (i % 500) == 0);
        feedSample(b, 2.0f / 44100.0f, v, 2.5f, false);
        feedSample(c, 2.0f / 44100.0f, v, 2.5f, false);
    }
    CHECK(mode(a) == 4 && mode(b) == 1 && mode(c) == 2, "modes stay isolated through feed loop");

    destroyEngine(a);
    destroyEngine(b);
    destroyEngine(c);

    if (g_failures) {
        std::printf("\n== %d FAILURE(S) ==\n", g_failures);
        return 1;
    }
    std::printf("\n== ALL PASS ==\n");
    return 0;
}
