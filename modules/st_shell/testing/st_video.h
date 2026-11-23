#pragma once
// Use-case videos of a plug-in's real editor (for the website), for test tools only: not part of the st_shell module
// build, include it from a test executable with  #include <st_shell/testing/st_video.h>.
//
// A synthesized mix (st::video::Music) plays through the processor in real time (one audio block per video frame)
// while a scripted pointer (st::video::Director) works the UI through the components' own mouse handlers. Frames
// (with a drawn cursor and click ripple) are piped to ffmpeg (must be on the PATH) as H.264 MP4.
//
//   int main (int argc, char** argv)
//   {
//       if (juce::StringArray (argv + 1, argc - 1).contains ("--video"))
//           return st::video::run (argc, argv, "st-eq", [] { return std::make_unique<MyProcessor>(); }, myScenes());
//       ...
//   }
//
// Run:  MyPluginTests --video <dir> [scene ...]   (or another flag: see run())   ->  <dir>/<prefix>-<scene>.mp4 (+ a poster .jpg each)
#include <st_shell/st_shell.h>
#include <cstdio>
#include <iostream>

namespace st::video
{
inline constexpr double sampleRate = 48000.0;
inline constexpr int fps = 30, blockSize = 1600;   // one audio block per video frame

//==============================================================================
/** Minimal RBJ biquad. */
struct Biquad
{
    enum Type { lowPass, bandPass, highPass };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (Type type, float f, float q)
    {
        const float w = juce::MathConstants<float>::twoPi * juce::jlimit (10.0f, (float) sampleRate * 0.45f, f) / (float) sampleRate;
        const float c = std::cos (w), al = std::sin (w) / (2.0f * q), a0 = 1.0f + al;
        if (type == lowPass) { b0 = (1 - c) * 0.5f; b1 = 1 - c; b2 = b0; }
        else if (type == bandPass) { b0 = al; b1 = 0; b2 = -al; }
        else { b0 = (1 + c) * 0.5f; b1 = -(1 + c); b2 = b0; }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    float operator() (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

/** Band-limited sawtooth (polyBLEP). */
struct Saw
{
    double phase = 0.0;
    float operator() (double hz)
    {
        const double dt = hz / sampleRate;
        phase += dt; if (phase >= 1.0) phase -= 1.0;
        const double v = 2.0 * phase - 1.0;
        auto blep = [dt] (double t) { if (t < dt) { t /= dt; return t + t - t * t - 1.0; } if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; } return 0.0; };
        return (float) (v - blep (phase));
    }
};

/** A small Schroeder room (four damped combs, two all-passes), different taps per side. */
struct Room
{
    struct Comb { std::vector<float> d; size_t i = 0; float lp = 0; float operator() (float x) { const float y = d[i]; lp += 0.35f * (y - lp); d[i] = x + lp * 0.8f; i = (i + 1) % d.size(); return y; } };
    struct AllPass { std::vector<float> d; size_t i = 0; float operator() (float x) { const float y = d[i]; d[i] = x + y * 0.5f; i = (i + 1) % d.size(); return y - x; } };
    Comb comb[2][4];
    AllPass ap[2][2];
    Room()
    {
        const int sizes[4] = { 1557, 1617, 1491, 1422 }, aps[2] = { 556, 441 };
        for (int s = 0; s < 2; ++s)
        {
            for (int k = 0; k < 4; ++k) comb[s][k].d.assign ((size_t) ((sizes[k] + s * 23) * 0.75), 0.0f);
            for (int k = 0; k < 2; ++k) ap[s][k].d.assign ((size_t) (aps[k] + s * 17), 0.0f);
        }
    }
    float operator() (int side, float x)
    {
        float y = 0.0f;
        for (auto& cb : comb[side]) y += cb (x * 0.25f);
        for (auto& a : ap[side]) y = a (y);
        return y;
    }
};

inline float midiHz (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

/** A small band at 96 BPM (Am F C G, a four-bar loop): drums (kick, snare, hats; room mics optional), bass, pad,
    a formant "vocal" with sibilance, low rumble / hum and a boxy room resonance. Set the levels per scene; 0 = off.
    `dynamics` > 0 makes the vocal and drums swing between loud and soft phrases (for compressors). */
struct Music
{
    float vocal = 0, sib = 0, rumble = 0, box = 0, drums = 0, rooms = 0, bass = 0, pad = 0, dynamics = 0;
    juce::Random rng { 42 };
    int64_t n = 0;

    static constexpr double beat = 60.0 / 96.0, bar = 4.0 * beat;
    Saw voice, bassOsc, padOsc[3][2];
    Room room;
    Biquad formant[3], sibBp, breathBp, boxBp, rumbleLp, rumbleLp2, bassLp, padLpL, padLpR, snareBp, hatHp, hatHp2, kickClick;
    float vocalSend = 0.0f, drumSend = 0.0f, padSendL = 0.0f, padSendR = 0.0f;
    float voiceHz = 220.0f, glottal = 0.0f, kickPhase = 0.0f, snareTone = 0.0f, subPhase = 0.0f, humPhase = 0.0f;

    Music()
    {
        sibBp.set (Biquad::bandPass, 7200.0f, 1.4f); breathBp.set (Biquad::bandPass, 2200.0f, 0.8f); boxBp.set (Biquad::bandPass, 330.0f, 2.2f);
        rumbleLp.set (Biquad::lowPass, 55.0f, 0.9f); rumbleLp2.set (Biquad::lowPass, 55.0f, 0.9f); bassLp.set (Biquad::lowPass, 420.0f, 1.1f);
        padLpL.set (Biquad::lowPass, 1700.0f, 0.7f); padLpR.set (Biquad::lowPass, 1700.0f, 0.7f); snareBp.set (Biquad::bandPass, 2600.0f, 0.6f);
        hatHp.set (Biquad::highPass, 7000.0f, 0.7f); hatHp2.set (Biquad::highPass, 7000.0f, 0.7f); kickClick.set (Biquad::bandPass, 3500.0f, 1.0f);
    }

    /** Time of the current sample within the loop (seconds). */
    double time() const noexcept { return (double) n / sampleRate; }

    void render (juce::AudioBuffer<float>& buf)
    {
        static const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 60, 64 }, { 55, 59, 62 } };   // Am F C G
        static const int roots[4] = { 33, 29, 36, 31 };
        static const int melody[4][4] = { { 76, 76, 74, 72 }, { 72, 69, 72, 74 }, { 76, 79, 76, 74 }, { 74, 71, 0, 0 } };
        static const float vowels[4][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 450, 800, 2830 }, { 330, 2100, 2900 } };
        static const float vowelGain[3] = { 1.0f, 0.55f, 0.3f };
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        const int numCh = buf.getNumChannels();

