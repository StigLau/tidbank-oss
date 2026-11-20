#include "PluginEditor.h"
#include "BuildInfo.h"

// Layout: a header with the Engine switch, then sections on a grid of 6 columns x 3 rows of cells. The Tape and BBD sections
// share the engine area (left 4 columns, rows 2-3); the window size is the same for both faces.
namespace {
using P = TidBankProcessor;
using E = TidBankEditor;
constexpr int kCols = 6, kRows = 3, kCellW = 150, kCellH = 104, kTitleH = 22, kGap = 8, kHeaderH = 44, kStripH = 34;
constexpr int kRowH = kCellH + kTitleH + kGap;
#ifdef TIDBANK_DEBUG_PROBE
constexpr int kProbeH = 24;
#else
constexpr int kProbeH = 0;
#endif

// THE table: which section each parameter is in, in host (enum) order. Order inside a section = host order.
constexpr E::Section kSectionOf[] = {
    E::Performance,   // Intensity
    E::TimeSync,      // Division (sync on; shares its slot with Time)
    E::Tape,          // Mode
    E::TimeSync,      // Nudge
    E::Tape,          // Inertia
    E::LoopFilters,   // FbLpf
    E::LoopFilters,   // FbHpf
    E::Performance,   // Echo
    E::TimeSync,      // TimeMs (sync off; drawn in Division's slot)
    E::Tape,          // Drive (record drive)
    E::Tape,          // Spacing (head preset)
    E::Tape,          // HeadTarget
    E::TimeSync,      // Sync
    E::OutputSafety,  // Dry
    E::OutputSafety,  // Output
    E::OutputSafety,  // Panic
    E::Header,        // Engine
    E::Bbd,           // BbdProfile
    E::Bbd,           // BbdBright
    E::Bbd,           // BbdSpacing
    E::Bbd,           // PingPong
    E::Bbd,           // BbdDroop
};
static_assert(sizeof(kSectionOf) / sizeof(kSectionOf[0]) == P::NumParams, "every parameter needs a section in kSectionOf");

// Place on the grid in cells (col, row, cols, rows); perRow = controls per row inside the section (cell width = width / perRow).
struct SectionDef { const char* title; E::Face face; int col, row, cols, rows, perRow; };
constexpr SectionDef kSections[E::NumSections] = {
    { "",                  E::Both,     0, 0, 0, 0, 1 },  // Header: own strip above the grid
    { "Performance",       E::Both,     0, 0, 2, 1, 2 },
    { "Time and sync",     E::Both,     2, 0, 4, 1, 3 },  // Division-or-Time, Nudge, Sync
    { "Tape",              E::TapeFace, 0, 1, 3, 2, 3 },
    { "Loop filters",      E::TapeFace, 3, 1, 1, 2, 1 },
    { "BBD",               E::BbdFace,  0, 1, 4, 1, 5 },
    { "Output and safety", E::Both,     4, 1, 2, 2, 2 },  // Dry, Output, Panic
};
}

TidBankEditor::Section TidBankEditor::sectionOf(int p) { return kSectionOf[p]; }
TidBankEditor::Face TidBankEditor::faceOf(Section s) { return kSections[s].face; }
const char* TidBankEditor::title(Section s) { return kSections[s].title; }
bool TidBankEditor::onFace(int p, bool bbd) {
    const Face f = faceOf(sectionOf(p));
    return f == Both || f == (bbd ? BbdFace : TapeFace);
}
// Division and Time share one slot: Division with Sync on, Time with Sync off (2026-11-17). Both stay separate host
// parameters (d20); only this window swaps them.
bool TidBankEditor::shown(int p, bool bbd, bool sync) {
    if (p == P::Division) return sync;
    if (p == P::TimeMs) return !sync;
    return onFace(p, bbd);
}

