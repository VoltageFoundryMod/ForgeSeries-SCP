#pragma once
//  ForgeView — scope engine (platform-independent).
//
//  This header holds ALL of the scope's mutable state (file-scope globals) plus
//  the acquisition + render + encoder logic.  It is included unchanged by:
//    • src/main.cpp                — the RP2040 firmware
//    • vcv-plugin/.../fw_engine.cpp — the VCV Rack engine (via the Arduino shim)
//
//  Acquisition is PUSH-based: the host calls ScopeFeedSample() once per input
//  sample (per ADC read on hardware, per audio sample in Rack) and the engine
//  decimates / captures internally.  This is the key adaptation over the original
//  SAMD21 firmware, which pulled samples in a blocking analogRead()+delay loop —
//  a model that cannot work under Rack's one-sample-at-a-time process().  The
//  timebase knob becomes a sample-decimation factor instead of a busy-wait delay.
//
//  Every mutable global below is mirrored in the VCV engine_state.def registry so
//  multiple Rack instances can each own an independent copy (context-swap).

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

// fixfft.cpp is #included (not compiled separately) so this single source builds
// as part of whichever TU pulls in scope.hpp — the RP2040 firmware or the VCV
// engine — mirroring how ClockForge vendors quantizer.cpp / scales.cpp.
#include "fixfft.cpp"

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 64
#endif

// ── Scope modes ──────────────────────────────────────────────────────────────
enum ScopeMode {
    MODE_LFO = 1,      // dual-trace slow scope of CV1 + CV2
    MODE_WAVE = 2,     // single-channel oscilloscope (CV1)
    MODE_SHOT = 3,     // triggered single-shot capture (on CLK rising edge)
    MODE_SPECTRUM = 4, // 128-point FFT spectrum of CV1
};
#define SCOPE_MODE_COUNT 4

// ── Mutable state (registered in engine_state.def for the VCV context-swap) ──
int menuMode = MODE_LFO;    // current scope mode (1..4)
int oldMenuMode = MODE_LFO; // previous mode (detects mode change)
int param = 1;              // overlay cursor row (1..4)
int param_select = 0;       // 0 = navigating rows; 1..4 = editing that row
int param1 = 2;             // per-mode parameter A (timebase / hi-freq / …)
int param2 = 1;             // per-mode parameter B (offset / refresh / filter)
float scale = 1.0f;         // vertical gain multiplier

bool switchState = true; // encoder push-button level (1 = released)
bool oldSwitchState = true;

unsigned long hideTimer = 0; // millis() of last interaction (overlay visibility)
bool overlayOn = true;       // draw the parameter overlay box

bool trig = false;     // latched external-trigger level (shot mode)
bool old_trig = false; // previous trigger level (rising-edge detect)
int rfrs = 0;          // wave-mode refresh divider counter

int _decimCount = 0;         // samples seen since the last stored trace sample
bool _shotCapturing = false; // shot mode: capture in progress
int _shotIdx = 0;            // shot mode: next capture index
int _capIdx = 0;             // spectrum mode: next FFT capture index

// Trace buffers: index 0 = oldest, 127 = newest (shift register).
// Values are signed "screen units": ADC midscale (2048) -> 0, ±2048 -> ±64.
char cv0[SCREEN_WIDTH] = {0}; // channel 1 trace
char cv1[SCREEN_WIDTH] = {0}; // channel 2 trace (LFO mode)

char fftCap[SCREEN_WIDTH] = {0};            // spectrum: rolling capture buffer
unsigned char spec[SCREEN_WIDTH / 2] = {0}; // spectrum: last computed magnitudes (64 bins)

// ── Small helpers ────────────────────────────────────────────────────────────
// Map a 0..4095 ADC reading to a signed screen unit (-64..+63 at full scale).
static inline char ScopeMapSample(int adc) {
    int v = (adc - 2048) >> 5; // ±2048 -> ±64
    return (char)constrain(v, -120, 120);
}

// Timebase decimation: knob 1..8 -> keep 1 sample every 1,2,4,…128 (exp/div).
static inline int ScopeTimebaseDecim(int p) {
    p = constrain(p, 1, 8);
    return 1 << (p - 1);
}

// Vertical placement: center + scaled sample (positive sample = up on screen).
static inline int ScopeY(int center, char s, float sc) {
    int y = center - (int)((float)s * sc * 0.5f);
    return constrain(y, 0, SCREEN_HEIGHT - 1);
}

static void DrawHDashedLine(Adafruit_SSD1306 &display, int x0, int y0, int width, int color) {
    for (int i = 0; i < width; i += 4)
        display.drawPixel(x0 + i, y0, color);
}
static void DrawVDashedLine(Adafruit_SSD1306 &display, int x0, int y0, int height, int color) {
    for (int i = 0; i < height; i += 4)
        display.drawPixel(x0, y0 + i, color);
}