        for (int i = 0; i < buf.getNumSamples(); ++i, ++n)
        {
            const double t = (double) n / sampleRate;
            const int barIdx = (int) (t / bar) % 4;
            const double inBar = std::fmod (t, bar);
            const int beatIdx = (int) (inBar / beat);
            const double inBeat = std::fmod (inBar, beat);
            const int eighth = (int) (inBar / (beat * 0.5));
            const double inEighth = std::fmod (inBar, beat * 0.5);
            // loud / soft phrases: bars 0 and 2 loud, 1 and 3 soft (smoothed at the bar lines)
            const float swing = 1.0f - dynamics * 0.75f * (float) (0.5 - 0.5 * std::cos (twoPi * 0.5f * (float) (t / bar - 0.25)));
            float L = 0.0f, R = 0.0f;

            if (drums > 0.0f)
            {
                double kt = -1.0;
                if (eighth == 0 || eighth == 4 || (eighth == 5 && barIdx % 2 == 1)) kt = inEighth;
                float d = 0.0f;
                if (kt >= 0.0)
                {
                    const float f = 46.0f + 115.0f * (float) std::exp (-kt * 32.0);
                    kickPhase += f / (float) sampleRate;
                    d += std::sin (twoPi * kickPhase) * (float) std::exp (-kt * 6.5) * 0.95f;
                    d += kickClick (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-kt * 250.0) * 0.6f;
                }
                if (beatIdx == 1 || beatIdx == 3)
                {
                    snareTone += 185.0f / (float) sampleRate;
                    d += std::sin (twoPi * snareTone) * (float) std::exp (-inBeat * 28.0) * 0.35f;
                    d += snareBp (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-inBeat * 15.0) * 1.1f;
                }
                const bool open = eighth == 7;
                const float hatEnv = (float) std::exp (-inEighth * (open ? 9.0 : 55.0)) * (eighth % 2 ? 0.32f : 0.2f);
                const float hat = hatHp2 (hatHp (rng.nextFloat() * 2.0f - 1.0f)) * hatEnv;
                const float g = drums * (dynamics > 0.0f ? 0.6f + 0.4f * swing : 1.0f);
                L += (d + hat * 0.8f) * g;
                R += (d + hat * 1.2f) * g;
                drumSend = d * g * rooms;
            }

            if (bass > 0.0f)
            {
                static const bool pattern[8] = { true, false, true, true, false, true, true, false };
                const float env = pattern[eighth] ? (float) (std::exp (-inEighth * 3.0) * juce::jmin (1.0, inEighth * 300.0)) : 0.0f;
                const float hz = midiHz ((float) roots[barIdx]);
                subPhase += hz / (float) sampleRate;
                const float b = bassLp (bassOsc (hz)) * 0.55f + std::sin (twoPi * subPhase) * 0.5f;
                L += b * env * bass; R += b * env * bass;
            }

            if (pad > 0.0f)
            {
                const float env = (float) (juce::jmin (1.0, inBar / 0.35) * juce::jmin (1.0, (bar - inBar) / 0.2));
                float pl = 0.0f, pr = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const float hz = midiHz ((float) chords[barIdx][k]);
                    pl += padOsc[k][0] (hz * 0.996);
                    pr += padOsc[k][1] (hz * 1.004);
                }
                padSendL = padLpL (pl) * 0.16f * env * pad;
                padSendR = padLpR (pr) * 0.16f * env * pad;
                L += padSendL; R += padSendR;
            }