TidBankEditor::TidBankEditor(TidBankProcessor& p) : AudioProcessorEditor(p), proc(p) {
    using APVTS = juce::AudioProcessorValueTreeState;
    for (int i = 0; i < TidBankProcessor::NumParams; ++i) {
        auto& c = cells[(size_t) i];
        auto& prm = proc.param(i);
        const juce::String id = TidBankProcessor::paramId(i);
        c.label.setText(prm.getName(32), juce::dontSendNotification);
        c.label.setJustificationType(i == P::Engine ? juce::Justification::centredRight : juce::Justification::centred);
        addAndMakeVisible(c.label);
        if (i == TidBankProcessor::Panic) {
            auto b = std::make_unique<juce::TextButton>("PANIC");
            b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb3261e));
            b->setTooltip("Fade out and erase all echoes (dry keeps playing)");
            b->onClick = [this] { proc.panicRequest.store(true); };
            c.control = std::move(b);
        } else if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(&prm)) {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList(ch->choices, 1);
            c.cAtt = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, id, *box);
            c.control = std::move(box);
        } else if (dynamic_cast<juce::AudioParameterBool*>(&prm) != nullptr) {
            auto b = std::make_unique<juce::ToggleButton>("on");
            c.bAtt = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, id, *b);
            c.control = std::move(b);
        } else {
            auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 18);
            c.sAtt = std::make_unique<APVTS::SliderAttachment>(proc.apvts, id, *s);
            c.control = std::move(s);
        }
        addAndMakeVisible(*c.control);
    }
    addAndMakeVisible(status);
#ifdef TIDBANK_DEBUG_PROBE
    probeStrip.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    probeStrip.setMinimumHorizontalScale(0.8f);
    probeStrip.setTooltip("Debug probe: measured echo delay per head (ms) and its difference from the engine's own value (samples), "
                          "CPU per block. Updates once per probe interval. Full log: ~/Library/Logs/TidBank/probe.log");
    addAndMakeVisible(probeStrip);
#endif
    setSize(kCols * kCellW, kHeaderH + kRows * kRowH + kStripH + kProbeH);  // fixed: the same for both faces
    showFace();
    startTimerHz(10);
}

void TidBankEditor::showFace() {
    const bool bbd = proc.param(TidBankProcessor::Engine).getValue() > 0.5f;
    const bool sync = proc.param(TidBankProcessor::Sync).getValue() > 0.5f;
    face = bbd ? 1 : 0; syncShown = sync ? 1 : 0;
    for (int i = 0; i < TidBankProcessor::NumParams; ++i) {
        const bool show = shown(i, bbd, sync);
        cells[(size_t) i].label.setVisible(show); cells[(size_t) i].control->setVisible(show);
    }
    repaint();
}

void TidBankEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1d1f22));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("TidBank", 14, 0, 200, kHeaderH, juce::Justification::centredLeft);
    const bool bbd = face == 1;
    for (int s = Performance; s < NumSections; ++s) {
        const Face f = kSections[s].face;
        if (f != Both && f != (bbd ? BbdFace : TapeFace)) continue;
        const auto r = sectionBounds[(size_t) s].toFloat().reduced(4.0f, 2.0f);
        g.setColour(juce::Colour(0xff26292d)); g.fillRoundedRectangle(r, 6.0f);
        g.setColour(juce::Colour(0xff3a3d42)); g.drawRoundedRectangle(r, 6.0f, 1.0f);
        g.setColour(juce::Colour(0xff9aa3ad));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String(kSections[s].title).toUpperCase(), r.toNearestInt().reduced(10, 0).removeFromTop(kTitleH),
                   juce::Justification::centredLeft);
    }
}

void TidBankEditor::resized() {
    // Section boxes, in pixels. The header is its own strip; grid sections sit below it.
    for (int s = Performance; s < NumSections; ++s) {
        const auto& d = kSections[s];
        sectionBounds[(size_t) s] = { d.col * kCellW, kHeaderH + d.row * kRowH, d.cols * kCellW, d.rows * kRowH - kGap };
    }
    auto header = juce::Rectangle<int>(0, 0, getWidth(), kHeaderH).reduced(8, 8);
    cells[P::Engine].control->setBounds(header.removeFromRight(140));
    cells[P::Engine].label.setBounds(header.removeFromRight(80));

    // Each section flows its controls left to right, top to bottom, in host order.
    std::array<int, NumSections> next {};
    int divisionSlot = -1;
    auto place = [&](Section s, int k, juce::Label& label, juce::Component& control, bool slider, int controlH) {
        const auto& d = kSections[s];
        auto r = sectionBounds[(size_t) s].withTrimmedTop(kTitleH);
        const int cw = r.getWidth() / d.perRow;
        r = { r.getX() + (k % d.perRow) * cw, r.getY() + (k / d.perRow) * kCellH, cw, kCellH };
        r.reduce(6, 4);
        label.setBounds(r.removeFromTop(18));
        control.setBounds(slider ? r : r.withSizeKeepingCentre(std::min(r.getWidth(), kCellW - 12), controlH));
    };
    for (int i = 0; i < TidBankProcessor::NumParams; ++i) {
        const Section s = sectionOf(i);
        if (s == Header) continue;
        int k = next[(size_t) s]++;
        if (i == P::Division) divisionSlot = k;
        if (i == P::TimeMs) { k = divisionSlot; --next[(size_t) s]; }  // shares Division's slot (Division comes first in host order)
        auto& c = cells[(size_t) i];
        place(s, k, c.label, *c.control, dynamic_cast<juce::Slider*>(c.control.get()) != nullptr, i == P::Panic ? 48 : 24);
    }

    auto bottom = getLocalBounds().removeFromBottom(kStripH + kProbeH);
#ifdef TIDBANK_DEBUG_PROBE
    probeStrip.setBounds(bottom.removeFromBottom(kProbeH).reduced(8, 2));
#endif
    status.setBounds(bottom.reduced(8, 6));
}

