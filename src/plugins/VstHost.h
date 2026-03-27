#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace fizzle
{
struct HostedPlugin
{
    struct MutationListener final : juce::AudioProcessorListener
    {
        std::atomic<uint64_t>* counter { nullptr };

        void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
        {
            if (counter != nullptr)
                counter->fetch_add(1);
        }

        void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override
        {
            if (counter != nullptr)
                counter->fetch_add(1);
        }
    };

    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::atomic<bool> enabled { true };
    std::atomic<bool> faulted { false };
    std::atomic<bool> editorOpen { false };
    std::atomic<float> mix { 1.0f };
    juce::SpinLock callbackLock;
    MutationListener mutationListener;

    ~HostedPlugin()
    {
        if (instance != nullptr)
            instance->removeListener(&mutationListener);
    }
};

class VstHost
{
public:
    using HostedPluginHandle = std::shared_ptr<HostedPlugin>;

    VstHost();

    juce::StringArray scanFolder(const juce::File& folder);
    juce::Array<juce::PluginDescription> getKnownPluginDescriptions() const;
    juce::StringArray getScannedPaths() const;
    void importScannedPaths(const juce::StringArray& paths);
    bool addPlugin(const juce::PluginDescription& description, double sampleRate, int blockSize, juce::String& error);
    bool addPluginWithState(const juce::PluginDescription& description,
                            double sampleRate,
                            int blockSize,
                            const juce::String& base64State,
                            juce::String& error);
    bool findDescriptionByIdentifier(const juce::String& identifier, juce::PluginDescription& out) const;
    void removePlugin(int index);
    void movePlugin(int from, int to);
    void swapPlugin(int first, int second);
    void setEnabled(int index, bool enabled);
    void clear();
    void setLowCpuModeEnabled(bool enabled);
    bool isLowCpuModeEnabled() const { return lowCpuModeEnabled.load(); }
    uint64_t getMutationCounter() const { return mutationCounter.load(); }

    void processBlock(juce::AudioBuffer<float>& buffer);
    juce::Array<HostedPlugin*> getChain();
    HostedPlugin* getPlugin(int index);
    HostedPluginHandle getPluginHandle(int index);
    std::vector<HostedPluginHandle> getChainHandles() const;
    int getPluginCount() const;
    void prepare(double sampleRate, int blockSize);
    void release();

private:
    using HostedPluginPtr = HostedPluginHandle;

    struct ScannedEntry
    {
        juce::String name;
        juce::String path;
    };

    juce::AudioPluginFormatManager formatManager;
    mutable juce::CriticalSection chainLock;
    mutable juce::CriticalSection scannedLock;
    std::vector<HostedPluginPtr> chain;
    juce::Array<ScannedEntry> scanned;
    juce::AudioBuffer<float> wetBuffer;
    std::atomic<int> cachedLatencySamples { 0 };
    std::atomic<double> activeProcessingSampleRate { 0.0 };
    std::atomic<int> activeProcessingBlockSize { 0 };
    std::atomic<bool> lowCpuModeEnabled { false };
    std::atomic<uint64_t> mutationCounter { 0 };

    bool createHostedPlugin(const juce::PluginDescription& description,
                            double sampleRate,
                            int blockSize,
                            juce::String& error,
                            HostedPluginPtr& outHosted);
    std::vector<HostedPluginPtr> copyChainSnapshot() const;
    void refreshLatencyCacheLocked();

public:
    void setMix(int index, float mix);
    float getMix(int index) const;
    int getLatencySamples() const;
};
}
