// Renders the plugin editor offscreen to a PNG, so an agent can look at the UI without screen-recording rights.
// Usage: tidbank_snapshot out.png [param=value ...]   plain values; choices are 0-based indices (mode=6 is mode 7, head_target=0 is head 1);
#include <JuceHeader.h>
#include "PluginEditor.h"

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::fprintf(stderr, "usage: %s out.png [id=value ...]\n", argv[0]); return 2; }
    TidBankProcessor proc;
    for (int a = 2; a < argc; ++a) {
        const juce::String kv(argv[a]);
        auto* p = proc.apvts.getParameter(kv.upToFirstOccurrenceOf("=", false, false));
        if (p == nullptr) { std::fprintf(stderr, "unknown param %s\n", argv[a]); return 2; }
        p->setValueNotifyingHost(p->convertTo0to1(kv.fromFirstOccurrenceOf("=", false, false).getFloatValue()));
    }
    proc.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi; buf.clear();
    proc.processBlock(buf, midi);  // fills the status line (tempo, clamp)
#ifdef TIDBANK_DEBUG_PROBE
    if (std::getenv("TIDBANK_PROBE")) {  // probe builds: play 3 s of clicks so the probe strip has values, and let its Timer drain
        for (int j = 0; j < 3 * 48000 / 512; ++j) {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample(c, i, j % 47 == 0 && i == 0 ? 0.5f : 0.0f);
            proc.processBlock(buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil(600);
    }
#endif
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    if (auto* te = dynamic_cast<TidBankEditor*>(ed.get())) te->refreshStatus();
    const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    out.deleteFile();
    juce::FileOutputStream os(out);
    if (!os.openedOk() || !juce::PNGImageFormat().writeImageToStream(img, os)) { std::fprintf(stderr, "write failed\n"); return 1; }
    std::printf("{\"out\": \"%s\", \"w\": %d, \"h\": %d}\n", out.getFullPathName().toRawUTF8(), ed->getWidth(), ed->getHeight());
    return 0;
}