void TidBankEditor::refreshStatus() {
    if ((proc.param(TidBankProcessor::Engine).getValue() > 0.5f ? 1 : 0) != face
        || (proc.param(TidBankProcessor::Sync).getValue() > 0.5f ? 1 : 0) != syncShown) showFace();  // Engine or Sync changed
    juce::String s = (proc.hostTempo.load() ? "Host tempo " : "No host tempo, fixed ") + juce::String(proc.bpmInUse.load(), 1) + " BPM";
    const bool bbdOn = proc.param(TidBankProcessor::Engine).getValue() > 0.5f;
    if (bbdOn) s << "   |   BBD " << proc.param(TidBankProcessor::BbdProfile).getCurrentValueAsText();
    if (!bbdOn && proc.clamped.load()) s << "   |   sync out of range: head 1 clamped to 40 ms..2 s";
    if (bbdOn && proc.bbdClamped.load()) s << "   |   delay outside this profile's range: clamped";
    s << "   |   build " << TIDBANK_BUILD;
    status.setText(s, juce::dontSendNotification);
#ifdef TIDBANK_DEBUG_PROBE
    refreshProbe();
#endif
}

#ifdef TIDBANK_DEBUG_PROBE
// One line, e.g. "probe  h1 129.15  h2 -  h3 -  BBD -  ms  |diff| 0.000 smp  CPU 5.3% peak 6.3%  drops 0".
// A slot not updated for 2 intervals + 1 s shows "-" (head unused, or the other engine is active).
void TidBankEditor::refreshProbe() {
    const auto& lv = proc.probeLive();
    if (!lv.on.load()) {
        probeStrip.setColour(juce::Label::textColourId, juce::Colours::grey);
        probeStrip.setText("probe off: set TIDBANK_PROBE or probe.cfg, then reload the plugin", juce::dontSendNotification);
        return;
    }
    const uint32_t now = juce::Time::getMillisecondCounter();
    const auto staleMs = (uint32_t) (2000.0 * lv.interval.load() + 1000.0);
    const auto fresh = [&](uint32_t seen) { return seen != 0 && now - seen <= staleMs; };
    const double msPerSmp = 1000.0 / lv.sampleRate.load();
    juce::String s = "probe ";
    double worst = -1.0;
    static const char* const names[4] = { "h1", "h2", "h3", "BBD" };
    for (int k = 0; k < 4; ++k) {
        s << " " << names[k] << " ";
        if (!fresh(lv.seenMs[k].load())) { s << "-"; continue; }
        const double m = lv.measured[k].load();
        s << juce::String(m * msPerSmp, 2);
        worst = std::max(worst, std::abs(m - lv.computed[k].load()));
    }
    s << " ms  |diff| " << (worst < 0.0 ? juce::String("-") : juce::String(worst, 3) + " smp");
    if (fresh(lv.cpuSeenMs.load()))
        s << "  CPU " << juce::String(lv.cpuAvg.load(), 1) << "% peak " << juce::String(lv.cpuPeak.load(), 1) << "%  drops " << juce::String((juce::int64) lv.drops.load());
    else s << "  CPU -";
    const bool bad = worst >= 1.0 || lv.drops.load() > 0;
    probeStrip.setColour(juce::Label::textColourId, bad ? juce::Colour(0xffff6b5e) : juce::Colour(0xff8fd18a));
    probeStrip.setText(s, juce::dontSendNotification);
}
#endif
