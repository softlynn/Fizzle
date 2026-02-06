#pragma once

#include "../AppConfig.h"
#include <map>
#include <optional>

namespace fizzle
{
struct PluginPresetState
{
    juce::String identifier;
    juce::String name;
    bool enabled { true };
    float mix { 1.0f };
    juce::String base64State;
};

struct PresetData
{
    juce::String name;
    EngineSettings engine;
    std::map<juce::String, float> values;
    juce::Array<PluginPresetState> plugins;
};

class PresetStore
{
public:
    explicit PresetStore(const juce::File& appDirectory);

    void savePreset(const PresetData& preset) const;
    std::optional<PresetData> loadPreset(const juce::String& name) const;
    juce::StringArray listPresets() const;
    bool deletePreset(const juce::String& name) const;

    [[nodiscard]] juce::File getPresetDirectory() const;

private:
    juce::File presetDir;
};
}
