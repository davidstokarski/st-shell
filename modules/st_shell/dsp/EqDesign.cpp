namespace st::dsp
{

std::complex<double> AnalogSection::response (double w) const noexcept
{
    if (firstOrder)
        return std::complex<double> (n0, n1 * w) / std::complex<double> (d0, w);
    return std::complex<double> (n0 - n2 * w * w, n1 * w) / std::complex<double> (d0 - w * w, d1 * w);
}

namespace eq
{

namespace
{
    constexpr double kBilinearLimit = 0.3;   // section frequency * T below which the prewarped bilinear transform is used

    struct Builder
    {
        Analog a;
        double w0 = 1.0;

        void second (double n2, double n1, double n0, double d1, double d0, bool ap = false) noexcept
        {
            if (a.numSections >= kMaxSections) return;
            auto& s = a.sections[(size_t) a.numSections++];
            s.firstOrder = false;
            s.allPass = ap;
            s.n2 = n2; s.n1 = n1 * w0; s.n0 = n0 * w0 * w0;
            s.d1 = d1 * w0; s.d0 = d0 * w0 * w0;
        }

        void first (double n1, double n0, double d0, bool ap = false) noexcept
        {
            if (a.numSections >= kMaxSections) return;
            auto& s = a.sections[(size_t) a.numSections++];
            s.firstOrder = true;
            s.allPass = ap;
            s.n2 = 0.0; s.n1 = n1; s.n0 = n0 * w0;
            s.d1 = 1.0; s.d0 = d0 * w0;
        }
    };

    double butterworthDamping (int k, int order) noexcept   // k = 1..order/2
    {
        return 2.0 * std::sin ((2.0 * k - 1.0) * kPi / (2.0 * order));
    }

    /** Butterworth-type shelf with DC gain `g` (linear) and HF gain 1, order N, resonance via q. */
    void lowShelfSections (Builder& b, double g, int order, double q) noexcept
    {
        const double ga = std::pow (g, -1.0 / (2.0 * order));   // pole radius
        const double gb = std::pow (g, 1.0 / (2.0 * order));    // zero radius
        const double reso = 0.70710678118654752 / std::clamp (q, 0.025, 40.0);

        for (int k = 1; k <= order / 2; ++k)
        {
            double damp = butterworthDamping (k, order);
            if (k == 1) damp *= reso;
            b.second (1.0, damp * gb, gb * gb, damp * ga, ga * ga);
        }
        if (order % 2 == 1)
            b.first (1.0, gb, ga);
    }
}

bool hasGain (EqShape s) noexcept
{
    return s == EqShape::bell || s == EqShape::lowShelf || s == EqShape::highShelf || s == EqShape::tiltShelf || s == EqShape::flatTilt;
}

bool hasSlope (EqShape s) noexcept
{
    return s != EqShape::bell && s != EqShape::flatTilt;
}

bool hasQ (EqShape s) noexcept
{
    return s != EqShape::flatTilt;
}

Analog designAnalog (const EqBandSpec& spec) noexcept
{
    Builder b;
    b.w0 = kTwoPi * std::clamp (spec.freq, 1.0, 1.0e6);
    const int order = std::clamp (spec.order, 1, kMaxOrder);
    const double q = std::clamp (spec.q, 0.025, 40.0);
    const double gainDb = std::clamp (spec.gainDb, -60.0, 60.0);

    switch (spec.shape)
    {
        case EqShape::bell:
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            b.second (1.0, A / q, 1.0, 1.0 / (A * q), 1.0);
            break;
        }

        case EqShape::lowShelf:
            lowShelfSections (b, std::pow (10.0, gainDb / 20.0), order, q);
            break;

        case EqShape::highShelf:
        case EqShape::tiltShelf:
        {
            // High shelf = G * lowShelf(1/G); tilt shelf splits the gain around the pivot.
            const double g = std::pow (10.0, gainDb / 20.0);
            lowShelfSections (b, 1.0 / g, order, q);
            b.a.gain = spec.shape == EqShape::tiltShelf ? std::sqrt (g) : g;
            break;
        }

        case EqShape::lowCut:
        case EqShape::highCut:
        {
            const bool hp = spec.shape == EqShape::lowCut;
            const double reso = 0.70710678118654752 / q;
            for (int k = 1; k <= order / 2; ++k)
            {
                double damp = butterworthDamping (k, order);
                if (k == 1) damp *= reso;
                if (hp) b.second (1.0, 0.0, 0.0, damp, 1.0);
                else    b.second (0.0, 0.0, 1.0, damp, 1.0);
            }
            if (order % 2 == 1)
            {
                if (hp) b.first (1.0, 0.0, 1.0);
                else    b.first (0.0, 1.0, 1.0);
            }
            break;
        }

        case EqShape::notch:
            for (int k = 0; k < std::max (1, order / 2); ++k)
                b.second (1.0, 0.0, 1.0, 1.0 / q, 1.0);
            break;

        case EqShape::bandPass:
            for (int k = 0; k < std::max (1, order / 2); ++k)
                b.second (0.0, 1.0 / q, 0.0, 1.0 / q, 1.0);
            break;

        case EqShape::allPass:
            if (order == 1)
                b.first (1.0, -1.0, 1.0, true);
            else
                for (int k = 0; k < order / 2; ++k)
                    b.second (1.0, -1.0 / q, 1.0, 1.0 / q, 1.0, true);
            break;

        case EqShape::flatTilt:
        {
            // Constant slope across the spectrum: alternating real poles / zeros spaced 1.5 octaves apart
            // (zero = pole * r^-alpha gives 6*alpha dB/oct on average). gainDb = tilt over 5 octaves.
            const double slope = std::clamp (gainDb / 5.0, -6.0, 6.0);   // dB/oct
            const double alpha = slope / 6.0;
            const double r = std::exp2 (1.5);
            b.w0 = 1.0;
            double p = kTwoPi * 3.0;
            constexpr int pairs = 10;
            for (int k = 0; k < pairs; k += 2)
            {
                const double p1 = p, p2 = p * r;
                const double z1 = p1 * std::pow (r, -alpha), z2 = p2 * std::pow (r, -alpha);
                b.second (1.0, z1 + z2, z1 * z2, p1 + p2, p1 * p2);
                p *= r * r;
            }
            // 0 dB at the pivot
            b.a.gain = 1.0;
            const double m = b.a.magnitude (std::clamp (spec.freq, 1.0, 1.0e5));
            b.a.gain = m > 1.0e-12 ? 1.0 / m : 1.0;
            break;
        }
    }

    return b.a;
}

std::complex<double> Analog::response (double hz) const noexcept
{
    const double w = kTwoPi * hz;
    std::complex<double> h (gain, 0.0);
    for (int i = 0; i < numSections; ++i)
        h *= sections[(size_t) i].response (w);
    return h;
}

double Analog::magnitudeDb (double hz) const noexcept
{
    return 20.0 * std::log10 (std::max (1.0e-12, magnitude (hz)));
}

std::complex<double> Digital::response (double hz, double sampleRate) const noexcept
{
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < numSections; ++i)
        h *= sections[(size_t) i].response (hz, sampleRate);
    return h;
}

