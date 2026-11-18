// ST Shell modulation tests. Console app; exit code = number of failed checks.

#include <st_shell/st_shell.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

//==============================================================================
// Allocation tracking (best effort: counts operator new while armed on any thread).
static std::atomic<bool> gTrackAllocs { false };
static std::atomic<int> gAllocCount { 0 };

void* operator new (std::size_t n)
{
    if (gTrackAllocs.load (std::memory_order_relaxed)) gAllocCount.fetch_add (1);
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n)
{
    if (gTrackAllocs.load (std::memory_order_relaxed)) gAllocCount.fetch_add (1);
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept                 { std::free (p); }
void operator delete[] (void* p) noexcept               { std::free (p); }
void operator delete (void* p, std::size_t) noexcept    { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept  { std::free (p); }

namespace
{
int gChecks = 0, gFailures = 0;

void check (bool ok, const std::string& what)
{
    ++gChecks;
    if (! ok)
        ++gFailures;
    std::printf ("  %s %s\n", ok ? "ok  " : "FAIL", what.c_str());
}

std::string fmt (const char* f, ...)
{
    char buf[512];
    va_list args;
    va_start (args, f);
    std::vsnprintf (buf, sizeof (buf), f, args);
    va_end (args);
    return buf;
}

using namespace st::mod;

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout (const Config& cfg)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (st::params::makeFloat ("drive", "Drive", st::params::linear (0.0f, 10.0f), 5.0f));
    layout.add (st::params::makeFloat ("mix", "Mix", st::params::linear (0.0f, 100.0f), 100.0f));
    layout.add (st::params::makeFloat ("tone", "Tone", st::params::linear (-1.0f, 1.0f), 0.0f));
    ModMatrix::addParameters (layout, cfg);
    return layout;
}

struct TestProcessor : juce::AudioProcessor
{
    explicit TestProcessor (const Config& cfg)
        : apvts (*this, &undo, "STATE", makeLayout (cfg)) {}

    const juce::String getName() const override { return "Test"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
};

void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plain)
{
    auto* p = s.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

constexpr double kSr = 48000.0;
constexpr int kChunk = 64;

/** Runs the matrix and measures the frequency of LFO 1 from interpolated upward zero crossings. */
double measureLfoHz (ModMatrix& m, double seconds, bool transport, double bpm = 120.0)
{
    const int hostBlock = 512;
    const auto total = (std::int64_t) (seconds * kSr);
    double prevV = 0.0, prevT = 0.0;
    double firstCross = -1.0, lastCross = -1.0;
    int crossings = 0;
    bool first = true;

    for (std::int64_t pos = 0; pos < total; pos += hostBlock)
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setIsPlaying (transport);
        info.setBpm (bpm);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        info.setPpqPosition ((double) pos / kSr * bpm / 60.0);
        m.setTransport (info);

        for (int off = 0; off < hostBlock; off += kChunk)
        {
            m.advance (kChunk);
            const double t = (double) (pos + off) / kSr;
            const double v = m.getSourceValue (0);
            if (! first && prevV < 0.0 && v >= 0.0)
            {
                const double tc = prevT + (t - prevT) * (-prevV / (v - prevV));
                if (firstCross < 0.0) firstCross = tc;
                lastCross = tc;
                ++crossings;
            }
            prevV = v;
            prevT = t;
            first = false;
        }
    }

    return crossings > 1 ? (double) (crossings - 1) / (lastCross - firstCross) : 0.0;
}

//==============================================================================
void testPersistence()
{
    std::printf ("Routing persistence / replaceState / undo\n");
    Config cfg;
    TestProcessor proc (cfg);
    ModMatrix m (proc.apvts, cfg);
    const int drive = m.addTarget ("drive");
    const int mix = m.addTarget ("mix");
    m.prepare (kSr, 512);

    check (drive == 0 && mix == 1, "target indices");
    check (m.addRouting (SourceId::lfo (0), "drive", 0.5f), "add routing lfo1->drive");
    check (m.addRouting (SourceId::macro (1), "mix", -0.3f), "add routing macro2->mix");
    check (! m.addRouting (SourceId::lfo (0), "tone", 0.5f), "unregistered target rejected");
    check (! m.addRouting (SourceId::lfo (7), "drive", 0.5f), "invalid source rejected");
    check (m.getNumRoutings() == 2, "two routings");
    check (m.getRoutingsForTarget ("drive").size() == 1, "routings for target");
    check (m.getRoutingsForSource (SourceId::macro (1)).size() == 1, "routings for source");

    auto saved = proc.apvts.copyState();
    const auto xml = saved.toXmlString();
    check (xml.contains ("MODULATION") && xml.contains ("lfo1") && xml.contains ("macro2"), "routings serialised in apvts.state");

    // Undo / redo of an edit
    proc.undo.beginNewTransaction();
    m.setRoutingDepth (SourceId::lfo (0), "drive", 0.9f);
    m.setRoutingDepth (SourceId::lfo (0), "drive", 0.8f);
    check (std::abs (m.getRouting (SourceId::lfo (0), "drive")->depth - 0.8f) < 1e-6f, "depth set");
    proc.undo.undo();
    check (std::abs (m.getRouting (SourceId::lfo (0), "drive")->depth - 0.5f) < 1e-6f, "depth drags coalesce into one undo step");
    m.removeRouting (SourceId::macro (1), "mix");
    check (m.getNumRoutings() == 1, "removed");
    proc.undo.undo();
    check (m.getNumRoutings() == 2, "undo remove restores routing");

    // Replace with an empty state (e.g. preset without modulation)
    juce::ValueTree empty ("STATE");
    proc.apvts.replaceState (empty);
    check (m.getNumRoutings() == 0, "replaceState with no MODULATION clears routings");

    m.advance (kChunk);
    check (! m.isTargetModulated (drive), "audio side cleared after replaceState");

    // Restore saved state
    proc.apvts.replaceState (saved.createCopy());
    check (m.getNumRoutings() == 2, "replaceState restores routings");
    auto r = m.getRouting (SourceId::macro (1), "mix");
    check (r.has_value() && std::abs (r->depth + 0.3f) < 1e-6f, "restored depth");

    m.advance (kChunk);
    check (m.isTargetModulated (drive) && m.isTargetModulated (mix), "audio side picks up restored routings");

    // A second matrix built from a saved state (session reload) sees routings immediately.
    TestProcessor proc2 (cfg);
    proc2.apvts.replaceState (saved.createCopy());
    ModMatrix m2 (proc2.apvts, cfg);
    m2.addTarget ("drive");
    m2.addTarget ("mix");
    check (m2.getNumRoutings() == 2, "routings loaded on construction");

    // maxRoutings
    Config small;
    small.maxRoutings = 2;
    TestProcessor proc3 (small);
    ModMatrix m3 (proc3.apvts, small);
    m3.addTarget ("drive"); m3.addTarget ("mix"); m3.addTarget ("tone");
    m3.addRouting (SourceId::lfo (0), "drive");
    m3.addRouting (SourceId::lfo (1), "mix");
    check (! m3.addRouting (SourceId::lfo (2), "tone"), "maxRoutings enforced");
}

void testLfoFrequency()
{
    std::printf ("LFO frequency accuracy\n");
    Config cfg;
    TestProcessor proc (cfg);
    ModMatrix m (proc.apvts, cfg);
    m.addTarget ("drive");
    m.prepare (kSr, 512);

    for (float hz : { 0.5f, 2.0f, 7.3f })
    {
        setParam (proc.apvts, "mod_lfo1_rate", hz);
        m.reset();
        const double f = measureLfoHz (m, 12.0, false);
        check (std::abs (f - hz) / hz < 0.002, fmt ("free-run %.2f Hz measured %.4f Hz", hz, f));
    }

    setParam (proc.apvts, "mod_lfo1_sync", 1.0f);
    const auto divs = getLfoDivisionNames();
    struct Case { const char* div; double hz; };
    for (auto c : { Case { "1/4", 2.0 }, Case { "1/8T", 6.0 }, Case { "1 Bar", 0.5 }, Case { "1/16.", 16.0 / 3.0 } })
    {
        setParam (proc.apvts, "mod_lfo1_division", (float) divs.indexOf (c.div));
        m.reset();
        const double f = measureLfoHz (m, 12.0, true, 120.0);
        check (std::abs (f - c.hz) / c.hz < 0.002, fmt ("synced %s @120 BPM: expected %.3f Hz, measured %.4f Hz", c.div, c.hz, f));
    }

    // Synced but transport stopped: free-runs at the tempo rate.
    setParam (proc.apvts, "mod_lfo1_division", (float) divs.indexOf ("1/4"));
    m.reset();
    const double fs = measureLfoHz (m, 12.0, false, 120.0);
    check (std::abs (fs - 2.0) / 2.0 < 0.002, fmt ("synced, stopped transport: measured %.4f Hz", fs));

    // Phase lock: at the start of a beat the phase is 0 (+ offset).
    for (float offsetDeg : { 0.0f, 90.0f })
    {
        setParam (proc.apvts, "mod_lfo1_phase", offsetDeg);
        juce::AudioPlayHead::PositionInfo info;
        info.setIsPlaying (true);
        info.setBpm (120.0);
        info.setPpqPosition (37.0);
        m.setTransport (info);
        m.advance (kChunk);
        const float expected = offsetDeg / 360.0f;
        check (std::abs (m.getLfoPhase (0) - expected) < 1e-4f, fmt ("phase locked to PPQ (offset %.0f): %.5f", offsetDeg, m.getLfoPhase (0)));
    }
    setParam (proc.apvts, "mod_lfo1_phase", 0.0f);

    // Unipolar sine stays in 0..1 and random shapes stay in range.
    setParam (proc.apvts, "mod_lfo1_sync", 0.0f);
    setParam (proc.apvts, "mod_lfo1_rate", 5.0f);
    setParam (proc.apvts, "mod_lfo1_unipolar", 1.0f);
    float lo = 1.0f, hi = 0.0f;
    for (int i = 0; i < 4000; ++i) { m.advance (kChunk); lo = std::min (lo, m.getSourceValue (0)); hi = std::max (hi, m.getSourceValue (0)); }
    check (lo >= 0.0f && lo < 0.01f && hi <= 1.0f && hi > 0.99f, fmt ("unipolar range [%.3f, %.3f]", lo, hi));
    setParam (proc.apvts, "mod_lfo1_unipolar", 0.0f);

    for (auto shape : { LfoShape::randomStep, LfoShape::randomSmooth })
    {
        setParam (proc.apvts, "mod_lfo1_shape", (float) (int) shape);
        float maxJump = 0.0f, prev = 0.0f, mn = 1.0f, mx = -1.0f;
        int changes = 0;
        m.reset();
        for (int i = 0; i < 7500; ++i) // 10 s @ 5 Hz -> ~50 cycles
        {
            m.advance (kChunk);
            const float v = m.getSourceValue (0);
            if (i > 0) { maxJump = std::max (maxJump, std::abs (v - prev)); if (! juce::exactlyEqual (v, prev)) ++changes; }
            prev = v; mn = std::min (mn, v); mx = std::max (mx, v);
        }
        check (mn >= -1.0f && mx <= 1.0f && mx - mn > 0.5f, fmt ("random shape %d range [%.2f, %.2f]", (int) shape, mn, mx));
        if (shape == LfoShape::randomStep)
            check (changes >= 45 && changes <= 51, fmt ("random step changes once per cycle (%d changes)", changes));
        else
            check (maxJump < 0.1f, fmt ("random smooth is continuous (max jump %.3f)", maxJump));
    }
}

void testEnvelope()
{
    std::printf ("Envelope follower response\n");
    Config cfg;
    TestProcessor proc (cfg);
    ModMatrix m (proc.apvts, cfg);
    m.addTarget ("drive");
    m.prepare (kSr, 512);
    setParam (proc.apvts, "mod_env1_attack", 10.0f);
    setParam (proc.apvts, "mod_env1_release", 100.0f);
    setParam (proc.apvts, "mod_env1_gain", 0.0f);

    const int envIdx = cfg.numLfos; // flat source index of ENV 1
    std::vector<float> ones (kChunk, 1.0f), zeros (kChunk, 0.0f), quarter (kChunk, 0.25f);
    auto run = [&] (const std::vector<float>& sig, double ms)
    {
        const float* ch[2] = { sig.data(), sig.data() };
        const int n = (int) std::lround (ms * 0.001 * kSr / kChunk);
        for (int i = 0; i < n; ++i) { m.feedEnvelopeInput (0, ch, 2, kChunk); m.advance (kChunk); }
        return m.getSourceValue (envIdx);
    };

    // Output maps the follower level to 0..1 over a 48 dB range (0 dBFS + 0 dB gain = 1).
    auto expectedOut = [] (double level) { return (float) std::clamp ((48.0 + 20.0 * std::log10 (level)) / 48.0, 0.0, 1.0); };
    auto actualMs = [] (double ms) { return (double) std::lround (ms * 0.001 * kSr / kChunk) * kChunk / kSr * 1000.0; };

    check (run (zeros, 50.0) <= 0.0f, "silence -> 0");
    const float atTau = run (ones, 10.0);
    const float expA = expectedOut (1.0 - std::exp (-actualMs (10.0) / 10.0));
    check (std::abs (atTau - expA) < 0.005f, fmt ("attack: value after ~10 ms = %.4f (expect %.4f)", atTau, expA));
    const float settled = run (ones, 200.0);
    check (settled > 0.995f, fmt ("full-scale input settles at 1 (%.4f)", settled));
    const float rel = run (zeros, 100.0);
    const float expR = expectedOut (std::exp (-actualMs (100.0) / 100.0));
    check (std::abs (rel - expR) < 0.005f, fmt ("release: value after ~100 ms = %.4f (expect %.4f)", rel, expR));
    const float gone = run (zeros, 1500.0);
    check (gone < 0.01f, fmt ("decays to 0 (%.4f)", gone));

    setParam (proc.apvts, "mod_env1_gain", 12.0f);
    const float boosted = run (quarter, 300.0);
    check (boosted > 0.99f, fmt ("-12 dBFS input + 12 dB gain -> ~1 (%.4f)", boosted));
    setParam (proc.apvts, "mod_env1_gain", 0.0f);
    const float plain = run (quarter, 300.0);
    check (std::abs (plain - 0.75f) < 0.01f, fmt ("-12 dBFS input -> 0.75 (%.4f)", plain));
    check (m.getEnvelopeInputChoice (0) == 0, "envelope input choice");
}

void testClamping()
{
    std::printf ("Modulated value clamping / display\n");
    Config cfg;
    TestProcessor proc (cfg);
    ModMatrix m (proc.apvts, cfg);
    const int drive = m.addTarget ("drive");
    m.prepare (kSr, 512);

    m.addRouting (SourceId::macro (0), "drive", 1.0f);
    m.addRouting (SourceId::macro (1), "drive", 0.25f);
    setParam (proc.apvts, "mod_macro1", 100.0f);
    setParam (proc.apvts, "mod_macro2", 100.0f);
    m.advance (kChunk);

    check (std::abs (m.getOffset (drive) - 1.25f) < 1e-5f, "offsets sum across routings");
    check (juce::exactlyEqual (m.getModulatedNormalised (drive, 0.8f), 1.0f), "clamped to 1");
    check (m.getModulatedNormalised (drive, -0.5f) >= 0.0f, "clamped >= 0");
    check (juce::exactlyEqual (m.getModulatedNormalised (42, 0.3f), 0.3f), "unknown target returns base");
    check (juce::exactlyEqual (m.getModulatedNormalised (-1, 0.3f), 0.3f), "negative target returns base");

    m.setRoutingDepth (SourceId::macro (0), "drive", -1.0f);
    m.removeRouting (SourceId::macro (1), "drive");
    m.advance (kChunk);
    check (juce::exactlyEqual (m.getModulatedNormalised (drive, 0.2f), 0.0f), "clamped to 0");

    setParam (proc.apvts, "mod_macro1", 50.0f);
    m.advance (kChunk);
    check (std::abs (m.getModulatedNormalised (drive, 0.6f) - 0.1f) < 1e-5f, "macro 50% * depth -1 -> -0.5");
    setParam (proc.apvts, "drive", 6.0f); // normalised 0.6
    check (std::abs (m.getModulatedPlain (drive) - 1.0f) < 1e-4f, fmt ("modulated plain value %.4f", m.getModulatedPlain (drive)));

    auto d = m.getDisplay ("drive", 0.6f);
    check (d.active && std::abs (d.lo - 0.0f) < 1e-5f && std::abs (d.hi - 0.6f) < 1e-5f, fmt ("display range [%.3f, %.3f]", d.lo, d.hi));
    check (std::abs (d.current - 0.1f) < 1e-5f, "display current");

    // Bipolar LFO depth 0.3 around 0.5 -> [0.2, 0.8]
    m.clearRoutings();
    m.addRouting (SourceId::lfo (0), "drive", 0.3f);
    d = m.getDisplay ("drive", 0.5f);
    check (d.active && std::abs (d.lo - 0.2f) < 1e-5f && std::abs (d.hi - 0.8f) < 1e-5f, fmt ("bipolar display [%.3f, %.3f]", d.lo, d.hi));
    check (! m.getDisplay ("mix", 0.5f).active, "unregistered target not active");

    m.setRoutingEnabled (SourceId::lfo (0), "drive", false);
    m.advance (kChunk);
    check (! m.isTargetModulated (drive) && ! m.getDisplay ("drive", 0.5f).active, "disabled routing ignored");
}

void testNoAllocation()
{
    std::printf ("No allocation in audio paths\n");
    Config cfg;
    TestProcessor proc (cfg);
    ModMatrix m (proc.apvts, cfg);
    const int drive = m.addTarget ("drive");
    m.addTarget ("mix");
    m.prepare (kSr, 512);
    for (int i = 0; i < cfg.numLfos; ++i)
        m.addRouting (SourceId::lfo (i), i % 2 ? "drive" : "mix", 0.2f);
    m.addRouting (SourceId::envelope (0), "drive", 0.4f);
    setParam (proc.apvts, "mod_lfo2_sync", 1.0f);
    setParam (proc.apvts, "mod_lfo3_shape", (float) (int) LfoShape::randomSmooth);

    std::vector<float> sig (kChunk, 0.5f);
    const float* ch[2] = { sig.data(), sig.data() };
    juce::AudioPlayHead::PositionInfo info;
    info.setIsPlaying (true);
    info.setBpm (128.0);
    float sink = 0.0f;

    // routing change pending -> must be picked up without allocating
    m.setRoutingDepth (SourceId::lfo (0), "mix", 0.7f);

    // Sanity check that the tracker sees allocations at all.
    gAllocCount = 0;
    gTrackAllocs = true;
    { auto* probe = new juce::String ("probe"); delete probe; }
    gTrackAllocs = false;
    check (gAllocCount.load() > 0, "allocation tracker is armed");

    gAllocCount = 0;
    gTrackAllocs = true;
    for (int block = 0; block < 2000; ++block)
    {
        info.setPpqPosition (block * 0.02);
        m.setTransport (info);
        for (int s = 0; s < 8; ++s)
        {
            m.feedEnvelopeInput (0, ch, 2, kChunk);
            m.advance (kChunk);
            sink += m.getModulatedNormalised (drive, 0.5f) + m.getModulatedPlain (drive) + (m.isTargetModulated (drive) ? 1.0f : 0.0f);
        }
    }
    gTrackAllocs = false;
    check (gAllocCount.load() == 0, fmt ("allocations during 16000 advance() calls: %d", gAllocCount.load()));
    check (std::isfinite (sink), "outputs finite");
}

void testUi()
{
    std::printf ("UI smoke test (panel, binder, knob drop + menu)\n");
    Config cfg;
    TestProcessor proc (cfg);
    ModMatrix m (proc.apvts, cfg);
    m.addTarget ("drive");
    m.prepare (kSr, 512);

    juce::Component root;
    root.setBounds (0, 0, 800, 400);
    st::Knob driveKnob (*proc.apvts.getParameter ("drive"), &proc.undo);
    st::Knob mixKnob (*proc.apvts.getParameter ("mix"), &proc.undo);
    st::ModulationPanel panel (m);
    root.addAndMakeVisible (driveKnob);
    root.addAndMakeVisible (mixKnob);
    root.addAndMakeVisible (panel);
    driveKnob.setBounds (10, 10, 70, 80);
    mixKnob.setBounds (90, 10, 70, 80);
    panel.setBounds (0, 120, 720, st::ModulationPanel::preferredHeight);

    st::ModulationBinder binder (m, root, &panel);
    check (driveKnob.onModulationDropped != nullptr, "registered knob bound");
    check (mixKnob.onModulationDropped == nullptr, "unregistered knob not bound");

    int src = -1;
    juce::Colour col;
    check (st::Knob::parseModulationDrag (st::Knob::makeModulationDragDescription (5, juce::Colours::red), src, col)
               && src == 5 && col == juce::Colours::red, "drag description round trip");
    check (! st::Knob::parseModulationDrag ("something", src, col), "foreign drag rejected");

    driveKnob.onModulationDropped (1); // drop LFO 2
    check (m.getRouting (SourceId::lfo (1), "drive").has_value(), "drop creates routing");
    check (panel.getSelectedSource() == 1, "drop selects source");
    m.dispatchPendingMessages();

    juce::PopupMenu menu;
    driveKnob.extendContextMenu (menu);
    check (menu.getNumItems() >= 3, fmt ("context menu has modulation items (%d)", menu.getNumItems()));

    // Paint every editor type with live data.
    for (int i = 0; i < m.getNumSources(); ++i)
    {
        panel.selectSource (i);
        m.addRouting (m.getSourceId (i), "drive", 0.1f * (float) (i + 1) - 0.5f);
        m.dispatchPendingMessages();
        m.advance (kChunk);
        juce::Image img (juce::Image::ARGB, 800, 400, true);
        juce::Graphics g (img);
        root.paintEntireComponent (g, false);
    }
    check (true, "panel painted for all sources");

    // Optional snapshots for visual review: ST_MOD_SNAPSHOT_DIR=/some/dir
    if (auto dir = juce::SystemStats::getEnvironmentVariable ("ST_MOD_SNAPSHOT_DIR", {}); dir.isNotEmpty())
    {
        setParam (proc.apvts, "mod_lfo1_shape", 1.0f);
        setParam (proc.apvts, "mod_macro1", 65.0f);
        for (int i = 0; i < 300; ++i)
        {
            std::vector<float> sig (kChunk, 0.5f * (float) std::abs (std::sin (i * 0.05)));
            const float* ch[1] = { sig.data() };
            m.feedEnvelopeInput (0, ch, 1, kChunk);
            m.advance (kChunk);
        }
        for (int i : { 0, 4, 6, 1 })
        {
            if (i == 1) // show drop-target feedback in the last snapshot
            {
                driveKnob.setBounds (10, 10, 70, 80);
                driveKnob.itemDragEnter ({ st::Knob::makeModulationDragDescription (1, panel.getSourceColour (1)), nullptr, {} });
            }
            panel.selectSource (i);
            m.dispatchPendingMessages();
            for (int k = 0; k < 60; ++k)
            {
                for (int a = 0; a < 18; ++a)
                {
                    std::vector<float> sig (kChunk, 0.6f * (float) std::abs (std::sin ((double) (k * 18 + a) * 0.02)));
                    const float* ch[1] = { sig.data() };
                    m.feedEnvelopeInput (0, ch, 1, kChunk);
                    m.advance (kChunk);
                }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (25); // lets the panel timer tick
            }
            const float scale = 2.0f;
            juce::Image img (juce::Image::ARGB, (int) (root.getWidth() * scale), (int) (root.getHeight() * scale), true);
            {
                juce::Graphics g (img);
                g.fillAll (st::Theme::get().bg0);
                g.addTransform (juce::AffineTransform::scale (scale));
                root.paintEntireComponent (g, false);
            }
            juce::PNGImageFormat png;
            juce::File f (dir + "/mod_panel_" + juce::String (i) + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            png.writeImageToStream (img, os);
        }
    }

    binder.refresh();
}

} // namespace

int main()
{
    {
        juce::ScopedJuceInitialiser_GUI juceInit;

        testPersistence();
        testLfoFrequency();
        testEnvelope();
        testClamping();
        testNoAllocation();
        testUi();
    }

    std::printf ("\n%d checks, %d failures\n", gChecks, gFailures);
    std::fflush (stdout);
    // Skip static destructors: st::fonts' function-local Typeface cache is destroyed after
    // JUCE's own typeface cache and aborts at exit (pre-existing, see theme/Theme.cpp).
    std::_Exit (gFailures);
}
