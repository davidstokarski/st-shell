#pragma once

namespace st
{

/** Frame-rate independent eased value for UI motion (exponential approach). */
struct Animated
{
    float current = 0.0f, target = 0.0f;

    void snap (float v) noexcept { current = target = v; }
    void setTarget (float v) noexcept { target = v; }

    /** Advances by dt seconds; `speed` ~ 1/time-constant. Returns true while still moving. */
    bool tick (double dt, float speed = 14.0f) noexcept
    {
        const float k = 1.0f - std::exp (-speed * (float) dt);
        current += (target - current) * k;
        if (std::abs (target - current) < 1.0e-4f) { current = target; return false; }
        return true;
    }

    operator float() const noexcept { return current; }
};

/** Calls `onFrame(dt)` once per display refresh while the component is showing: in step with the monitor
    (60 / 120 / 144 Hz ...), so scrolling displays, analysers and meters move on every refresh. With the shared
    "Cap frame rate" setting on (userSettings::isFrameRateCapped) it fires at most ~60 times per second.
    Falls back to a 60 Hz timer when no vblank arrives (offscreen rendering, some hosts).

    `dt` is the real time since the previous callback (seconds, at most 0.1): advance animation and ballistics by
    it, never by a fixed step, so motion has the same speed at any refresh rate. */
class FrameClock : private juce::Timer
{
public:
    FrameClock (juce::Component& c, std::function<void (double dt)> onFrame)
        : callback (std::move (onFrame)),
          attachment (&c, [this] (double ts)
          {
              lastVBlankMs = juce::Time::getMillisecondCounterHiRes();
              const double dt = lastTs > 0.0 ? juce::jlimit (0.0, 0.1, ts - lastTs) : 1.0 / 60.0;
              lastTs = ts;
              if (dt > 0.0) measuredHz += ((1.0 / dt) - measuredHz) * 0.05;
              fire (dt);
          })
    {
        startTimerHz (60);
    }

    ~FrameClock() override { stopTimer(); }

    /** The measured display refresh rate (Hz, smoothed), or 60 before any vblank arrived. */
    double getDisplayRate() const noexcept { return measuredHz; }

private:
    std::function<void (double)> callback;
    double lastTs = 0.0, lastVBlankMs = 0.0, lastTimerMs = 0.0, measuredHz = 60.0;
    juce::VBlankAttachment attachment;

    double pending = 0.0;

    void fire (double dt)
    {
        pending += dt;
        if (userSettings::isFrameRateCapped() && pending < 1.0 / 62.0) return;   // capped: ~60 updates per second
        const double step = juce::jmin (pending, 0.1);
        pending = 0.0;
        if (callback) callback (step);
    }

    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const double dt = lastTimerMs > 0.0 ? juce::jlimit (0.0, 0.1, (now - lastTimerMs) * 0.001) : 1.0 / 60.0;
        lastTimerMs = now;
        if (now - lastVBlankMs > 100.0)
            fire (dt);
    }
};

} // namespace st
