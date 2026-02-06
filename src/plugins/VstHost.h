#pragma once

#include <JuceHeader.h>

namespace fizzle
{
struct HostedPlugin
{
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    bool enabled { true };
    float mix { 1.0f };
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
    void setEnabled(int index, bool enabled);
    void clear();

    void processBlock(juce::AudioBuffer<float>& buffer);
    juce::Array<HostedPlugin*> getChain();
    HostedPlugin* getPlugin(int index);
    void prepare(double sampleRate, int blockSize);
    void release();

private:
    struct ScannedEntry
    {
        juce::String name;
        juce::String path;
    };

    juce::AudioPluginFormatManager formatManager;
    juce::CriticalSection chainLock;
    juce::OwnedArray<HostedPlugin> chain;
    juce::Array<ScannedEntry> scanned;
    juce::AudioBuffer<float> wetBuffer;

public:
    void setMix(int index, float mix);
    float getMix(int index) const;
    int getLatencySamples() const;
};
}
