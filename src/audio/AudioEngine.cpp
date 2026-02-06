#include "AudioEngine.h"

namespace fizzle
{
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    monitorDeviceManager.removeAudioCallback(&monitorCallback);
    monitorDeviceManager.closeAudioDevice();
    stop();
}

bool AudioEngine::start(const EngineSettings& requested, juce::String& error)
{
    settings = requested;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    if (! deviceManagerInitialised)
    {
        const auto initError = deviceManager.initialise(2, 2, nullptr, true, {}, nullptr);
        if (initError.isNotEmpty())
        {
            error = initError;
            Logger::instance().log("Audio device manager init failed: " + initError);
            return false;
        }
        deviceManagerInitialised = true;
    }

    deviceManager.removeAudioCallback(this);
    deviceManager.getAudioDeviceSetup(setup);

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        const auto inputs = type->getDeviceNames(true);
        const auto outputs = type->getDeviceNames(false);

        if (settings.inputDeviceName.isEmpty() && inputs.size() > 0)
            settings.inputDeviceName = inputs[0];

        bool outputExists = outputs.contains(settings.outputDeviceName);
        if (! outputExists)
        {
            for (const auto& out : outputs)
            {
                if (out.containsIgnoreCase("vb-cable") || out.containsIgnoreCase("cable input") || out.containsIgnoreCase("virtual"))
                {
                    settings.outputDeviceName = out;
                    outputExists = true;
                    break;
                }
            }
        }

        if (! outputExists && outputs.size() > 0)
            settings.outputDeviceName = outputs[0];
    }

    setup.inputDeviceName = settings.inputDeviceName;
    setup.outputDeviceName = settings.outputDeviceName;
    setup.bufferSize = settings.bufferSize;
    setup.sampleRate = settings.preferredSampleRate;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.outputChannels.clear();
    setup.inputChannels.setBit(0, true);
    setup.outputChannels.setBit(0, true);
    setup.outputChannels.setBit(1, true);

    auto result = deviceManager.setAudioDeviceSetup(setup, true);
    if (result.isNotEmpty())
    {
        Logger::instance().log("Preferred setup failed, retrying with system defaults: " + result);
        setup.sampleRate = 0.0;
        setup.bufferSize = 0;
        result = deviceManager.setAudioDeviceSetup(setup, true);
        if (result.isNotEmpty())
        {
            error = result;
            Logger::instance().log("Audio setup failed: " + result);
            return false;
        }
    }

    juce::AudioDeviceManager::AudioDeviceSetup appliedSetup;
    deviceManager.getAudioDeviceSetup(appliedSetup);
    settings.inputDeviceName = appliedSetup.inputDeviceName;
    settings.outputDeviceName = appliedSetup.outputDeviceName;
    if (appliedSetup.bufferSize > 0)
        settings.bufferSize = appliedSetup.bufferSize;
    if (appliedSetup.sampleRate > 0.0)
        settings.preferredSampleRate = appliedSetup.sampleRate;

    deviceManager.addAudioCallback(this);

    if (listenEnabled.load() && monitorOutputDevice.isNotEmpty())
        setListenEnabled(true);

    Logger::instance().log("Audio engine started");
    return true;
}

void AudioEngine::stop()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

void AudioEngine::setMonitorOutputDevice(const juce::String& name)
{
    monitorOutputDevice = name;
}

void AudioEngine::setListenEnabled(bool enabled)
{
    listenEnabled.store(enabled);

    monitorDeviceManager.removeAudioCallback(&monitorCallback);
    monitorDeviceManager.closeAudioDevice();
    monitorFifo.reset();

    if (! enabled)
        return;

    juce::String error;
    if (! monitorManagerInitialised)
    {
        error = monitorDeviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
        if (error.isNotEmpty())
        {
            Logger::instance().log("Monitor manager init failed: " + error);
            listenEnabled.store(false);
            return;
        }
        monitorManagerInitialised = true;
    }
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    monitorDeviceManager.getAudioDeviceSetup(setup);
    setup.outputDeviceName = monitorOutputDevice;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.outputChannels.clear();
    setup.outputChannels.setBit(0, true);
    setup.outputChannels.setBit(1, true);
    const auto preferredRate = currentDeviceSampleRate.load();
    if (preferredRate > 0.0)
        setup.sampleRate = preferredRate;
    if (settings.bufferSize > 0)
        setup.bufferSize = settings.bufferSize;

    error = monitorDeviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        Logger::instance().log("Monitor preferred setup failed, retrying with defaults: " + error);
        setup.sampleRate = 0.0;
        setup.bufferSize = 0;
        error = monitorDeviceManager.setAudioDeviceSetup(setup, true);
    }
    if (error.isNotEmpty())
    {
        Logger::instance().log("Monitor setup failed: " + error);
        listenEnabled.store(false);
        return;
    }

    monitorDeviceManager.addAudioCallback(&monitorCallback);
}

