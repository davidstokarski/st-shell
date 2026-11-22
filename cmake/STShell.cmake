# st_add_plugin(<target>
#     PRODUCT_NAME   "ST Something"
#     PLUGIN_CODE    Abcd               # 4 chars, unique per plugin, >=1 uppercase
#     VERSION        1.0.0
#     SOURCES        a.cpp b.cpp ...
#     [BUNDLE_SUFFIX something]          # defaults to lowercase target name
#     [AU_TYPE kAudioUnitType_Effect]    # default: Effect, or MusicDevice with IS_SYNTH
#     [VST3_CATEGORIES Fx ...]           # default: Fx, or Instrument with IS_SYNTH
#     [IS_SYNTH] [NEEDS_MIDI_INPUT]      # IS_SYNTH = an instrument (MIDI in, no audio input needed)
#     [BINARY_DATA file1 file2 ...])
#
# Creates a VST3 + AU (+ Standalone for development) plugin that links the
# ST Shell module, the shared fonts and JUCE's recommended flags.

set(ST_COMPANY_NAME      "ST Audio"   CACHE STRING "Company name shown in hosts")
set(ST_MANUFACTURER_CODE "StAu"       CACHE STRING "4-char manufacturer code")
set(ST_BUNDLE_PREFIX     "com.staudio" CACHE STRING "Bundle id prefix")
option(ST_BUILD_STANDALONE "Also build a Standalone app (handy for development)" ON)
option(ST_COPY_AFTER_BUILD "Copy plugins into ~/Library/Audio/Plug-Ins after build" ON)

function(st_add_plugin target)
    set(flags IS_SYNTH NEEDS_MIDI_INPUT)
    set(one PRODUCT_NAME PLUGIN_CODE VERSION BUNDLE_SUFFIX AU_TYPE)
    set(multi SOURCES BINARY_DATA VST3_CATEGORIES)
    cmake_parse_arguments(ST "${flags}" "${one}" "${multi}" ${ARGN})

    if(NOT ST_BUNDLE_SUFFIX)
        string(TOLOWER "${target}" ST_BUNDLE_SUFFIX)
    endif()

    set(formats VST3 AU)
    if(ST_BUILD_STANDALONE)
        list(APPEND formats Standalone)
    endif()

    set(synth FALSE)
    if(ST_IS_SYNTH)
        set(synth TRUE)
    endif()
    set(midiIn FALSE)
    if(ST_NEEDS_MIDI_INPUT OR ST_IS_SYNTH)
        set(midiIn TRUE)
    endif()

    # Instruments register as MusicDevice (AU "aumu") / VST3 "Instrument" unless told otherwise.
    set(auType kAudioUnitType_Effect)
    set(vst3Categories Fx)
    if(ST_IS_SYNTH)
        set(auType kAudioUnitType_MusicDevice)
        set(vst3Categories Instrument)
    endif()
    if(ST_AU_TYPE)
        set(auType ${ST_AU_TYPE})
    endif()
    if(ST_VST3_CATEGORIES)
        set(vst3Categories ${ST_VST3_CATEGORIES})
    endif()

    juce_add_plugin(${target}
        VERSION                  "${ST_VERSION}"
        COMPANY_NAME             "${ST_COMPANY_NAME}"
        BUNDLE_ID                "${ST_BUNDLE_PREFIX}.${ST_BUNDLE_SUFFIX}"
        PLUGIN_MANUFACTURER_CODE "${ST_MANUFACTURER_CODE}"
        PLUGIN_CODE              "${ST_PLUGIN_CODE}"
        PRODUCT_NAME             "${ST_PRODUCT_NAME}"
        FORMATS                  ${formats}
        IS_SYNTH                 ${synth}
        NEEDS_MIDI_INPUT         ${midiIn}
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD  ${ST_COPY_AFTER_BUILD}
        VST3_CATEGORIES          ${vst3Categories}
        AU_MAIN_TYPE             ${auType}
        HARDENED_RUNTIME_ENABLED TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE)

    target_sources(${target} PRIVATE ${ST_SOURCES})
    target_compile_features(${target} PRIVATE cxx_std_20)

    if(ST_BINARY_DATA)
        juce_add_binary_data(${target}_data
            HEADER_NAME "${target}Binary.h"
            NAMESPACE   "${target}Binary"
            SOURCES     ${ST_BINARY_DATA})
        set_target_properties(${target}_data PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_link_libraries(${target} PRIVATE ${target}_data)
    endif()

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        ST_PRODUCT_NAME="${ST_PRODUCT_NAME}"
        ST_PRODUCT_VERSION="${ST_VERSION}")

    target_link_libraries(${target}
        PRIVATE
            st::st_shell
            st_shell_resources
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