// ── Mode defaults ────────────────────────────────────────────────────────────
// Applied when the mode changes; mirrors the original firmware's per-mode seeds.
static void ScopeApplyModeDefaults() {
    switch (menuMode) {
    case MODE_LFO:
        param1 = 2; // timebase
        param2 = 4; // vertical offset (mid)
        break;
    case MODE_WAVE:
        param1 = 3; // timebase
        param2 = 3; // refresh divider
        break;
    case MODE_SHOT:
        param1 = 2; // timebase
        param2 = 1;
        _shotCapturing = false;
        break;
    case MODE_SPECTRUM:
        param1 = 2; // high-frequency emphasis
        param2 = 3; // noise floor
        _capIdx = 0;
        break;
    }
    _decimCount = 0;
}

void ScopeInit() {
    menuMode = MODE_LFO;
    oldMenuMode = MODE_LFO;
    param = 1;
    param_select = 0;
    scale = 1.0f;
    switchState = oldSwitchState = true;
    hideTimer = millis();
    overlayOn = true;
    ScopeApplyModeDefaults();
}

// ── Acquisition ──────────────────────────────────────────────────────────────
// Called once per input sample. ch1/ch2 are 0..4095 ADC; clkHigh is the CLK/
// trigger input level. Handles per-mode decimation and capture.
void ScopeFeedSample(int ch1adc, int ch2adc, bool clkHigh) {
    // Trigger edge detection is never decimated (shot mode needs every edge).
    old_trig = trig;
    trig = clkHigh;

    if (menuMode == MODE_SPECTRUM) {
        // Sample as fast as possible for maximum bandwidth; run the FFT when the
        // 128-sample window is full.
        fftCap[_capIdx++] = (char)constrain((ch1adc - 2048) >> 4, -127, 127);
        if (_capIdx >= SCREEN_WIDTH) {
            _capIdx = 0;
            char data[SCREEN_WIDTH];
            char im[SCREEN_WIDTH];
            for (int i = 0; i < SCREEN_WIDTH; i++) {
                data[i] = fftCap[i];
                im[i] = 0;
            }
            fix_fft(data, im, 7, 0); // 2^7 = 128-point FFT
            for (int i = 0; i < SCREEN_WIDTH / 2; i++) {
                int level = (int)sqrtf((float)(data[i] * data[i] + im[i] * im[i]));
                spec[i] = (unsigned char)constrain(level, 0, SCREEN_HEIGHT);
            }
        }
        return;
    }

    if (menuMode == MODE_SHOT) {
        // Start a capture on a rising trigger edge; fill the buffer once, freeze.
        if (!_shotCapturing && trig && !old_trig) {
            _shotCapturing = true;
            _shotIdx = 0;
            _decimCount = 0;
        }
        if (_shotCapturing) {
            if (++_decimCount < ScopeTimebaseDecim(param1))
                return;
            _decimCount = 0;
            cv0[_shotIdx++] = ScopeMapSample(ch1adc);
            if (_shotIdx >= SCREEN_WIDTH)
                _shotCapturing = false; // frozen until next trigger
        }
        return;
    }

    // LFO + WAVE: rolling shift-register scope at the selected timebase.
    if (++_decimCount < ScopeTimebaseDecim(param1))
        return;
    _decimCount = 0;
    char s0 = ScopeMapSample(ch1adc);
    char s1 = ScopeMapSample(ch2adc);
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
        cv0[i] = cv0[i + 1];
        cv1[i] = cv1[i + 1];
    }
    cv0[SCREEN_WIDTH - 1] = s0;
    cv1[SCREEN_WIDTH - 1] = s1;
}

// ── Rendering ────────────────────────────────────────────────────────────────
static void ScopeDrawGrid(Adafruit_SSD1306 &display) {
    for (int x = 0; x < SCREEN_WIDTH; x += 16)
        DrawVDashedLine(display, x, 0, SCREEN_HEIGHT, WHITE);
    DrawVDashedLine(display, SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT, WHITE);
    for (int y = 0; y < SCREEN_HEIGHT; y += 16)
        DrawHDashedLine(display, 0, y, SCREEN_WIDTH, WHITE);
    DrawHDashedLine(display, 0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, WHITE);
}

static void ScopeDrawTrace(Adafruit_SSD1306 &display, const char *buf, int center, float sc) {
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
        display.drawLine(i, ScopeY(center, buf[i], sc),
                         i + 1, ScopeY(center, buf[i + 1], sc), WHITE);
    }
}

