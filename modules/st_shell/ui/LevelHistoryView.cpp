namespace st
{

HistoryReader::HistoryReader (const LevelHistory& h, int capacity, float releaseDbPerSecond)
    : history (h), releaseDb (releaseDbPerSecond)
{
    // power of two: absolute indices keep mapping to the same slots when the counter wraps
    frames.assign ((size_t) juce::nextPowerOfTwo (juce::jlimit (16, LevelHistory::kSize, capacity)), LevelHistory::Frame {});
}

int HistoryReader::update()
{
    const uint32_t w = history.getWritten();
    const uint32_t cap = (uint32_t) juce::jmin ((int) frames.size(), LevelHistory::kSize - 64);
    if (! primed) { last = w > cap ? w - cap : 0u; primed = true; }
    if (w - last > cap) last = w - cap;
    const float fall = releaseDb > 0.0f ? (float) ((double) releaseDb / juce::jmax (1.0, history.getFrameRate())) : 0.0f;
    int count = 0;
    for (; last != w; ++last, ++count)
    {
        auto f = history.read (last);
        if (fall > 0.0f)
        {
            // fast eased attack (keeps hits sharp, takes the frame-to-frame jitter off the floor), steady release
            auto follow = [fall] (float e, float x) { return x > e ? e + 0.72f * (x - e) : juce::jmax (x, e - fall); };
            env.inDb = follow (env.inDb, f.inDb);
            env.outDb = follow (env.outDb, f.outDb);
            f.inDb = env.inDb;
            f.outDb = env.outDb;
        }
        frames[(size_t) (last % (uint32_t) frames.size())] = f;
        filled = juce::jmin (filled + 1, (int) frames.size());
    }
    return count;
}

LevelHistory::Frame HistoryReader::peakOfNewest (int n) const noexcept
{
    LevelHistory::Frame f;
    n = juce::jmin (n, filled);
    for (int i = 0; i < n; ++i)
    {
        const auto& x = get (i);
        f.inDb = juce::jmax (f.inDb, x.inDb);
        f.outDb = juce::jmax (f.outDb, x.outDb);
        f.detDb = juce::jmax (f.detDb, x.detDb);
        f.grDb = LevelHistory::strongerGain (f.grDb, x.grDb);
    }
    return f;
}

//==============================================================================
double SmoothScroll::update (double dt, uint32_t newestEnd, double frameRate)
{
    const double end = (double) newestEnd;
    if (! started)
    {
        position = end - lag;
        lastEnd = end;
        started = true;
        return position;
    }

    // how bursty the audio is: the lag follows the biggest jump seen recently (one host block), plus a margin
    const double burst = end - lastEnd;
    lastEnd = end;
    lag = juce::jmax (lag * 0.995, juce::jlimit (2.0, 200.0, burst + 2.0));

    const double target = end - lag;
    if (std::abs (target - position) > 400.0) { position = target; return position; }   // jumped (transport, reset)
    if (burst <= 0.0 && position >= end - 1.0) return position;                           // audio stopped: hold still

    position += dt * frameRate;                                       // glide at the frame rate
    position += (target - position) * juce::jmin (1.0, dt * 3.0);    // and drift gently onto the target
    position = juce::jmin (position, end - 1.0);
    return position;
}

//==============================================================================
void HistoryPaths::build (const HistoryReader& r, double position, float left, float right, double fpp,
                          const std::function<float (float)>& levelY, const std::function<float (float)>& grY,
                          float bottom, float grTop)
{
    inFill.clear(); outFill.clear(); outLine.clear(); grLine.clear(); grFill.clear();
    any = false;
    if (r.size() < 2 || right <= left) return;

    // whole frames per bucket; buckets start at multiples of the bucket size (absolute index), so they never shift
    const uint32_t bucket = (uint32_t) juce::jmax (1, juce::roundToInt (fpp));
    const double oldestVisible = position - (double) (right - left) * fpp - (double) bucket;
    const uint32_t first = (uint32_t) juce::jmax ((double) r.begin(), std::floor (juce::jmax (0.0, oldestVisible)));
    const uint32_t lastFrame = (uint32_t) juce::jmax ((double) r.begin(), std::floor (position));   // newest frame drawn
    if (lastFrame < first || lastFrame >= r.end()) return;

    struct Pt { float x, in, out, gr; };
    std::vector<Pt> pts;
    pts.reserve ((size_t) (lastFrame - first) / bucket + 4);
    for (uint32_t b = first - first % bucket; b <= lastFrame; b += bucket)
    {
        LevelHistory::Frame agg;
        uint32_t n = 0;
        for (uint32_t f = juce::jmax (b, first); f < b + bucket && f <= lastFrame; ++f, ++n)
        {
            const auto& x = r.at (f);
            agg.inDb = juce::jmax (agg.inDb, x.inDb);
            agg.outDb = juce::jmax (agg.outDb, x.outDb);
            agg.grDb = LevelHistory::strongerGain (agg.grDb, x.grDb);
        }
        if (n == 0) continue;
        const double centre = (double) b + (double) bucket * 0.5;
        const float x = right - (float) ((position - juce::jmin (centre, position)) / fpp);
        pts.push_back ({ x, levelY (agg.inDb), levelY (agg.outDb), grY (agg.grDb) });
    }
    if (pts.size() < 2) return;

    const float x0 = pts.front().x, x1 = pts.back().x;
    inFill.startNewSubPath (x0, bottom);
    outFill.startNewSubPath (x0, bottom);
    grFill.startNewSubPath (x0, grTop);
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& p = pts[i];
        inFill.lineTo (p.x, p.in);
        outFill.lineTo (p.x, p.out);
        grFill.lineTo (p.x, p.gr);
        if (i == 0) { outLine.startNewSubPath (p.x, p.out); grLine.startNewSubPath (p.x, p.gr); }
        else { outLine.lineTo (p.x, p.out); grLine.lineTo (p.x, p.gr); }
    }
    inFill.lineTo (x1, bottom); inFill.closeSubPath();
    outFill.lineTo (x1, bottom); outFill.closeSubPath();
    grFill.lineTo (x1, grTop); grFill.closeSubPath();
    any = true;
}

//==============================================================================
bool LiveLevels::update (double dt, float releaseSeconds)
{
    const int n = reader.update();
    const auto p = reader.peakOfNewest (n);   // n == 0: no audio -> everything falls back
    const float k = (float) (1.0 - std::exp (-dt / (double) releaseSeconds));
    auto follow = [k] (float& v, float target, bool rising)
    {
        const float before = v;
        v = rising ? target : v + k * (target - v);
        return std::abs (v - before) > 0.02f;
    };
    bool moved = false;
    moved |= follow (grDb, p.grDb, std::abs (p.grDb) > std::abs (grDb) && (p.grDb * grDb >= 0.0f || std::abs (grDb) < 0.05f));
    moved |= follow (detDb, p.detDb, p.detDb > detDb);
    moved |= follow (inDb, p.inDb, p.inDb > inDb);
    moved |= follow (outDb, p.outDb, p.outDb > outDb);
    return moved;
}

} // namespace st