            if (vocal > 0.0f || rumble > 0.0f)
            {
                const int note = melody[barIdx][beatIdx];
                float v = 0.0f;
                if (vocal > 0.0f)
                {
                    if (note > 0)
                    {
                        const float target = midiHz ((float) note - 12.0f);
                        voiceHz += (target - voiceHz) * 0.0025f;
                        const float vib = 1.0f + 0.006f * std::sin (twoPi * 5.4f * (float) t) * (float) juce::jmin (1.0, inBeat * 3.0);
                        const float env = (float) (juce::jmin (1.0, inBeat * 25.0) * juce::jmin (1.0, (beat - inBeat) * 12.0 + 0.35));
                        glottal += 0.25f * (voice (voiceHz * vib) - glottal);
                        const auto& fm = vowels[(barIdx * 4 + beatIdx) % 4];
                        for (int k = 0; k < 3; ++k)
                        {
                            formant[k].set (Biquad::bandPass, fm[k], k == 0 ? 6.0f : 9.0f);
                            v += formant[k] (glottal) * vowelGain[k];
                        }
                        v = v * 2.4f * env + breathBp (rng.nextFloat() * 2.0f - 1.0f) * 0.03f * env;
                    }
                    const bool s = (note > 0 && beatIdx % 2 == 0 && inBeat < 0.14) || (barIdx == 3 && beatIdx == 2 && inBeat < 0.32);
                    if (s)
                    {
                        const float senv = (float) (juce::jmin (1.0, inBeat * 60.0) * std::exp (-inBeat * (barIdx == 3 ? 5.0 : 14.0)));
                        v += sibBp (rng.nextFloat() * 2.0f - 1.0f) * senv * 1.6f * sib;
                    }
                    v += boxBp (v) * box * 1.8f;
                    v *= vocal * (dynamics > 0.0f ? swing : 1.0f);
                }
                if (rumble > 0.0f)
                {
                    humPhase += 50.0f / (float) sampleRate;
                    v += (rumbleLp2 (rumbleLp (rng.nextFloat() * 2.0f - 1.0f)) * 3.5f + std::sin (twoPi * humPhase) * 0.05f) * rumble;
                }
                L += v; R += v;
                vocalSend = v;
            }

