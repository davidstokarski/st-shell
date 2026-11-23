#pragma once

namespace st
{

/** Level / gain-change history for scrolling dynamics displays (Pro-style level history, multiview modules):
    one frame every few milliseconds, written by the audio thread, read by any number of UI readers without
    consuming it (st::HistoryReader).

    A frame holds the input / output / detector peaks (dB) and the gain change (dB) with the largest magnitude
    in it: negative = gain reduction, positive = upward gain (upward compression / expansion).

    Threading: push() from one (audio) thread; read() / getWritten() from any thread. Lock-free, no allocation. */
class LevelHistory
{
public:
    static constexpr int kSize = 4096;   // ~16 s of frames at 4 ms
    struct Frame { float inDb = -120.0f, outDb = -120.0f, grDb = 0.0f, detDb = -120.0f; };

    void push (const Frame& f) noexcept
    {
        const uint32_t w = written.load (std::memory_order_relaxed);
        auto& slot = frames[(size_t) (w & (kSize - 1))];
        slot[0].store (f.inDb, std::memory_order_relaxed);
        slot[1].store (f.outDb, std::memory_order_relaxed);
        slot[2].store (f.grDb, std::memory_order_relaxed);
        slot[3].store (f.detDb, std::memory_order_relaxed);
        written.store (w + 1, std::memory_order_release);
    }

    /** Frames written so far (absolute index of the next frame). */
    uint32_t getWritten() const noexcept { return written.load (std::memory_order_acquire); }

    Frame read (uint32_t index) const noexcept
    {
        const auto& slot = frames[(size_t) (index & (kSize - 1))];
        return { slot[0].load (std::memory_order_relaxed), slot[1].load (std::memory_order_relaxed),
                 slot[2].load (std::memory_order_relaxed), slot[3].load (std::memory_order_relaxed) };
    }

    void setFrameRate (double hz) noexcept { frameRate.store (hz); }
    double getFrameRate() const noexcept { return frameRate.load(); }

    /** The gain change with the larger magnitude (frame aggregation: deepest reduction or highest boost). */
    static float strongerGain (float a, float b) noexcept { return std::abs (b) > std::abs (a) ? b : a; }

private:
    std::array<std::array<std::atomic<float>, 4>, kSize> frames {};
    std::atomic<uint32_t> written { 0 };
    std::atomic<double> frameRate { 250.0 };
};

} // namespace st