static void ScopeDrawOverlay(Adafruit_SSD1306 &display) {
    // Overlay auto-hides ~5s after the last interaction.
    overlayOn = (millis() - hideTimer) < 5000;
    if (!overlayOn)
        return;

    display.fillRect(30, 8, 68, 44, BLACK);
    display.drawRect(30, 8, 68, 44, WHITE);

    const char *labels[4] = {"Mode", "Par1", "Par2", "Vert"};
    switch (menuMode) {
    case MODE_LFO:
        labels[1] = "Hori";
        labels[2] = "Offs";
        labels[3] = "Vert";
        break;
    case MODE_WAVE:
        labels[1] = "Hori";
        labels[2] = "Refr";
        labels[3] = "Vert";
        break;
    case MODE_SHOT:
        labels[1] = "Hori";
        labels[2] = "Trig";
        labels[3] = "Vert";
        break;
    case MODE_SPECTRUM:
        labels[1] = "High";
        labels[2] = "Filt";
        labels[3] = "-";
        break;
    }

    // Cursor marker for the highlighted row.
    int cursorY = 20 + (param - 1) * 10;
    display.drawLine(34, cursorY, 64, cursorY, WHITE);

    for (int row = 0; row < 4; row++) {
        display.setTextColor(WHITE);
        if (param_select == row + 1)
            display.setTextColor(BLACK, WHITE); // inverse while editing
        display.setCursor(34, 12 + row * 10);
        display.print(labels[row]);
        display.print(": ");
        switch (row) {
        case 0:
            display.print(menuMode);
            break;
        case 1:
            display.print(param1);
            break;
        case 2:
            if (menuMode == MODE_SHOT)
                display.print(trig ? "ON" : "--");
            else
                display.print(param2);
            break;
        case 3:
            if (menuMode == MODE_SPECTRUM)
                display.print("-");
            else
                display.print(scale, 2);
            break;
        }
    }
}

// Render the current mode into the display's GFX buffer (no I2C flush here).
void ScopeRender(Adafruit_SSD1306 &display) {
    display.clearDisplay();
    display.setTextSize(1);

    if (menuMode == MODE_SPECTRUM) {
        for (int i = 0; i < SCREEN_WIDTH / 2; i++) {
            int level = spec[i] + i * (param1 - 1) / 8; // high-frequency emphasis
            if (spec[i] >= param2 && level > 0) {       // noise-floor gate
                level = constrain(level, 0, SCREEN_HEIGHT - 1);
                display.fillRect(i * 2, (SCREEN_HEIGHT - 1) - level, 2, level, WHITE);
            }
        }
        ScopeDrawOverlay(display);
        return;
    }

    if (menuMode == MODE_LFO) {
        // Two stacked traces; param2 nudges them apart/together.
        int off = (param2 - 4) * 2;
        ScopeDrawTrace(display, cv0, 16 - off, scale);
        ScopeDrawTrace(display, cv1, 48 + off, scale);
    } else {
        // WAVE + SHOT: single centred trace.
        ScopeDrawTrace(display, cv0, SCREEN_HEIGHT / 2, scale);
    }

    ScopeDrawGrid(display);
    ScopeDrawOverlay(display);
}

// ── Encoder input ────────────────────────────────────────────────────────────
// One detent of rotation (dir = +1 clockwise, -1 counter-clockwise).
void ScopeEncoderTurn(int dir) {
    hideTimer = millis();
    switch (param_select) {
    case 0: // navigate overlay rows
        param = constrain(param + dir, 1, 4);
        break;
    case 1: // mode
        menuMode = constrain(menuMode + dir, 1, SCOPE_MODE_COUNT);
        if (menuMode != oldMenuMode) {
            ScopeApplyModeDefaults();
            oldMenuMode = menuMode;
        }
        break;
    case 2: // param1
        param1 = constrain(param1 + dir, 1, 8);
        break;
    case 3: // param2
        param2 = constrain(param2 + dir, 1, 8);
        break;
    case 4: // vertical scale (multiplicative, snaps to 1.0 near unity)
        scale *= (dir > 0) ? 1.1f : (1.0f / 1.1f);
        if (scale > 0.91f && scale < 1.09f)
            scale = 1.0f;
        scale = constrain(scale, 0.05f, 20.0f);
        break;
    }
}

// Encoder push-button; edge-detected. pressed = true while held.
void ScopeEncoderButton(bool pressed) {
    oldSwitchState = switchState;
    switchState = !pressed; // switchState mirrors the (active-low) button level
    hideTimer = millis();
    // Act on the release edge (button goes pressed -> released), matching the
    // original firmware's push-to-select behaviour.
    if (oldSwitchState == false && switchState == true) {
        if (param_select == param)
            param_select = 0; // clicking the selected row deselects it
        else
            param_select = param; // otherwise start editing the cursor row
    }
}
