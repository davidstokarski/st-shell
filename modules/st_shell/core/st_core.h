#pragma once

// Fallbacks so the module also compiles in non-plugin targets (tests, tools).
#ifndef JucePlugin_Name
 #define JucePlugin_Name "ST Plugin"
#endif
#ifndef ST_PRODUCT_VERSION
 #define ST_PRODUCT_VERSION "0.0.0"
#endif

#include "Brand.h"
#include "Params.h"
#include "Metering.h"
#include "LevelHistory.h"
