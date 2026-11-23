#pragma once

namespace st
{

/** Reads a LevelHistory without consuming it and keeps the latest frames in a local ring, addressed by the
    history's absolute frame index.

    With `releaseDbPerSecond` > 0 the input / output levels are turned into a peak envelope as they arrive (fast
    attack, constant-rate release): raw 4 ms peaks of drums jump by several dB from frame to frame, an envelope
    reads as one calm line. Gain change is already smooth (compressor ballistics) and is kept as is. */
class HistoryReader
{
public:
    explicit HistoryReader (const LevelHistory& h, int capacity = 2048, float releaseDbPerSecond = 0.0f);
    /** Pulls new frames; returns how many arrived. */
    int update();
    /** Frames received so far (up to the capacity). */
    int size() const noexcept { return filled; }
    /** Absolute index one past the newest frame, and of the oldest frame still held. */
    uint32_t end() const noexcept { return last; }
    uint32_t begin() const noexcept { return last - (uint32_t) filled; }
    /** Frame by absolute index (must be in [begin, end)). */
    const LevelHistory::Frame& at (uint32_t index) const noexcept { return frames[(size_t) (index % (uint32_t) frames.size())]; }
    /** Frame `ago` frames before the newest (0 = newest). */
    const LevelHistory::Frame& get (int ago) const noexcept { return at (last - 1u - (uint32_t) ago); }
    double getFrameRate() const noexcept { return history.getFrameRate(); }
    /** Peaks (max levels, strongest gain change) of the newest n frames. */
    LevelHistory::Frame peakOfNewest (int n) const noexcept;

private:
    const LevelHistory& history;
    std::vector<LevelHistory::Frame> frames;
    int filled = 0;
    uint32_t last = 0;
    bool primed = false;
    float releaseDb = 0.0f;
    LevelHistory::Frame env;
};

//==============================================================================
/** A display clock for a scrolling history: advances smoothly at the frame rate every UI frame and follows the
    audio (which arrives in bursts, one host block at a time) a little behind, correcting drift gently. Frames are
    drawn at `position`, so the picture glides instead of stepping by a block at a time. */
class SmoothScroll
{
public:
    /** Returns the display position (absolute frame index, fractional) for this UI frame. */
    double update (double dt, uint32_t newestEnd, double frameRate);
    double getPosition() const noexcept { return position; }

private:
    double position = 0.0, lastEnd = -1.0, lag = 6.0;
    bool started = false;
};

//==============================================================================
/** Paths of a level history: input / output fills, the output line and the gain-change trace (+ its fill).
    Buckets of frames are anchored to absolute frame indices, so the shape doesn't shimmer as it scrolls. */
struct HistoryPaths
{
    juce::Path inFill, outFill, outLine, grLine, grFill;
    bool any = false;

    /** levelY / grY map dB to y (grY gets the signed gain change). x of absolute frame f =
        right - (position - f) / framesPerPixel. The gain fill closes towards `grTop`. */
    void build (const HistoryReader&, double position, float left, float right, double framesPerPixel,
                const std::function<float (float)>& levelY, const std::function<float (float)>& grY,
                float bottom, float grTop);
};

//==============================================================================
/** Live values for meters / readouts, read from the history (so no hit between two UI frames is missed) with
    display ballistics: instant attack, smooth release. */
class LiveLevels
{
public:
    explicit LiveLevels (const LevelHistory& h) : reader (h, 256, 0.0f) {}
    /** Returns true when a value moved visibly. */
    bool update (double dt, float releaseSeconds = 0.18f);
    float grDb = 0.0f, detDb = -120.0f, inDb = -120.0f, outDb = -120.0f;

private:
    HistoryReader reader;
};

} // namespace st