            L += room (0, vocalSend * 0.55f + padSendL * 0.4f + drumSend * 1.2f) * 0.9f;
            R += room (1, vocalSend * 0.55f + padSendR * 0.4f + drumSend * 1.2f) * 0.9f;
            vocalSend = drumSend = padSendL = padSendR = 0.0f;

            buf.setSample (0, i, std::tanh (L * 0.8f) * 0.6f);
            if (numCh > 1) buf.setSample (1, i, std::tanh (R * 0.8f) * 0.6f);
            for (int ch = 2; ch < numCh; ++ch) buf.setSample (ch, i, 0.0f);
        }
    }
};

//==============================================================================
/** Sends mouse events straight to the components under a virtual pointer (in editor coordinates). */
struct Pointer
{
    explicit Pointer (juce::Component& r) : root (r) {}

    juce::Component& root;
    juce::Point<float> pos { 500.0f, 300.0f }, downPos;
    juce::Component::SafePointer<juce::Component> hover, pressed;
    double pressFlash = -10.0;   // video time of the last press (cursor ripple)

    /** The deepest visible component under p that takes clicks (front-most first). */
    static juce::Component* deepest (juce::Component& c, juce::Point<float> local)
    {
        for (int i = c.getNumChildComponents(); --i >= 0;)
        {
            auto* child = c.getChildComponent (i);
            if (! child->isVisible()) continue;
            const auto q = child->getLocalPoint (&c, local);
            if (! child->getLocalBounds().toFloat().contains (q)) continue;
            if (auto* hit = deepest (*child, q)) return hit;
        }
        bool self = true, kids = true;
        c.getInterceptsMouseClicks (self, kids);
        return self && c.hitTest (juce::roundToInt (local.x), juce::roundToInt (local.y)) ? &c : nullptr;
    }
    juce::Component* at (juce::Point<float> p)
    {
        auto* c = deepest (root, p);
        return c != nullptr ? c : &root;
    }
    juce::MouseEvent ev (juce::Component* c, juce::ModifierKeys mods, int clicks = 1, bool dragged = false) const
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), c->getLocalPoint (&root, pos), mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 c, c, now, c->getLocalPoint (&root, downPos), now, clicks, dragged);
    }
    void moveTo (juce::Point<float> p)
    {
        pos = p;
        if (pressed != nullptr) { pressed->mouseDrag (ev (pressed, juce::ModifierKeys::leftButtonModifier, 1, true)); return; }
        auto* c = at (p);
        if (c != hover.getComponent())
        {
            if (hover != nullptr) hover->mouseExit (ev (hover, {}));
            hover = c;
            c->mouseEnter (ev (c, {}));
        }
        c->mouseMove (ev (c, {}));
    }
    void down (int clicks = 1)
    {
        downPos = pos;
        pressed = at (pos);
        pressed->mouseDown (ev (pressed, juce::ModifierKeys::leftButtonModifier, clicks));
    }
    void up()
    {
        if (pressed != nullptr) pressed->mouseUp (ev (pressed, {}, 1, pos != downPos));
        pressed = nullptr;
    }
    void doubleClick()
    {
        down(); up(); down (2);
        if (pressed != nullptr) pressed->mouseDoubleClick (ev (pressed, {}, 2));
        up();
    }
    void wheel (float dy)
    {
        auto* c = at (pos);
        juce::MouseWheelDetails w { 0.0f, dy, false, false, false };
        c->mouseWheelMove (ev (c, {}), w);
    }
};

