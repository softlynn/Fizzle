#pragma once

#include "../AppConfig.h"

namespace fizzle
{
class SettingsStore
{
public:
    SettingsStore();

    [[nodiscard]] EngineSettings loadEngineSettings() const;
    void saveEngineSettings(const EngineSettings& settings) const;

    [[nodiscard]] juce::File getSettingsFile() const;
    [[nodiscard]] juce::File getAppDirectory() const;

private:
    juce::File appDir;
    juce::File settingsFile;
};
}