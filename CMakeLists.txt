cmake_minimum_required(VERSION 3.22)

# Build universal binary on macOS (Intel + Apple Silicon)
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build universal binary")
endif()

project(Jimmy VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

juce_add_plugin(Jimmy
    COMPANY_NAME "doralteres"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Jmmy
    PLUGIN_CODE Jmm1
    FORMATS VST3
    PRODUCT_NAME "Jimmy"
    VST3_CATEGORIES "Instrument" "Tools"
)

target_sources(Jimmy
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

target_compile_definitions(Jimmy
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Jimmy
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ──────────────────────────────────────────────────
# Unit Tests (opt-in: cmake -DJIMMY_BUILD_TESTS=ON)
# ──────────────────────────────────────────────────
option(JIMMY_BUILD_TESTS "Build unit tests" OFF)

if(JIMMY_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(JimmyTests
        tests/ChordParserTests.cpp
        tests/SongModelTests.cpp
        tests/SharedStateTests.cpp
        tests/MidiChordImporterTests.cpp
        tests/LyricsEditorTests.cpp
        tests/ThemeTests.cpp
        tests/MidiExporterTests.cpp
        tests/MidiImporterTests.cpp
    )

    target_include_directories(JimmyTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_link_libraries(JimmyTests
        PRIVATE
            GTest::gtest_main
            juce::juce_core
            juce::juce_audio_basics
            juce::juce_data_structures
            juce::juce_graphics
    )

    target_compile_definitions(JimmyTests
        PRIVATE
            JUCE_STANDALONE_APPLICATION=1
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    include(GoogleTest)
    gtest_discover_tests(JimmyTests)
endif()