/** A timeline of steps; targets are functions, so they follow things that move. */
struct Script
{
    struct Step { double dur; std::function<void()> start; std::function<void (float)> tick; };
    std::vector<Step> steps;
    size_t index = 0;
    double stepTime = 0.0;
    bool started = false;

    bool done() const { return index >= steps.size(); }
    void advance (double dt)
    {
        stepTime += dt;
        while (! done())
        {
            auto& s = steps[index];
            if (! started) { if (s.start) s.start(); started = true; }
            const float u = s.dur > 0.0 ? (float) juce::jlimit (0.0, 1.0, stepTime / s.dur) : 1.0f;
            if (s.tick) s.tick (u);
            if (stepTime < s.dur) return;
            stepTime -= s.dur;
            ++index;
            started = false;
        }
    }
};

inline float ease (float u) { return u * u * (3.0f - 2.0f * u); }

using Target = std::function<juce::Point<float>()>;

/** Writes a scene: pointer moves, presses, drags, wheel steps, waits and calls, in editor coordinates. */
struct Director
{
    juce::AudioProcessorEditor& editor;
    juce::AudioProcessor& processor;
    Pointer& ptr;
    Script& s;
    double now = 0.0;   // video time, kept current by the recorder

    // --- finding things
    /** Centre of `c` in editor coordinates (the pointer's position when c is null). */
    juce::Point<float> centreOf (const juce::Component* c) const
    {
        return c == nullptr || c->getParentComponent() == nullptr ? ptr.pos
                                                                  : editor.getLocalPoint (c->getParentComponent(), c->getBounds().toFloat().getCentre());
    }
    /** A point given in `c`'s own coordinates, in editor coordinates. */
    juce::Point<float> pointIn (const juce::Component& c, juce::Point<float> local) const { return editor.getLocalPoint (&c, local); }
    /** The first visible component below `root` (the editor by default) that matches. */
    juce::Component* find (std::function<bool (juce::Component&)> pred, juce::Component* root = nullptr) const
    {
        std::function<juce::Component* (juce::Component&)> walk = [&] (juce::Component& c) -> juce::Component*
        {
            for (auto* child : c.getChildren())
            {
                if (! child->isVisible()) continue;
                if (pred (*child)) return child;
                if (auto* hit = walk (*child)) return hit;
            }
            return nullptr;
        };
        return walk (root != nullptr ? *root : static_cast<juce::Component&> (editor));
    }
    template <typename T> T* findType (std::function<bool (T&)> pred = {}, juce::Component* root = nullptr) const
    {
        return dynamic_cast<T*> (find ([&] (juce::Component& c) { auto* t = dynamic_cast<T*> (&c); return t != nullptr && (! pred || pred (*t)); }, root));
    }
    /** A button by its tooltip. */
    juce::Point<float> button (const juce::String& tooltip) const
    {
        return centreOf (findType<juce::Button> ([&] (juce::Button& b) { return b.getTooltip() == tooltip; }));
    }
    /** A control bound to the parameter `id` (st::ParameterControl: knobs, dropdowns...), below `root`. */
    st::ParameterControl* control (const juce::String& id, juce::Component* root = nullptr) const
    {
        return findType<st::ParameterControl> ([&] (st::ParameterControl& k) { return k.getParameter().paramID == id; }, root);
    }
    juce::Point<float> controlCentre (const juce::String& id) const { return centreOf (control (id)); }

