namespace st::dsp
{

//==============================================================================
// BiquadCoeffs
//==============================================================================
BiquadCoeffs BiquadCoeffs::design (BiquadType type, double sampleRate, double freq, double q, double gainDb) noexcept
{
    const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double f  = std::clamp (freq, 1.0e-3, 0.4999 * fs);
    q = std::max (q, 1.0e-4);

    const double w0 = kTwoPi * f / fs;
    const double cw = std::cos (w0);
    const double sw = std::sin (w0);
    const double sh = std::sin (0.5 * w0);
    const double oneMinusCos = 2.0 * sh * sh; // accurate for tiny w0
    const double alpha = sw / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);

    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (type)
    {
        case BiquadType::lowPass:
            b0 = 0.5 * oneMinusCos; b1 = oneMinusCos; b2 = b0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        case BiquadType::highPass:
            b0 = 0.5 * (1.0 + cw); b1 = -(1.0 + cw); b2 = b0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        case BiquadType::bandPass:
            b0 = alpha; b1 = 0.0; b2 = -alpha;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        case BiquadType::notch:
            b0 = 1.0; b1 = -2.0 * cw; b2 = 1.0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        case BiquadType::allPass:
            b0 = 1.0 - alpha; b1 = -2.0 * cw; b2 = 1.0 + alpha;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        case BiquadType::peak:
            b0 = 1.0 + alpha * A; b1 = -2.0 * cw; b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A; a1 = -2.0 * cw; a2 = 1.0 - alpha / A;
            break;
        case BiquadType::lowShelf:
        {
            const double sa = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cw + sa);
            b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - sa);
            a0 = (A + 1) + (A - 1) * cw + sa;
            a1 = -2 * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - sa;
            break;
        }
        case BiquadType::highShelf:
        {
            const double sa = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + sa);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - sa);
            a0 = (A + 1) - (A - 1) * cw + sa;
            a1 = 2 * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - sa;
            break;
        }
    }

    const double inv = 1.0 / a0;
    return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
}

std::complex<double> BiquadCoeffs::response (double hz, double sampleRate) const noexcept
{
    const double w = kTwoPi * hz / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
}

void Biquad::process (float* data, int numSamples, int channel) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    double z1 = s1[(size_t) channel], z2 = s2[(size_t) channel];
    const auto c = coeffs;
    for (int i = 0; i < numSamples; ++i)
    {
        const double x = data[i];
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        data[i] = (float) y;
    }
    s1[(size_t) channel] = flushDenormal (z1);
    s2[(size_t) channel] = flushDenormal (z2);
}

void Biquad::process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    const int chans = std::min (numCh, buffer.getNumChannels());
    for (int ch = 0; ch < chans; ++ch)
        process (buffer.getWritePointer (ch), numSamples, ch);
}

//==============================================================================
// Svf
//==============================================================================
SvfCoeffs SvfCoeffs::design (SvfMode mode, double sampleRate, double freq, double q, double gainDb) noexcept
{
    const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double f  = std::clamp (freq, 1.0e-3, 0.4999 * fs);
    q = std::max (q, 1.0e-4);

    SvfCoeffs c;
    double g = std::tan (kPi * f / fs);
    double k = 1.0 / q;
    const double A = std::pow (10.0, gainDb / 40.0);

    switch (mode)
    {
        case SvfMode::lowPass:   c.m0 = 0;  c.m1 = 0;  c.m2 = 1;  break;
        case SvfMode::bandPass:  c.m0 = 0;  c.m1 = k;  c.m2 = 0;  break;
        case SvfMode::highPass:  c.m0 = 1;  c.m1 = -k; c.m2 = -1; break;
        case SvfMode::notch:     c.m0 = 1;  c.m1 = -k; c.m2 = 0;  break;
        case SvfMode::peak:      c.m0 = -1; c.m1 = k;  c.m2 = 2;  break; // low - high
        case SvfMode::allPass:   c.m0 = 1;  c.m1 = -2 * k; c.m2 = 0; break;
        case SvfMode::bell:
            k = 1.0 / (q * A);
            c.m0 = 1; c.m1 = k * (A * A - 1.0); c.m2 = 0;
            break;
        case SvfMode::lowShelf:
            g /= std::sqrt (A);
            c.m0 = 1; c.m1 = k * (A - 1.0); c.m2 = A * A - 1.0;
            break;
        case SvfMode::highShelf:
            g *= std::sqrt (A);
            c.m0 = A * A; c.m1 = k * (1.0 - A) * A; c.m2 = 1.0 - A * A;
            break;
    }

    c.g = g;
    c.k = k;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    return c;
}

