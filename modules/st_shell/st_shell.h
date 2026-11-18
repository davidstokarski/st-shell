/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 st_shell
  vendor:             ST Audio
  version:            0.1.0
  name:               ST Shell
  description:        Shared DSP, state, modulation and UI foundation for ST plugins.
  dependencies:       juce_audio_utils juce_dsp juce_gui_extra
  minimumCppStandard: 20

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define ST_SHELL_H_INCLUDED 1

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

// Order matters: lower layers first.
#include "core/st_core.h"
#include "dsp/st_dsp.h"
#include "mod/st_mod.h"
#include "state/st_state.h"
#include "theme/st_theme.h"
#include "ui/st_ui.h"
