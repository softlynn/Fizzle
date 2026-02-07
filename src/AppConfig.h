#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace fizzle
{
constexpr double kInternalSampleRate = 48000.0;
constexpr int kDefaultBlockSize = 256;

struct EffectParameters
{
    std::atomic<float> hpfHz { 80.0f };
    std::atomic<float> gateThresholdDb { -45.0f };
    std::atomic<float> gateRatio { 2.0f };
    std::atomic<float> deEssAmount { 0.25f };
    std::atomic<float> lowGainDb { 0.0f };
    std::atomic<float> midGainDb { 0.0f };
    std::atomic<float> highGainDb { 0.0f };
    std::atomic<float> compThresholdDb { -18.0f };
    std::atomic<float> compRatio { 2.5f };
    std::atomic<float> compMakeupDb { 2.0f };
    std::atomic<float> limiterCeilDb { -1.0f };
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<bool> bypass { false };
    std::atomic<bool> mute { false };
};

struct EngineSettings
{
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    juce::String outputDeviceType { "Windows Audio" };
    int bufferSize { kDefaultBlockSize };
    double preferredSampleRate { kInternalSampleRate };
    juce::String vstScanFolder;
    bool hasCompletedInitialVstScan { false };
    juce::StringArray scannedVstPaths;
    bool autoEnableByApp { false };
    juce::String autoEnableProcessName;
    juce::StringArray autoEnableProcessNames;
    juce::String autoEnableProcessPath;
    juce::String listenMonitorDeviceName;
    juce::String lastPresetName;
    bool hasSeenFirstLaunchGuide { false };
    bool autoInstallUpdates { false };
    bool startWithWindows { false };
    bool startMinimizedToTray { false };
    bool followAutoEnableWindowState { false };
    bool lightMode { false };
    bool transparentBackground { false };
    int themeVariant { 0 }; // 0 = Aqua, 1 = Salmon
    int uiDensity { 1 }; // 0 = Small (Compact), 1 = Normal, 2 = Large
    juce::StringArray vstSearchPaths;
};
}