    // --- steps
    void wait (double sec) { s.steps.push_back ({ sec, {}, {} }); }
    void call (std::function<void()> f) { s.steps.push_back ({ 0.0, std::move (f), {} }); }
    void move (Target target, double sec)
    {
        auto from = std::make_shared<juce::Point<float>>();
        s.steps.push_back ({ sec, [this, from] { *from = ptr.pos; }, [this, from, target] (float u)
        {
            const auto to = target();
            const auto a = *from + (to - *from) * ease (u);
            const float lift = std::sin (u * juce::MathConstants<float>::pi) * juce::jmin (18.0f, from->getDistanceFrom (to) * 0.08f);   // a hand's arc
            ptr.moveTo (a.translated (0.0f, -lift));
        } });
    }
    void moveTo (juce::Point<float> p, double sec) { move ([p] { return p; }, sec); }
    /** Press where the pointer is, drag to a target, release. The target is re-read every frame: never aim at
        something the drag itself moves (a split line, a handle); use dragBy for those. */
    void drag (Target target, double sec)
    {
        auto from = std::make_shared<juce::Point<float>>();
        s.steps.push_back ({ 0.12, [this, from] { *from = ptr.pos; ptr.pressFlash = now; ptr.down(); }, {} });
        s.steps.push_back ({ sec, [this, from] { *from = ptr.pos; }, [this, from, target] (float u) { ptr.moveTo (*from + (target() - *from) * ease (u)); } });
        s.steps.push_back ({ 0.1, [this] { ptr.up(); }, {} });
    }
    void dragBy (juce::Point<float> offset, double sec)
    {
        auto from = std::make_shared<juce::Point<float>>();
        s.steps.push_back ({ 0.12, [this, from] { *from = ptr.pos; ptr.pressFlash = now; ptr.down(); }, {} });
        s.steps.push_back ({ sec, {}, [this, from, offset] (float u) { ptr.moveTo (*from + offset * ease (u)); } });
        s.steps.push_back ({ 0.1, [this] { ptr.up(); }, {} });
    }
    void click() { s.steps.push_back ({ 0.15, [this] { ptr.pressFlash = now; ptr.down(); ptr.up(); }, {} }); }
    void doubleClick() { s.steps.push_back ({ 0.3, [this] { ptr.pressFlash = now; ptr.doubleClick(); }, {} }); }
    void wheel (float dy, int times, double interval)
    {
        for (int i = 0; i < times; ++i)
            s.steps.push_back ({ interval, [this, dy] { ptr.wheel (dy); }, {} });
    }
    /** Set a parameter as a gesture (for things that only menus reach; pair it with a click on the control). */
    void setParameter (const juce::String& id, float plain)
    {
        call ([this, id, plain]
        {
            for (auto* p : processor.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p); r != nullptr && r->paramID == id)
                {
                    r->beginChangeGesture();
                    r->setValueNotifyingHost (r->convertTo0to1 (plain));
                    r->endChangeGesture();
                }
        });
    }
};

/** Sets a parameter by ID (plain value) without a gesture: for scene setup. */
inline void setParam (juce::AudioProcessor& p, const juce::String& id, float plain)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm); r != nullptr && r->paramID == id)
            r->setValueNotifyingHost (r->convertTo0to1 (plain));
}

//==============================================================================
struct Scene
{
    juce::String name;
    double seconds;
    /** Prepare the (fresh) processor and the mix; runs before the editor opens. */
    std::function<void (juce::AudioProcessor&, Music&)> setup;
    std::function<void (Director&)> script;
    /** Optional: MIDI for each block (instruments). `start` is the block's first sample since the scene began. */
    std::function<void (juce::MidiBuffer&, int64_t start, int numSamples)> midi = {};
    /** Optional: called once the editor is open (open a panel, pick a view...). */
    std::function<void (juce::AudioProcessorEditor&)> afterOpen = {};
};

