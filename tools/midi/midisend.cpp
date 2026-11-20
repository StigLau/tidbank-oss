// Virtual MIDI device "TidBank Test" (CoreMIDI source) for testing without a hardware controller.
//   tidbank_midisend sweep <cc> [secs] [ch] sends a 0..127..0 sine sweep on <cc>: in Live, enable "TidBank Test" as a
//                                          Remote input, Cmd-M, click a TidBank parameter, run the sweep (d20: host maps)
//   tidbank_midisend send <cc> <value> [ch] sends one CC after a 2 s wait (time to arm the host's MIDI map), then holds the port 2 s
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>

using juce::Thread;

static std::unique_ptr<juce::MidiOutput> openVirtual() {
    auto out = juce::MidiOutput::createNewDevice("TidBank Test");
    if (out == nullptr) std::fprintf(stderr, "could not create virtual MIDI device\n");
    return out;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String cmd = argc > 1 ? argv[1] : "";
    if (cmd == "sweep" && argc > 2) {
        const int cc = std::atoi(argv[2]); const double secs = argc > 3 ? std::atof(argv[3]) : 30.0;
        const int ch = argc > 4 ? std::atoi(argv[4]) : 1;
        auto out = openVirtual(); if (!out) return 1;
        std::printf("sweeping CC %d on ch %d for %.0f s from \"TidBank Test\"\n", cc, ch, secs); std::fflush(stdout);
        for (double t = 0.0; t < secs; t += 0.02) {
            const int v = (int) std::lround(63.5 - 63.5 * std::cos(2.0 * juce::MathConstants<double>::pi * t / 4.0));
            out->sendMessageNow(juce::MidiMessage::controllerEvent(ch, cc, v));
            Thread::sleep(20);
        }
        return 0;
    }
    if (cmd == "send" && argc > 3) {
        const int ch = argc > 4 ? std::atoi(argv[4]) : 1;
        auto out = openVirtual(); if (!out) return 1;
        Thread::sleep(2000);
        out->sendMessageNow(juce::MidiMessage::controllerEvent(ch, std::atoi(argv[2]), std::atoi(argv[3])));
        Thread::sleep(2000);
        return 0;
    }
    std::fprintf(stderr, "usage: %s sweep <cc> [secs] [ch] | send <cc> <value> [ch]\n", argv[0]);
    return 2;
}
