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
