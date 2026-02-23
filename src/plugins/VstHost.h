#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace fizzle
{
struct HostedPlugin
{
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::atomic<bool> enabled { true };
    std::atomic<float> mix { 1.0f };
};

class VstHost
{
public:
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

    void processBlock(juce::AudioBuffer<float>& buffer);
    juce::Array<HostedPlugin*> getChain();
    HostedPlugin* getPlugin(int index);
    void prepare(double sampleRate, int blockSize);
    void release();

private:
    using HostedPluginPtr = std::shared_ptr<HostedPlugin>;

    struct ScannedEntry
    {
        juce::String name;
        juce::String path;
    };

    juce::AudioPluginFormatManager formatManager;
    mutable juce::CriticalSection chainLock;
    std::vector<HostedPluginPtr> chain;
    juce::Array<ScannedEntry> scanned;
    juce::AudioBuffer<float> wetBuffer;
    std::atomic<int> cachedLatencySamples { 0 };

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
