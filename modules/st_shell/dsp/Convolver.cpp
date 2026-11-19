namespace st::dsp
{

PartitionedConvolver::PartitionedConvolver() = default;
PartitionedConvolver::~PartitionedConvolver() = default;

void PartitionedConvolver::prepare (int partitionSize, int maxIrLength, int numChannels, int maxIrs)
{
    P = juce::nextPowerOfTwo (std::max (16, partitionSize));
    fftSize = 2 * P;
    numBins = P + 1;
    numParts = std::max (1, (maxIrLength + P - 1) / P);
    numCh = std::clamp (numChannels, 1, 2);
    maxIrCount = std::clamp (maxIrs, 1, 4);

    const int order = juce::roundToInt (std::log2 ((double) fftSize));
    fftAudio = std::make_unique<juce::dsp::FFT> (order);
    fftWriter = std::make_unique<juce::dsp::FFT> (order);

    for (auto& s : slots)
    {
        s.spectra.assign ((size_t) (maxIrCount * numParts * numBins), Cpx {});
        s.numIrs = 0;
        s.usedParts = 0;
        s.routing = Routing::mono;
    }
    shared.store (2);
    writerSlot = 3; activeSlot = 0; prevSlot = 1;

    for (int c = 0; c < 2; ++c)
    {
        inBuf[(size_t) c].assign ((size_t) fftSize, 0.0f);
        outBuf[(size_t) c].assign ((size_t) P, 0.0f);
        fdl[(size_t) c].assign ((size_t) (numParts * numBins), Cpx {});
        acc[(size_t) c].assign ((size_t) numBins, Cpx {});
        accOld[(size_t) c].assign ((size_t) numBins, Cpx {});
    }
    work.assign ((size_t) (2 * fftSize), 0.0f);
    writerWork.assign ((size_t) (2 * fftSize), 0.0f);
    fadeOld.assign ((size_t) P, 0.0f);
    fdlHead = 0;
    fill = 0;
}

void PartitionedConvolver::reset() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        std::fill (inBuf[(size_t) c].begin(), inBuf[(size_t) c].end(), 0.0f);
        std::fill (outBuf[(size_t) c].begin(), outBuf[(size_t) c].end(), 0.0f);
        std::fill (fdl[(size_t) c].begin(), fdl[(size_t) c].end(), Cpx {});
    }
    fill = 0;
}

void PartitionedConvolver::writeImpulseResponses (Routing routing, const float* const* irs, int length)
{
    if (P == 0) return;

    int count = routing == Routing::mono ? 1 : (routing == Routing::diagonal ? 2 : 4);
    jassert (count <= maxIrCount);
    count = std::min (count, maxIrCount);

    auto& slot = slots[(size_t) writerSlot];
    length = std::clamp (length, 0, numParts * P);
    const int used = std::max (1, (length + P - 1) / P);

    for (int i = 0; i < count; ++i)
    {
        for (int p = 0; p < used; ++p)
        {
            std::fill (writerWork.begin(), writerWork.end(), 0.0f);
            const int start = p * P;
            const int n = std::max (0, std::min (P, length - start));
            for (int k = 0; k < n; ++k)
                writerWork[(size_t) k] = irs[i][start + k];

            fftWriter->performRealOnlyForwardTransform (writerWork.data(), true);
            auto* dst = slot.spectra.data() + (size_t) ((i * numParts + p) * numBins);
            for (int b = 0; b < numBins; ++b)
                dst[b] = Cpx (writerWork[(size_t) (2 * b)], writerWork[(size_t) (2 * b + 1)]);
        }
    }

    slot.routing = routing;
    slot.numIrs = count;
    slot.usedParts = used;

    writerSlot = shared.exchange (writerSlot | kFresh, std::memory_order_acq_rel) & 3;
}

void PartitionedConvolver::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (P == 0) return;
    const int nc = std::min (numCh, numChannels);

    int pos = 0;
    while (pos < numSamples)
    {
        const int k = std::min (P - fill, numSamples - pos);
        for (int c = 0; c < nc; ++c)
        {
            float* io = channels[c] + pos;
            std::copy (io, io + k, inBuf[(size_t) c].data() + P + fill);
            std::copy (outBuf[(size_t) c].data() + fill, outBuf[(size_t) c].data() + fill + k, io);
        }
        fill += k;
        pos += k;

        if (fill == P)
        {
            processPartition();
            fill = 0;
        }
    }
}