namespace
{
    double safeDb (double mag) noexcept { return 20.0 * std::log10 (std::max (1.0e-15, mag)); }

    /** Matched design of a section oriented so that its poles are the "easier" part (see matchSection). */
    BiquadCoeffs matchCore (const AnalogSection& s, double fs) noexcept
    {
        const double T = 1.0 / fs;
        BiquadCoeffs c;

        if (s.firstOrder)
        {
            const double p = std::max (1.0e-9, s.d0);
            const double wz = std::abs (s.n1) > 1.0e-15 ? std::abs (s.n0 / s.n1) : 0.0;
            const double wMax = std::max (p, wz);
            const double settled = std::abs (safeDb (std::abs (s.response (kPi * fs))) - safeDb (std::abs (s.n1)));

            if ((wMax * T < kBilinearLimit && settled < 0.05) || p * T > 0.95 * kPi)
            {
                const double K = p * T < 0.95 * kPi ? p / std::tan (0.5 * p * T) : 2.0 * fs;
                const double a0 = K + s.d0, a1 = s.d0 - K;
                c.b0 = (s.n1 * K + s.n0) / a0;
                c.b1 = (s.n0 - s.n1 * K) / a0;
                c.a1 = a1 / a0;
                return c;
            }

            c.a1 = -std::exp (-p * T);
            if (s.allPass)
            {
                c.b0 = c.a1; c.b1 = 1.0;
                return c;
            }
            const double h0 = std::abs (s.n0 / s.d0);
            const double hn = std::abs (s.response (kPi * fs));
            const double sum = h0 * (1.0 + c.a1), diff = hn * (1.0 - c.a1);
            c.b0 = 0.5 * (sum + diff);
            c.b1 = 0.5 * (sum - diff);
            return c;
        }

        const double wp = std::sqrt (std::max (1.0e-18, s.d0));
        const double zeta = s.d1 / (2.0 * wp);
        const double wz = std::abs (s.n2) > 1.0e-15 ? std::sqrt (std::abs (s.n0 / s.n2))
                        : (std::abs (s.n1) > 1.0e-15 ? std::abs (s.n0 / s.n1) : 0.0);
        const double wMax = std::max (wp, wz);

        auto bilinear = [&] (double K, bool numeratorToo)
        {
            const double K2 = K * K;
            const double a0 = K2 + s.d1 * K + s.d0;
            c.a1 = 2.0 * (s.d0 - K2) / a0;
            c.a2 = (K2 - s.d1 * K + s.d0) / a0;
            if (numeratorToo)
            {
                c.b0 = (s.n2 * K2 + s.n1 * K + s.n0) / a0;
                c.b1 = 2.0 * (s.n0 - s.n2 * K2) / a0;
                c.b2 = (s.n2 * K2 - s.n1 * K + s.n0) / a0;
            }
        };

        // Low sections whose response has fully settled by Nyquist: the prewarped bilinear transform is
        // exact enough and numerically best conditioned.
        const double settled = std::abs (safeDb (std::abs (s.response (kPi * fs))) - safeDb (std::abs (s.n2)));
        if (wMax * T < kBilinearLimit && settled < 0.05)
        {
            bilinear (wp / std::tan (0.5 * wp * T), true);
            return c;
        }

        // Poles: impulse invariance (exact pole positions) unless the resonance lies beyond Nyquist.
        if (zeta < 1.0)
        {
            const double wd = wp * T * std::sqrt (1.0 - zeta * zeta);
            if (wd < 0.95 * kPi)
            {
                const double e = std::exp (-zeta * wp * T);
                c.a1 = -2.0 * e * std::cos (wd);
                c.a2 = e * e;
            }
            else
                bilinear (2.0 * fs, false);
        }
        else
        {
            const double r = std::sqrt (zeta * zeta - 1.0);
            const double e1 = std::exp (-(zeta - r) * wp * T), e2 = std::exp (-(zeta + r) * wp * T);
            c.a1 = -(e1 + e2);
            c.a2 = e1 * e2;
        }

        if (s.allPass)
        {
            c.b0 = c.a2; c.b1 = c.a1; c.b2 = 1.0;
            return c;
        }

        const double A0 = std::pow (1.0 + c.a1 + c.a2, 2.0);
        const double A1 = std::pow (1.0 - c.a1 + c.a2, 2.0);
        const double A2 = -4.0 * c.a2;
        const double h0 = std::norm (s.response (0.0));
        const double hn = std::norm (s.response (kPi * fs));

        const double wmAnalog = wz > 0.0 ? std::sqrt (wp * wz) : wp;
        const double wm = std::clamp (wmAnalog * T, kBilinearLimit, 0.9 * kPi);
        const double hm = std::norm (s.response (wm * fs));
        const double phi1 = std::pow (std::sin (0.5 * wm), 2.0);
        const double phi0 = 1.0 - phi1;
        const double phi2 = 4.0 * phi0 * phi1;
        const double Am = A0 * phi0 + A1 * phi1 + A2 * phi2;

        // Double zero at DC (high pass): keep it, match at wm.
        if (std::abs (s.n0) < 1.0e-300 && std::abs (s.n1) < 1.0e-300)
        {
            const double k = std::sqrt (hm * Am) / (4.0 * phi1);
            c.b0 = k; c.b1 = -2.0 * k; c.b2 = k;
            return c;
        }

        // Resonant complex zeros (high-Q shelves, cut bells inverted above): place them exactly with the
        // matched z-transform, like the poles, and fit only the section gain (log-domain least squares).
        if (s.n2 > 0.0 && s.n0 > 0.0)
        {
            const double zn = (s.n1 / s.n2) / (2.0 * wz);
            const double wzd = wz * T * std::sqrt (std::max (0.0, 1.0 - zn * zn));
            if (zn < 0.4 && wzd < 0.9 * kPi)
            {
                const double e = std::exp (-zn * wz * T);
                const double u1 = -2.0 * e * std::cos (wzd), u2 = e * e;
                double sumW = 0.0, sumL = 0.0;
                constexpr int nGrid = 24;
                for (int i = 0; i < nGrid; ++i)
                {
                    const double w = i == 0 ? 0.0 : (i == 1 ? kPi : kPi * std::pow (0.004, 1.0 - (double) (i - 2) / (double) (nGrid - 3)));
                    const std::complex<double> z1 = std::polar (1.0, -w);
                    const double bu = std::norm (1.0 + u1 * z1 + u2 * z1 * z1);
                    const double au = std::norm (1.0 + c.a1 * z1 + c.a2 * z1 * z1);
                    const double h = std::norm (s.response (w * fs));
                    if (bu < 1.0e-30 || h < 1.0e-30) continue;
                    const double weight = i == 0 ? 20.0 : (i == 1 ? 3.0 : 1.0);
                    sumL += weight * 0.5 * std::log (h * au / bu);
                    sumW += weight;
                }
                const double k = sumW > 0.0 ? std::exp (sumL / sumW) : 1.0;
                c.b0 = k; c.b1 = k * u1; c.b2 = k * u2;
                return c;
            }
        }

        // General: fit |B|^2 = B0*phi0 + B1*phi1 + B2*phi2 (linear in B) to |H|^2 |A|^2 by weighted least squares
        // on relative error, with DC, Nyquist and wm weighted heavily (after Vicanek's matched design, which
        // matches exactly at those three points; the extra points keep wide skirts near Nyquist right).
        double M[3][3] = {}, v[3] = {};
        double tMax = 0.0;
        constexpr int nGrid = 28;
        double grid[nGrid], target[nGrid], weightMul[nGrid];
        grid[0] = 0.0;        weightMul[0] = 12.0;
        grid[1] = kPi;        weightMul[1] = 12.0;
        grid[2] = wm;         weightMul[2] = 12.0;
        for (int i = 3; i < nGrid; ++i)
        {
            grid[i] = kPi * std::pow (0.004, 1.0 - (double) (i - 3) / (double) (nGrid - 4));
            weightMul[i] = 1.0;
        }
        for (int i = 0; i < nGrid; ++i)
        {
            const double p1 = std::pow (std::sin (0.5 * grid[i]), 2.0), p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
            target[i] = std::norm (s.response (grid[i] * fs)) * (A0 * p0 + A1 * p1 + A2 * p2);
            tMax = std::max (tMax, target[i]);
        }
        for (int i = 0; i < nGrid; ++i)
        {
            const double p1 = std::pow (std::sin (0.5 * grid[i]), 2.0), p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
            const double r[3] = { p0, p1, p2 };
            const double w = weightMul[i] / (target[i] + 1.0e-7 * tMax + 1.0e-300);
            const double w2 = w * w;
            for (int a = 0; a < 3; ++a)
            {
                v[a] += w2 * r[a] * target[i];
                for (int b = 0; b < 3; ++b) M[a][b] += w2 * r[a] * r[b];
            }
        }

        const double det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
                         - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
                         + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
        double B0, B1, B2;
        if (std::abs (det) > 1.0e-300 && std::isfinite (det))
        {
            auto solve = [&] (int col)
            {
                double m[3][3];
                for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b) m[a][b] = b == col ? v[a] : M[a][b];
                return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                      - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                      + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])) / det;
            };
            B0 = solve (0); B1 = solve (1); B2 = solve (2);
        }
        else
        {
            B0 = h0 * A0; B1 = hn * A1;
            B2 = (hm * Am - B0 * phi0 - B1 * phi1) / phi2;
        }

        if (! std::isfinite (B0 + B1 + B2))
        {
            B0 = h0 * A0; B1 = hn * A1;
            B2 = (hm * Am - B0 * phi0 - B1 * phi1) / phi2;
        }
        B0 = std::max (0.0, B0);
        B1 = std::max (0.0, B1);

        const double sb0 = std::sqrt (B0), sb1 = std::sqrt (B1);
        const double W = 0.5 * (sb0 + sb1);
        if (W * W + B2 < 0.0) B2 = -W * W;

        c.b0 = 0.5 * (W + std::sqrt (std::max (0.0, W * W + B2)));
        c.b1 = 0.5 * (sb0 - sb1);
        c.b2 = c.b0 > 1.0e-300 ? -B2 / (4.0 * c.b0) : 0.0;
        return c;
    }


    //==============================================================================
    // Near Nyquist a section with fixed (impulse-invariant) poles can't follow the analog curve: a gently
    // damped low pass ends up ~1 dB high over the whole top octave, resonant ones bump and dip. There we
    // re-fit poles and zeros together: |H|^2 = B(phi) / A(phi), both linear in (phi0, phi1, phi2) with
    // phi1 = sin^2(w/2), solved by iterated, re-weighted linear least squares (equation error, Steiglitz-McBride
    // style) on the relative error, then factored back into a stable, minimum-phase biquad. The result is
    // used only if it beats the matched design.
    bool solveLinear (double (&m)[5][6], int n, double (&x)[5]) noexcept
    {
        for (int col = 0; col < n; ++col)
        {
            int piv = col;
            for (int r = col + 1; r < n; ++r) if (std::abs (m[r][col]) > std::abs (m[piv][col])) piv = r;
            if (std::abs (m[piv][col]) < 1.0e-300) return false;
            if (piv != col) for (int k = 0; k <= n; ++k) std::swap (m[col][k], m[piv][k]);
            for (int r = 0; r < n; ++r)
            {
                if (r == col) continue;
                const double f = m[r][col] / m[col][col];
                for (int k = col; k <= n; ++k) m[r][k] -= f * m[col][k];
            }
        }
        for (int r = 0; r < n; ++r) x[r] = m[r][n] / m[r][r];
        return true;
    }

    /** Max |dB error| of a digital section against the analog one, where the analog is above `floorDb`, to 0.95 Nyquist. */
    double sectionError (const AnalogSection& s, const BiquadCoeffs& c, double fs) noexcept
    {
        double peak = 0.0, worst = 0.0;
        constexpr int n = 48;
        double ma[n];
        for (int i = 0; i < n; ++i)
        {
            const double hz = 0.475 * fs * std::pow (0.002, 1.0 - (double) i / (n - 1));
            ma[i] = safeDb (std::abs (s.response (kTwoPi * hz)));
            peak = std::max (peak, ma[i]);
        }
        for (int i = 0; i < n; ++i)
        {
            if (ma[i] < peak - 30.0) continue;
            const double hz = 0.475 * fs * std::pow (0.002, 1.0 - (double) i / (n - 1));
            worst = std::max (worst, std::abs (ma[i] - safeDb (std::abs (c.response (hz, fs)))));
        }
        return worst;
    }

    BiquadCoeffs refineNearNyquist (const AnalogSection& s, const BiquadCoeffs& matched, double fs) noexcept
    {
        if (s.firstOrder || s.allPass) return matched;
        // zeros on the frequency axis (notches) are matched exactly already; nothing to gain
        if (std::abs (s.n1) < 1.0e-15 && s.n2 > 0.0 && s.n0 > 0.0) return matched;

        const double T = 1.0 / fs;
        const double wp = std::sqrt (std::max (1.0e-18, s.d0));
        const double wz = std::abs (s.n2) > 1.0e-15 ? std::sqrt (std::abs (s.n0 / s.n2)) : 0.0;
        if (std::max (wp, wz) * T < kBilinearLimit) return matched;

        // numerator basis: free, zero at DC (B0 = 0), or double zero at DC (B ~ phi1^2)
        const bool dcZero = std::abs (s.n0) < 1.0e-300;
        const bool dcDouble = dcZero && std::abs (s.n1) < 1.0e-300;
        const int nb = dcDouble ? 1 : (dcZero ? 2 : 3);

        constexpr int nGrid = 40;
        double phi1[nGrid], tgt[nGrid], wgt[nGrid], tMax = 0.0;
        for (int i = 0; i < nGrid; ++i)
        {
            const double w = i == 0 ? 0.0 : (i == 1 ? kPi : kPi * std::pow (0.003, 1.0 - (double) (i - 2) / (nGrid - 3)));
            phi1[i] = std::pow (std::sin (0.5 * w), 2.0);
            tgt[i] = std::norm (s.response (w * fs));
            wgt[i] = i == 0 ? 20.0 : (i == 1 ? 2.0 : 1.0);
            tMax = std::max (tMax, tgt[i]);
        }
        auto basisB = [&] (int k, double p1) -> double
        {
            const double p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
            if (dcDouble) return -4.0 * p1 * p1;
            if (dcZero) return k == 0 ? p1 : p2;
            return k == 0 ? p0 : (k == 1 ? p1 : p2);
        };

        // start from the matched denominator, normalised to A(phi=0) = 1
        const double mA0 = std::pow (1.0 + matched.a1 + matched.a2, 2.0);
        if (mA0 < 1.0e-12) return matched;
        double A1 = std::pow (1.0 - matched.a1 + matched.a2, 2.0) / mA0, A2 = -4.0 * matched.a2 / mA0;
        double B[3] = {};

        for (int iter = 0; iter < 6; ++iter)
        {
            const int n = nb + 2;
            double M[5][6] = {};
            for (int i = 0; i < nGrid; ++i)
            {
                const double p1 = phi1[i], p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
                const double aPrev = std::max (1.0e-12, p0 + A1 * p1 + A2 * p2);
                const double t = tgt[i];
                // residual: sum B_k b_k - t (A1 p1 + A2 p2) - t p0, relative to t * aPrev
                const double scale = wgt[i] / ((t + 1.0e-6 * tMax) * aPrev);
                double row[5];
                for (int k = 0; k < nb; ++k) row[k] = basisB (k, p1) * scale;
                row[nb] = -t * p1 * scale;
                row[nb + 1] = -t * p2 * scale;
                const double rhs = t * p0 * scale;
                for (int a = 0; a < n; ++a)
                {
                    M[a][n] += row[a] * rhs;
                    for (int b = 0; b < n; ++b) M[a][b] += row[a] * row[b];
                }
            }
            double x[5] = {};
            if (! solveLinear (M, n, x)) return matched;
            for (int k = 0; k < nb; ++k) B[k] = x[k];
            A1 = x[nb];
            A2 = x[nb + 1];
        }

        // factor the denominator: p = 1 + a1 + a2, m = 1 - a1 + a2 = p sqrt(A1), a2 = -A2 p^2 / 4
        if (! (A1 > 0.0) || ! std::isfinite (A1 + A2)) { if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f rejected: if (! (A1 > 0.0) || ! std::isf\n", wp / kTwoPi); return matched; }
        const double sA1 = std::sqrt (A1);
        double p;
        if (std::abs (A2) < 1.0e-12) p = 2.0 / (1.0 + sA1);
        else
        {
            const double disc = (1.0 + sA1) * (1.0 + sA1) + 4.0 * A2;
            if (disc < 0.0) { if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f rejected: if (disc < 0.0) return matched\n", wp / kTwoPi); return matched; }
            p = (-(1.0 + sA1) + std::sqrt (disc)) / A2;
        }
        if (! (p > 0.0) || ! std::isfinite (p)) { if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f rejected: if (! (p > 0.0) || ! std::isfi\n", wp / kTwoPi); return matched; }
        BiquadCoeffs c;
        c.a2 = -A2 * p * p / 4.0;
        c.a1 = 0.5 * (p - p * sA1);
        if (! (std::abs (c.a2) < 0.9995 && std::abs (c.a1) < 1.0 + c.a2 - 1.0e-6)) { if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f rejected: if (! (std::abs (c.a2) < 0.999\n", wp / kTwoPi); return matched; }

        // numerator from |B|^2 in (phi0, phi1, phi2) form, scaled back (A was normalised by p^2)
        const double sc = p * p;
        double Bf0, Bf1, Bf2;
        if (dcDouble)    { Bf0 = 0.0; Bf1 = -4.0 * B[0]; Bf2 = B[0]; }   // -4 phi1^2 = phi2 - 4 phi1
        else if (dcZero) { Bf0 = 0.0; Bf1 = B[0]; Bf2 = B[1]; }
        else             { Bf0 = B[0]; Bf1 = B[1]; Bf2 = B[2]; }
        Bf0 *= sc; Bf1 *= sc; Bf2 *= sc;
        if (Bf0 < 0.0 || Bf1 < 0.0) { if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f rejected: if (Bf0 < 0.0 || Bf1 < 0.0) re\n", wp / kTwoPi); return matched; }
        const double sb0 = std::sqrt (Bf0), sb1 = std::sqrt (Bf1);
        const double W = 0.5 * (sb0 + sb1);
        if (W * W + Bf2 < 0.0) { if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f rejected: if (W * W + Bf2 < 0.0) return \n", wp / kTwoPi); return matched; }
        c.b0 = 0.5 * (W + std::sqrt (W * W + Bf2));
        c.b1 = 0.5 * (sb0 - sb1);
        c.b2 = c.b0 > 1.0e-300 ? -Bf2 / (4.0 * c.b0) : 0.0;
        if (! std::isfinite (c.b0 + c.b1 + c.b2)) return matched;

        if (std::getenv ("ST_EQ_DEBUG")) std::fprintf (stderr, "refine wp %.0f: refit err %.3f matched err %.3f\n", wp / kTwoPi, sectionError (s, c, fs), sectionError (s, matched, fs));
        return sectionError (s, c, fs) < sectionError (s, matched, fs) ? c : matched;
    }

    BiquadCoeffs invert (BiquadCoeffs c) noexcept
    {
        if (std::abs (c.b2) > 0.99995 * std::abs (c.b0)) c.b2 = std::copysign (0.99995 * std::abs (c.b0), c.b2);   // keep poles inside
        const double g = 1.0 / c.b0;
        return { g, c.a1 * g, c.a2 * g, c.b1 * g, c.b2 * g };
    }
}

BiquadCoeffs matchSection (const AnalogSection& s, double fs) noexcept
{
    // Cut-type sections (zeros below / sharper than the poles) are designed as the reciprocal boost
    // section and inverted: their poles then stay well below Nyquist and the match is far better.
    if (! s.allPass)
    {
        if (s.firstOrder)
        {
            if (std::abs (s.n1) > 1.0e-15 && s.n0 > 0.0 && s.n0 / s.n1 < s.d0 * (1.0 - 1.0e-9))
            {
                AnalogSection r;
                r.firstOrder = true;
                r.n1 = 1.0 / s.n1; r.n0 = s.d0 / s.n1; r.d1 = 1.0; r.d0 = s.n0 / s.n1;
                return invert (matchCore (r, fs));
            }
        }
        else if (s.n2 > 0.0 && s.n0 > 0.0 && s.n1 > 0.0)
        {
            const double wn = std::sqrt (s.n0 / s.n2), zn = (s.n1 / s.n2) / (2.0 * wn);
            const double wd = std::sqrt (s.d0), zd = s.d1 / (2.0 * wd);
            const bool sameFreq = std::abs (wn - wd) < 1.0e-9 * wd;
            if ((! sameFreq && wn < wd) || (sameFreq && zn < zd))
            {
                AnalogSection r;
                r.n2 = 1.0 / s.n2; r.n1 = s.d1 / s.n2; r.n0 = s.d0 / s.n2;
                r.d1 = s.n1 / s.n2; r.d0 = s.n0 / s.n2;
                return refineNearNyquist (s, invert (matchCore (r, fs)), fs);
            }
        }
    }
    return refineNearNyquist (s, matchCore (s, fs), fs);
}

Digital designDigital (const Analog& a, double sampleRate) noexcept
{
    Digital d;
    const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    d.numSections = a.numSections;
    for (int i = 0; i < a.numSections; ++i)
        d.sections[(size_t) i] = matchSection (a.sections[(size_t) i], fs);

    if (d.numSections == 0 && std::abs (a.gain - 1.0) > 1.0e-12)
        d.numSections = 1;   // pure gain

    if (d.numSections > 0)
    {
        auto& s0 = d.sections[0];
        s0.b0 *= a.gain; s0.b1 *= a.gain; s0.b2 *= a.gain;
    }
    return d;
}

} // namespace eq

//==============================================================================
void EqCascade::reset() noexcept
{
    for (auto& s : state) s = { 0.0, 0.0 };
}

void EqCascade::setDesign (const eq::Digital& d, bool glide) noexcept
{
    if (d.numSections != target.numSections)
    {
        reset();
        current = target = d;
        ramping = false;
        return;
    }

    if (glide)
    {
        if (ramping) current = target;   // previous ramp never ran: start from where we would have ended
        target = d;
        ramping = true;
    }
    else
    {
        current = target = d;
        ramping = false;
    }
}

void EqCascade::process (int ch, float* data, int n) noexcept
{
    const int ns = target.numSections;
    if (ns == 0 || n <= 0) return;

    for (int s = 0; s < ns; ++s)
    {
        auto& z = state[(size_t) (s * kMaxChannels + ch)];
        double z1 = z[0], z2 = z[1];
        const auto& t = target.sections[(size_t) s];

        if (! ramping)
        {
            const double b0 = t.b0, b1 = t.b1, b2 = t.b2, a1 = t.a1, a2 = t.a2;
            for (int i = 0; i < n; ++i)
            {
                const double x = data[i];
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                data[i] = (float) y;
            }
        }
        else
        {
            const auto& c = current.sections[(size_t) s];
            const double inv = 1.0 / (double) n;
            const double db0 = (t.b0 - c.b0) * inv, db1 = (t.b1 - c.b1) * inv, db2 = (t.b2 - c.b2) * inv;
            const double da1 = (t.a1 - c.a1) * inv, da2 = (t.a2 - c.a2) * inv;
            double b0 = c.b0, b1 = c.b1, b2 = c.b2, a1 = c.a1, a2 = c.a2;
            for (int i = 0; i < n; ++i)
            {
                b0 += db0; b1 += db1; b2 += db2; a1 += da1; a2 += da2;
                const double x = data[i];
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                data[i] = (float) y;
            }
        }

        z[0] = flushDenormal (z1);
        z[1] = flushDenormal (z2);
    }
}

void EqCascade::advance() noexcept
{
    if (ramping)
    {
        current = target;
        ramping = false;
    }
}

} // namespace st::dsp
