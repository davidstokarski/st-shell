#pragma once

// Aggregator for the dsp layer. Add headers below.
//
// st::dsp — real-time-safe DSP building blocks shared by ST plugins.
//   Utils.h        dB helpers, LinearSmoother, pan law, mid/side, buffer sanitiser
//   Filters.h      Biquad (RBJ), Svf (TPT), OnePole, DcBlocker, TiltFilter
//   Crossover.h    N-band Linkwitz-Riley crossover (LR2/LR4/LR8)
//   Dynamics.h     UpDownCompressor (OTT-style)
//   Oversampler.h  runtime-switchable juce::dsp::Oversampling wrapper
//   Delay.h        integer DelayLine (latency compensation)
//   Envelope.h     EnvelopeFollower
//   EqDesign.h     analog-matched parametric EQ bands (bell/shelf/cut/notch/BP/tilt/AP), EqCascade
//   Convolver.h    uniform partitioned FFT convolver (mono/diagonal/2x2), lock-free IR swap
//   Stft.h         streaming STFT (sqrt-Hann WOLA, hop N/4) with per-frame spectral callback
//
// Denormal policy: block-level process() methods install juce::ScopedNoDenormals
// themselves; per-sample processSample() calls expect the caller to have one in scope.

#include "Utils.h"
#include "Filters.h"
#include "Crossover.h"
#include "Dynamics.h"
#include "Oversampler.h"
#include "Delay.h"
#include "Envelope.h"
#include "Reverb.h"
#include "EqDesign.h"
#include "Convolver.h"
#include "Stft.h"
