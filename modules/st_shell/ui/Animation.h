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

/** Calls `onFrame(dt)` once per display refresh while the component is showing.
    Falls back to a 60 Hz timer when no vblank arrives (offscreen rendering, some hosts). */
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
              fire (dt);
          })
    {
        startTimerHz (60);
    }

    ~FrameClock() override { stopTimer(); }

private:
    std::function<void (double)> callback;
    double lastTs = 0.0, lastVBlankMs = 0.0, lastTimerMs = 0.0;
    juce::VBlankAttachment attachment;

    double pending = 0.0;

    void fire (double dt)
    {
        pending += dt;
        if (pending < 1.0 / 62.0) return;   // never more than ~60 updates per second
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