void AudioEngine::setEffectParameters(EffectParameters* paramsRef)
{
    params = paramsRef;
}

Diagnostics AudioEngine::getDiagnostics() const
{
    const juce::ScopedLock sl(diagnosticsLock);
    return diagnostics;
}

EngineSettings AudioEngine::currentSettings() const
{
    return settings;
}

void AudioEngine::restartAudio(juce::String& error)
{
    stop();
    if (! start(settings, error))
        Logger::instance().log("Audio restart failed: " + error);
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    const auto tick = juce::Time::getHighResolutionTicks();

    if (numSamples <= 0 || outputChannelData == nullptr)
        return;

    if (params == nullptr)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return;
    }

    inBuffer.setSize(juce::jmax(1, numInputChannels), numSamples, false, false, true);
    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        if (inputChannelData[ch] != nullptr)
            inBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
        else
            inBuffer.clear(ch, 0, numSamples);
    }

    const auto deviceRate = currentDeviceSampleRate.load();
    const auto safeDeviceRate = (deviceRate > 1000.0) ? deviceRate : kInternalSampleRate;

    const auto internalSamples = static_cast<int>(std::ceil((static_cast<double>(numSamples) * kInternalSampleRate) / safeDeviceRate));
    internalBuffer.setSize(2, juce::jmax(1, internalSamples), false, false, true);
    internalBuffer.clear();

    resampler.process(inBuffer, internalBuffer, safeDeviceRate, kInternalSampleRate);
    if (numInputChannels <= 1 && internalBuffer.getNumChannels() > 1)
        internalBuffer.copyFrom(1, 0, internalBuffer, 0, 0, internalBuffer.getNumSamples());

    float inPeak = 0.0f;
    for (int c = 0; c < internalBuffer.getNumChannels(); ++c)
        inPeak = juce::jmax(inPeak, internalBuffer.getMagnitude(c, 0, internalBuffer.getNumSamples()));

    if (testToneEnabled.load())
    {
        const auto phaseDelta = juce::MathConstants<double>::twoPi * 440.0 / kInternalSampleRate;
        auto phase = tonePhase.load();
        for (int c = 0; c < internalBuffer.getNumChannels(); ++c)
        {
            auto* d = internalBuffer.getWritePointer(c);
            for (int i = 0; i < internalBuffer.getNumSamples(); ++i)
                d[i] = static_cast<float>(std::sin(phase + static_cast<double>(i) * phaseDelta) * 0.1);
        }
        phase += phaseDelta * static_cast<double>(internalBuffer.getNumSamples());
        tonePhase.store(std::fmod(phase, juce::MathConstants<double>::twoPi));
    }

    // Built-in FX removed: VST chain is the processing path.
    if (! params->bypass.load())
        vstHost.processBlock(internalBuffer);

    if (params->mute.load())
        internalBuffer.clear();

    internalBuffer.applyGain(juce::Decibels::decibelsToGain(params->outputGainDb.load()));

    outBuffer.setSize(juce::jmax(1, numOutputChannels), numSamples, false, false, true);
    outBuffer.clear();
    resampler.process(internalBuffer, outBuffer, kInternalSampleRate, safeDeviceRate);

    float outPeak = 0.0f;
    for (int c = 0; c < outBuffer.getNumChannels(); ++c)
        outPeak = juce::jmax(outPeak, outBuffer.getMagnitude(c, 0, outBuffer.getNumSamples()));

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::copy(outputChannelData[ch], outBuffer.getReadPointer(juce::jmin(ch, outBuffer.getNumChannels() - 1)), numSamples);
    }

    if (listenEnabled.load())
    {
        int start1, size1, start2, size2;
        monitorFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
        for (int c = 0; c < 2; ++c)
        {
            auto* fifo = monitorFifoBuffer.getWritePointer(c);
            const auto* src = outBuffer.getReadPointer(juce::jmin(c, outBuffer.getNumChannels() - 1));
            if (size1 > 0)
                juce::FloatVectorOperations::copy(fifo + start1, src, size1);
            if (size2 > 0)
                juce::FloatVectorOperations::copy(fifo + start2, src + size1, size2);
        }
        monitorFifo.finishedWrite(size1 + size2);
    }

    const auto elapsedTicks = juce::Time::getHighResolutionTicks() - tick;
    const auto seconds = juce::Time::highResolutionTicksToSeconds(elapsedTicks);
    const auto blockSeconds = static_cast<double>(numSamples) / safeDeviceRate;
    const auto configuredBuffer = numSamples;

    const juce::ScopedLock sl(diagnosticsLock);
    diagnostics.sampleRate = safeDeviceRate;
    diagnostics.bufferSize = configuredBuffer;
    diagnostics.cpuPercent = juce::jlimit(0.0, 100.0, (seconds / blockSeconds) * 100.0);
    const auto dryMs = (safeDeviceRate > 0.0) ? ((2.0 * static_cast<double>(configuredBuffer)) / safeDeviceRate) * 1000.0 : 0.0;
    const auto pluginMs = (safeDeviceRate > 0.0) ? (1000.0 * static_cast<double>(vstHost.getLatencySamples()) / safeDeviceRate) : 0.0;
    diagnostics.dryLatencyMs = dryMs;
    diagnostics.postFxLatencyMs = dryMs + pluginMs;
    diagnostics.inputLevel = inPeak;
    diagnostics.outputLevel = outPeak;
    if (seconds > blockSeconds)
        ++droppedBuffers;
    diagnostics.droppedBuffers = droppedBuffers.load();
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto rawRate = device != nullptr ? device->getCurrentSampleRate() : kInternalSampleRate;
    const auto sampleRate = rawRate > 1000.0 ? rawRate : kInternalSampleRate;
    const auto deviceBuffer = device != nullptr ? juce::jmax(1, device->getCurrentBufferSizeSamples()) : 256;
    const auto internalBlock = static_cast<int>(std::ceil((static_cast<double>(deviceBuffer) * kInternalSampleRate) / sampleRate));
    currentDeviceSampleRate.store(sampleRate);
    chain.prepare(kInternalSampleRate, 2);
    chain.reset();
    vstHost.prepare(kInternalSampleRate, juce::jmax(64, internalBlock));

    if (device != nullptr)
    {
        const juce::ScopedLock sl(diagnosticsLock);
        diagnostics.inputDevice = settings.inputDeviceName;
        diagnostics.outputDevice = settings.outputDeviceName;
        diagnostics.sampleRate = sampleRate;
        diagnostics.bufferSize = device->getCurrentBufferSizeSamples();
        const auto ioMs = (device->getInputLatencyInSamples() + device->getOutputLatencyInSamples()) * 1000.0 / sampleRate;
        diagnostics.dryLatencyMs = ioMs;
        diagnostics.postFxLatencyMs = ioMs + (1000.0 * static_cast<double>(vstHost.getLatencySamples()) / sampleRate);
        diagnostics.droppedBuffers = droppedBuffers.load();
    }

    Logger::instance().log("Audio device started at " + juce::String(sampleRate));
}