std::complex<double> SvfCoeffs::response (double hz, double sampleRate) const noexcept
{
    // Digital response = analog prototype at the bilinear-warped frequency.
    const double wa = std::tan (kPi * std::clamp (hz, 0.0, 0.4999999 * sampleRate) / sampleRate) / g;
    const std::complex<double> s (0.0, wa);
    const std::complex<double> den = s * s + k * s + 1.0;
    const std::complex<double> v1 = s / den, v2 = 1.0 / den;
    return m0 + m1 * v1 + m2 * v2;
}

void Svf::process (float* data, int numSamples, int channel) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < numSamples; ++i)
        data[i] = processSample (channel, data[i]);
    ic1[(size_t) channel] = flushDenormal (ic1[(size_t) channel]);
    ic2[(size_t) channel] = flushDenormal (ic2[(size_t) channel]);
}

void Svf::process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    const int chans = std::min (numCh, buffer.getNumChannels());
    for (int ch = 0; ch < chans; ++ch)
        process (buffer.getWritePointer (ch), numSamples, ch);
}

//==============================================================================
// OnePole
//==============================================================================
void OnePole::process (float* data, int numSamples, int channel) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < numSamples; ++i)
        data[i] = processSample (channel, data[i]);
    state[(size_t) channel] = flushDenormal (state[(size_t) channel]);
}

void OnePole::process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    const int chans = std::min (numCh, buffer.getNumChannels());
    for (int ch = 0; ch < chans; ++ch)
        process (buffer.getWritePointer (ch), numSamples, ch);
}

std::complex<double> OnePole::getResponse (OnePoleType type, double sampleRate, double cutoffHz, double evalHz) noexcept
{
    const double g = std::tan (kPi * std::clamp (cutoffHz, 0.01, 0.49 * sampleRate) / sampleRate);
    const std::complex<double> s (0.0, std::tan (kPi * std::clamp (evalHz, 0.0, 0.4999999 * sampleRate) / sampleRate) / g);
    const std::complex<double> lp = 1.0 / (s + 1.0);
    switch (type)
    {
        case OnePoleType::lowPass:  return lp;
        case OnePoleType::highPass: return 1.0 - lp;
        case OnePoleType::allPass:  return 2.0 * lp - 1.0;
    }
    return lp;
}

//==============================================================================
// TiltFilter
//==============================================================================
void TiltFilter::updateCoeffs (float tilt) noexcept
{
    lastTilt = tilt;
    const double hiDb = (double) tilt * (double) maxGainDb;
    const double gHi = std::pow (10.0, hiDb / 20.0);
    const double wp = std::tan (kPi * std::clamp (pivot, 1.0, 0.49 * fs) / fs) * gHi; // warped pole
    G = wp / (1.0 + wp);
    gainHi = gHi;
    gainLo = 1.0 / gHi;
}

std::complex<double> TiltFilter::getResponse (double sampleRate, double pivotHz, float tilt, float maxGainDb, double evalHz) noexcept
{
    const double gHi = std::pow (10.0, (double) std::clamp (tilt, -1.0f, 1.0f) * (double) maxGainDb / 20.0);
    const double wp = std::tan (kPi * std::clamp (pivotHz, 1.0, 0.49 * sampleRate) / sampleRate) * gHi;
    const std::complex<double> s (0.0, std::tan (kPi * std::clamp (evalHz, 0.0, 0.4999999 * sampleRate) / sampleRate));
    const std::complex<double> lp = wp / (s + wp);
    return gHi * (1.0 - lp) + lp / gHi;
}

void TiltFilter::process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    const int chans = std::min (numCh, buffer.getNumChannels());

    if (! smoother.isSmoothing())
    {
        if (! juce::exactlyEqual (smoother.getCurrent(), lastTilt))
            updateCoeffs (smoother.getCurrent());
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = processSample (ch, d[i]);
        }
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            updateCoeffs (smoother.getNext());
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[i] = processSample (ch, d[i]);
            }
        }
    }

    for (int ch = 0; ch < chans; ++ch)
        state[(size_t) ch] = flushDenormal (state[(size_t) ch]);
}

void TiltFilter::process (juce::AudioBuffer<float>& buffer, int numSamples, const float* tiltPerSample) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    const int chans = std::min (numCh, buffer.getNumChannels());

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = std::clamp (tiltPerSample[i], -1.0f, 1.0f);
        if (! juce::exactlyEqual (t, lastTilt))
            updateCoeffs (t);
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = processSample (ch, d[i]);
        }
    }
    smoother.setCurrentAndTarget (lastTilt);

    for (int ch = 0; ch < chans; ++ch)
        state[(size_t) ch] = flushDenormal (state[(size_t) ch]);
}

} // namespace st::dsp