void PartitionedConvolver::processPartition() noexcept
{
    fdlHead = (fdlHead + 1) % numParts;

    for (int c = 0; c < numCh; ++c)
    {
        std::copy (inBuf[(size_t) c].begin(), inBuf[(size_t) c].end(), work.begin());
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        fftAudio->performRealOnlyForwardTransform (work.data(), true);

        auto* dst = fdl[(size_t) c].data() + (size_t) (fdlHead * numBins);
        for (int b = 0; b < numBins; ++b)
            dst[b] = Cpx (work[(size_t) (2 * b)], work[(size_t) (2 * b + 1)]);

        // slide the input window
        std::copy (inBuf[(size_t) c].begin() + P, inBuf[(size_t) c].end(), inBuf[(size_t) c].begin());
    }

    bool crossfade = false;
    if ((shared.load (std::memory_order_acquire) & kFresh) != 0)
    {
        const int fresh = shared.exchange (prevSlot, std::memory_order_acq_rel) & 3;
        prevSlot = activeSlot;
        activeSlot = fresh;
        crossfade = true;
    }

    accumulate (slots[(size_t) activeSlot], acc);
    if (crossfade)
        accumulate (slots[(size_t) prevSlot], accOld);

    for (int c = 0; c < numCh; ++c)
    {
        inverse (acc[(size_t) c], outBuf[(size_t) c].data());
        if (crossfade)
        {
            inverse (accOld[(size_t) c], fadeOld.data());
            auto* o = outBuf[(size_t) c].data();
            const float inv = 1.0f / (float) P;
            for (int i = 0; i < P; ++i)
            {
                const float t = (float) (i + 1) * inv;
                o[i] = fadeOld[(size_t) i] + (o[i] - fadeOld[(size_t) i]) * t;
            }
        }
    }
}

void PartitionedConvolver::accumulate (const Slot& slot, std::array<std::vector<Cpx>, 2>& out) noexcept
{
    for (int c = 0; c < numCh; ++c)
        std::fill (out[(size_t) c].begin(), out[(size_t) c].end(), Cpx {});

    if (slot.numIrs == 0)
    {
        // pass-through: the newest input partition
        for (int c = 0; c < numCh; ++c)
        {
            const auto* x = fdl[(size_t) c].data() + (size_t) (fdlHead * numBins);
            for (int b = 0; b < numBins; ++b) out[(size_t) c][(size_t) b] = x[b];
        }
        return;
    }

    auto mac = [this] (Cpx* y, const Cpx* x, const Cpx* h) noexcept
    {
        auto* yf = reinterpret_cast<float*> (y);
        const auto* xf = reinterpret_cast<const float*> (x);
        const auto* hf = reinterpret_cast<const float*> (h);
        for (int b = 0; b < numBins; ++b)
        {
            const float xr = xf[2 * b], xi = xf[2 * b + 1], hr = hf[2 * b], hi = hf[2 * b + 1];
            yf[2 * b]     += xr * hr - xi * hi;
            yf[2 * b + 1] += xr * hi + xi * hr;
        }
    };

    auto H = [&] (int ir, int part) { return slot.spectra.data() + (size_t) ((ir * numParts + part) * numBins); };
    auto X = [&] (int ch, int delay) { return fdl[(size_t) ch].data() + (size_t) (((fdlHead - delay + numParts) % numParts) * numBins); };

    for (int k = 0; k < slot.usedParts; ++k)
    {
        switch (slot.routing)
        {
            case Routing::mono:
                for (int c = 0; c < numCh; ++c) mac (out[(size_t) c].data(), X (c, k), H (0, k));
                break;
            case Routing::diagonal:
                for (int c = 0; c < numCh; ++c) mac (out[(size_t) c].data(), X (c, k), H (std::min (c, slot.numIrs - 1), k));
                break;
            case Routing::matrix:
                if (numCh == 1)
                    mac (out[0].data(), X (0, k), H (0, k));
                else
                {
                    mac (out[0].data(), X (0, k), H (0, k));
                    mac (out[0].data(), X (1, k), H (1, k));
                    mac (out[1].data(), X (0, k), H (2, k));
                    mac (out[1].data(), X (1, k), H (3, k));
                }
                break;
        }
    }
}

void PartitionedConvolver::inverse (const std::vector<Cpx>& spectrum, float* dest) noexcept
{
    for (int b = 0; b < numBins; ++b)
    {
        work[(size_t) (2 * b)] = spectrum[(size_t) b].real();
        work[(size_t) (2 * b + 1)] = spectrum[(size_t) b].imag();
    }
    // conjugate-symmetric upper half (needed by some FFT back-ends)
    for (int b = 1; b < P; ++b)
    {
        work[(size_t) (2 * (fftSize - b))] = spectrum[(size_t) b].real();
        work[(size_t) (2 * (fftSize - b) + 1)] = -spectrum[(size_t) b].imag();
    }
    fftAudio->performRealOnlyInverseTransform (work.data());
    std::copy (work.begin() + P, work.begin() + fftSize, dest);
}

} // namespace st::dsp
