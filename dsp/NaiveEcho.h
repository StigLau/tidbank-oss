#pragma once
#include "DelayLine.h"
namespace tidbank {
// Reference engine: plain feedback delay. Exists to prove the ears harness; replaced by TapeEngine/BbdEngine.
struct NaiveEcho {
    DelayLine dl; float delaySamples = 4800.0f, feedback = 0.5f, dry = 1.0f, wet = 1.0f;
    void prepare(double sr) { dl.prepare((size_t) (sr * 4.0)); }
    float process(float x) {
        const float d = dl.read(delaySamples - 1.0f); // read-before-push: loop delay is read()+1
        dl.push(x + d * feedback);
        return x * dry + d * wet;
    }
};
}
