#pragma once
namespace tidbank {
// v0.1 tape-engine parameters, plain values (no JUCE). Defaults follow docs/design.md.
struct TapeParams {
    int   sync = 1;            // 1 = division from bpm, 0 = time_ms
    float bpm = 120.0f;
    float division = 0.5f;     // quarter notes (0.5 = 1/8)
    int   head_target = 1;     // head (1..3) that division/time_ms sets
    int   spacing = 0;         // 0 even 1:2:3, 1 vintage (provisional)
    float nudge_cents = 0.0f;  // detune of tempo, +-400
    float time_ms = 150.0f;    // target head time when sync == 0
    float inertia_ms = 300.0f; // time to 99 % of a speed step
    int   mode = 4;            // 1..12
    float intensity = 0.45f;   // feedback loop gain, 0..1.4 (d36 raised the shared knob for the BBD)
    float fb_hpf = 80.0f;      // Hz
    float fb_lpf = 6000.0f;    // Hz
    float drive = 0.0f;        // dB, -12..+18
    int   head = 0;            // 0 = mode decides, 1..3 solo one head (tests)
};
}
