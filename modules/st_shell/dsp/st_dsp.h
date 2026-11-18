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