inline void drawCursor (juce::Graphics& g, juce::Point<float> p, bool pressed, float ripple)
{
    if (ripple > 0.0f && ripple < 1.0f)
    {
        const float r = 6.0f + 16.0f * ripple;
        g.setColour (juce::Colours::white.withAlpha (0.45f * (1.0f - ripple)));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), 1.5f);
    }
    juce::Path arrow;
    arrow.startNewSubPath (0, 0); arrow.lineTo (0, 17); arrow.lineTo (4.2f, 13.2f); arrow.lineTo (7.2f, 19.8f);
    arrow.lineTo (9.8f, 18.7f); arrow.lineTo (6.9f, 12.3f); arrow.lineTo (12.4f, 12.3f); arrow.closeSubPath();
    const auto tf = juce::AffineTransform::scale (pressed ? 0.9f : 1.0f).translated (p);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (arrow, tf.translated (0.8f, 1.6f));
    g.setColour (juce::Colours::black);
    g.fillPath (arrow, tf);
    g.setColour (juce::Colours::white);
    g.strokePath (arrow, juce::PathStrokeType (1.3f, juce::PathStrokeType::mitered), tf);
}

//==============================================================================
struct Config
{
    juce::File dir;
    juce::String prefix;
    std::function<std::unique_ptr<juce::AudioProcessor>()> makeProcessor;
    std::vector<Scene> scenes;
    juce::StringArray only;
    float scale = 1.5f;
};

inline Config& config() { static Config c; return c; }

/** Runs the scenes on the message thread, paced in real time (the editor's own timers and frame clocks run as in a
    host); one audio block and one frame per tick. */
struct Recorder : juce::Timer
{
    Config& cfg;
    size_t index = 0;
    std::unique_ptr<juce::AudioProcessor> proc;
    std::unique_ptr<Music> music;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    std::unique_ptr<Pointer> ptr;
    std::unique_ptr<Script> script;
    std::unique_ptr<Director> director;
    FILE* pipe = nullptr;
    int frame = 0, preroll = 0, w = 0, h = 0;
    int64_t samplePos = 0;
    double wallStart = 0.0;
    juce::File out;

    explicit Recorder (Config& c) : cfg (c) {}

    void begin()
    {
        auto& sc = cfg.scenes[index];
        proc = cfg.makeProcessor();
        proc->setPlayConfigDetails (proc->getTotalNumInputChannels(), proc->getTotalNumOutputChannels(), sampleRate, blockSize);
        proc->prepareToPlay (sampleRate, blockSize);
        music = std::make_unique<Music>();
        sc.setup (*proc, *music);
        ed.reset (proc->createEditorAndMakeActive());
        ptr = std::make_unique<Pointer> (*ed);
        ptr->pos = { (float) ed->getWidth() * 0.62f, (float) ed->getHeight() * 0.3f };
        script = std::make_unique<Script>();
        director = std::make_unique<Director> (Director { *ed, *proc, *ptr, *script });
        frame = 0;
        samplePos = 0;
        preroll = fps * 2;   // meters and analysers fill before recording starts
        w = juce::roundToInt ((float) ed->getWidth() * cfg.scale) & ~1;
        h = juce::roundToInt ((float) ed->getHeight() * cfg.scale) & ~1;
        out = cfg.dir.getChildFile (cfg.prefix + "-" + sc.name + ".mp4");
        out.deleteFile();
        const auto cmd = "ffmpeg -y -loglevel error -f rawvideo -pix_fmt bgra -s " + juce::String (w) + "x" + juce::String (h) + " -r " + juce::String (fps)
                       + " -i - -an -c:v libx264 -preset slow -crf 22 -pix_fmt yuv420p -movflags +faststart \"" + out.getFullPathName() + "\"";
        pipe = popen (cmd.toRawUTF8(), "w");
        wallStart = juce::Time::getMillisecondCounterHiRes();
        std::cout << "rendering " << sc.name << "\n";
    }

