cmake_minimum_required(VERSION 3.24)

project(Fizzle VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FIZZLE_BUILD_TESTS "Build tests" ON)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(juce)

include(cmake/Version.cmake)
include(cmake/Warnings.cmake)

juce_add_binary_data(FizzleAssets
  SOURCES
    resources/icon/app.png
)

juce_add_gui_app(Fizzle
  PRODUCT_NAME "Fizzle"
  COMPANY_NAME "Fizzle Audio"
  BUNDLE_ID "com.fizzleaudio.fizzle"
  VERSION ${PROJECT_VERSION}
)

set(FIZZLE_SOURCES
  src/main.cpp
  src/AppConfig.h
  src/core/SettingsStore.h
  src/core/SettingsStore.cpp
  src/core/Logger.h
  src/core/Logger.cpp
  src/core/PresetStore.h
  src/core/PresetStore.cpp
  src/audio/Biquad.h
  src/audio/BuiltInProcessors.h
  src/audio/BuiltInProcessors.cpp
  src/audio/ProcessorChain.h
  src/audio/ProcessorChain.cpp
  src/audio/Resampler.h
  src/audio/Resampler.cpp
  src/audio/AudioEngine.h
  src/audio/AudioEngine.cpp
  src/plugins/VstHost.h
  src/plugins/VstHost.cpp
  src/ui/Theme.h
  src/ui/Knob.h
  src/ui/MeterComponent.h
  src/ui/DiagnosticsPanel.h
  src/ui/MainComponent.h
  src/ui/MainComponent.cpp
  src/ui/MainWindow.h
  src/ui/MainWindow.cpp
  src/ui/TrayController.h
  src/ui/TrayController.cpp
)

if (WIN32)
  list(APPEND FIZZLE_SOURCES resources/icon/app.rc)
endif()

target_sources(Fizzle PRIVATE ${FIZZLE_SOURCES})

juce_generate_juce_header(Fizzle)

target_compile_definitions(Fizzle PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Fizzle,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_PLUGINHOST_VST3=1
  FIZZLE_VERSION="${PROJECT_VERSION}"
)

target_include_directories(Fizzle PRIVATE src)

set_fizzle_warnings(Fizzle)

if(MSVC)
  target_compile_definitions(Fizzle PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(Fizzle PRIVATE
  juce::juce_gui_extra
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_dsp
  FizzleAssets
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

if (WIN32)
  set_target_properties(Fizzle PROPERTIES
    WIN32_EXECUTABLE TRUE
  )
endif()

if(FIZZLE_BUILD_TESTS)
  enable_testing()
  add_executable(FizzleTests
    tests/TestMain.cpp
    tests/DspTests.cpp
    src/audio/Biquad.h
    src/audio/BuiltInProcessors.h
    src/audio/BuiltInProcessors.cpp
  )

  target_include_directories(FizzleTests PRIVATE
    src
    ${CMAKE_BINARY_DIR}/Fizzle_artefacts/JuceLibraryCode
  )
  set_fizzle_warnings(FizzleTests)

  target_link_libraries(FizzleTests PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_graphics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
  )

  add_test(NAME FizzleTests COMMAND FizzleTests)
endif()