void AudioEngine::audioDeviceStopped()
{
    chain.reset();
    vstHost.release();
    Logger::instance().log("Audio device stopped");
}

void AudioEngine::MonitorCallback::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputChannelData, int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    if (outputChannelData == nullptr || numSamples <= 0)
        return;

    for (int c = 0; c < numOutputChannels; ++c)
        if (outputChannelData[c] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[c], numSamples);

    const auto queued = owner.monitorFifo.getNumReady();
    const auto maxQueued = numSamples * 2;
    if (queued > maxQueued)
    {
        int dropStart1, dropSize1, dropStart2, dropSize2;
        owner.monitorFifo.prepareToRead(queued - maxQueued, dropStart1, dropSize1, dropStart2, dropSize2);
        owner.monitorFifo.finishedRead(dropSize1 + dropSize2);
    }

    int start1, size1, start2, size2;
    owner.monitorFifo.prepareToRead(numSamples, start1, size1, start2, size2);
    const auto readSamples = size1 + size2;
    if (readSamples <= 0)
        return;

    for (int c = 0; c < numOutputChannels; ++c)
    {
        if (outputChannelData[c] == nullptr)
            continue;

        const auto* fifo = owner.monitorFifoBuffer.getReadPointer(juce::jmin(c, owner.monitorFifoBuffer.getNumChannels() - 1));
        if (size1 > 0)
            juce::FloatVectorOperations::copy(outputChannelData[c], fifo + start1, size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy(outputChannelData[c] + size1, fifo + start2, size2);
    }

    owner.monitorFifo.finishedRead(readSamples);
}
}