    void finish()
    {
        if (pipe != nullptr) pclose (pipe);
        pipe = nullptr;
        const double wall = (juce::Time::getMillisecondCounterHiRes() - wallStart) / 1000.0;
        std::cout << "  " << out.getFileName() << ": " << frame << " frames, " << juce::String (wall, 1) << " s wall for "
                  << juce::String ((double) (frame + fps * 2) / fps, 1) << " s\n";
        // poster: a frame near the end
        const auto poster = out.withFileExtension ("jpg");
        const auto cmd = "ffmpeg -v error -y -sseof -1.2 -i \"" + out.getFullPathName() + "\" -frames:v 1 -q:v 3 \"" + poster.getFullPathName() + "\"";
        std::system (cmd.toRawUTF8());
        director.reset(); script.reset(); ptr.reset();
        ed.reset();
        proc->releaseResources();
        proc.reset();
        ++index;
    }

    void timerCallback() override
    {
        if (ed == nullptr)
        {
            if (index >= cfg.scenes.size()) { stopTimer(); juce::JUCEApplicationBase::quit(); return; }
            begin();
            if (cfg.scenes[index].afterOpen) cfg.scenes[index].afterOpen (*ed);
            cfg.scenes[index].script (*director);
        }

        auto& sc = cfg.scenes[index];
        juce::AudioBuffer<float> buf (juce::jmax (2, juce::jmax (proc->getTotalNumInputChannels(), proc->getTotalNumOutputChannels())), blockSize);
        juce::MidiBuffer midi;
        music->render (buf);
        if (sc.midi) sc.midi (midi, samplePos, blockSize);
        samplePos += blockSize;
        proc->processBlock (buf, midi);

        if (preroll > 0) { --preroll; return; }

        director->now = (double) frame / fps;
        script->advance (1.0 / fps);

        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, cfg.scale);
        if (img.getWidth() != w || img.getHeight() != h) img = img.rescaled (w, h, juce::Graphics::highResamplingQuality);
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale ((float) w / (float) ed->getWidth()));
            drawCursor (g, ptr->pos, ptr->pressed != nullptr, (float) ((director->now - ptr->pressFlash) / 0.35));
        }
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < bd.height; ++y)
            fwrite (bd.getLinePointer (y), 4, (size_t) bd.width, pipe);

        if (++frame >= juce::roundToInt (sc.seconds * fps)) finish();
    }
};

struct App : juce::JUCEApplicationBase
{
    std::unique_ptr<Recorder> recorder;
    const juce::String getApplicationName() override { return "StVideos"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void suspended() override {}
    void resumed() override {}
    void systemRequestedQuit() override { quit(); }
    void unhandledException (const std::exception*, const juce::String&, int) override {}
    void shutdown() override { recorder.reset(); }
    void initialise (const juce::String&) override
    {
        auto& c = config();
        c.dir.createDirectory();
        if (! c.only.isEmpty())
            c.scenes.erase (std::remove_if (c.scenes.begin(), c.scenes.end(), [&] (const Scene& s) { return ! c.only.contains (s.name); }), c.scenes.end());
        recorder = std::make_unique<Recorder> (c);
        recorder->startTimerHz (fps);
    }
};

inline juce::JUCEApplicationBase* createApp() { return new App(); }

/** Entry point from a test tool's main(): parses `<flag> <dir> [scene ...]` (flag: "--video" unless the tool already
    uses that for something else) and runs the scenes. */
inline int run (int argc, char** argv, const juce::String& prefix, std::function<std::unique_ptr<juce::AudioProcessor>()> makeProcessor,
                std::vector<Scene> scenes, const juce::String& flag = "--video")
{
    juce::StringArray args (argv + 1, argc - 1);
    const int i = args.indexOf (flag);
    auto& c = config();
    c.dir = juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1]);
    for (int k = i + 2; k < args.size(); ++k) c.only.add (args[k]);
    c.prefix = prefix;
    c.makeProcessor = std::move (makeProcessor);
    c.scenes = std::move (scenes);
    juce::JUCEApplicationBase::createInstance = &createApp;
    return juce::JUCEApplicationBase::main (argc, (const char**) argv);
}
} // namespace st::video
