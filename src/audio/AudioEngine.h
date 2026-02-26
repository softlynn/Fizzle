#pragma once

#include "../AppConfig.h"
#include "../core/Logger.h"
#include "ProcessorChain.h"
#include "Resampler.h"
#include "../plugins/VstHost.h"

namespace fizzle
{
struct Diagnostics
{
    juce::String inputDevice;
    juce::String outputDevice;
    double sampleRate { 0.0 };
    int bufferSize { 0 };
    double cpuPercent { 0.0 };
    double dryLatencyMs { 0.0 };
    double postFxLatencyMs { 0.0 };
    float inputLevel { 0.0f };
    float outputLevel { 0.0f };
    uint64_t droppedBuffers { 0 };
};

class AudioEngine : public juce::AudioIODeviceCallback,
                    private juce::AsyncUpdater
{
public:
    AudioEngine();
    ~AudioEngine() override;

    bool start(const EngineSettings& settings, juce::String& error);
    void stop();

    void setEffectParameters(EffectParameters* paramsRef);
    EffectParameters* getEffectParameters() const { return params; }

    void setBypass(bool bypass) { if (params != nullptr) params->bypass.store(bypass); }
    void setMute(bool mute) { if (params != nullptr) params->mute.store(mute); }

    Diagnostics getDiagnostics() const;
    EngineSettings currentSettings() const;

    void restartAudio(juce::String& error);

    void setTestToneEnabled(bool enabled) { testToneEnabled.store(enabled); }
    void setListenEnabled(bool enabled);
    bool isListenEnabled() const { return listenEnabled.load(); }
    void setMonitorOutputDevice(const juce::String& name);

    VstHost& getVstHost() { return vstHost; }
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

private:
    class MonitorCallback : public juce::AudioIODeviceCallback
    {
    public:
        explicit MonitorCallback(AudioEngine& ownerRef) : owner(ownerRef) {}
        void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputChannelData, int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
        void audioDeviceStopped() override {}
    private:
        AudioEngine& owner;
    };

    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceManager monitorDeviceManager;
    MonitorCallback monitorCallback { *this };
    bool deviceManagerInitialised { false };
    bool monitorManagerInitialised { false };
    EffectParameters* params { nullptr };
    mutable juce::CriticalSection lifecycleLock;
    mutable juce::CriticalSection settingsLock;
    EngineSettings settings;

    ProcessorChain chain;
    VstHost vstHost;

    mutable juce::CriticalSection diagnosticsLock;
    Diagnostics diagnostics;
    std::atomic<uint64_t> droppedBuffers { 0 };

    juce::AudioBuffer<float> inBuffer;
    juce::AudioBuffer<float> internalBuffer;
    juce::AudioBuffer<float> outBuffer;
    juce::SpinLock ioCallbackLock;
    std::atomic<bool> deviceReconfiguring { false };

    Resampler resampler;

    std::atomic<bool> testToneEnabled { false };
    std::atomic<double> tonePhase { 0.0 };
    std::atomic<double> currentDeviceSampleRate { kInternalSampleRate };
    std::atomic<bool> listenEnabled { false };
    juce::String monitorOutputDevice;

    juce::AbstractFifo monitorFifo { 32768 };
    juce::AudioBuffer<float> monitorFifoBuffer { 2, 32768 };

    std::atomic<bool> autoRecoveryPending { false };
    std::atomic<juce::uint32> lastAutoRecoveryAttemptMs { 0 };
    juce::String lastAutoRecoveryReason;
    juce::CriticalSection autoRecoveryLock;

    void queueAutoRecoveryRestart(const juce::String& reason);
    void handleAsyncUpdate() override;
};
}
