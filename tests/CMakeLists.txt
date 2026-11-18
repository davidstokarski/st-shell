# ST Shell unit tests (console app). Enabled with -DST_SHELL_BUILD_TESTS=ON.

juce_add_console_app(st_dsp_tests PRODUCT_NAME "ST DSP Tests")

target_sources(st_dsp_tests PRIVATE DspTests.cpp)
target_compile_features(st_dsp_tests PRIVATE cxx_std_20)

target_compile_definitions(st_dsp_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(st_dsp_tests
    PRIVATE
        st::st_shell
        juce::juce_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_custom_target(run_st_dsp_tests
    COMMAND st_dsp_tests
    DEPENDS st_dsp_tests
    COMMENT "Running ST DSP tests")

# ---------------------------------------------------------------------------
# Modulation tests (st::mod + modulation UI smoke test). Separate console app.
juce_add_console_app(StModTests PRODUCT_NAME "ST Mod Tests")

target_sources(StModTests PRIVATE ModTests.cpp)
target_compile_features(StModTests PRIVATE cxx_std_20)

target_compile_definitions(StModTests PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(StModTests
    PRIVATE
        st::st_shell
        st_shell_resources
        juce::juce_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_custom_target(run_st_mod_tests
    COMMAND StModTests
    DEPENDS StModTests
    COMMENT "Running ST modulation tests")

# ---------------------------------------------------------------------------
# Reverb tests (st::dsp::Reverb). Separate console app; `StReverbTests --render <dir>` writes WAVs.
juce_add_console_app(StReverbTests PRODUCT_NAME "ST Reverb Tests")

target_sources(StReverbTests PRIVATE ReverbTests.cpp)
target_compile_features(StReverbTests PRIVATE cxx_std_20)

target_compile_definitions(StReverbTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(StReverbTests
    PRIVATE
        st::st_shell
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_custom_target(run_st_reverb_tests
    COMMAND StReverbTests
    DEPENDS StReverbTests
    COMMENT "Running ST reverb tests")
