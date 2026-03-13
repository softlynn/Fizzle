#pragma once

#include <JuceHeader.h>
#include <limits>

namespace fizzle
{
inline int scoreVirtualMicOutput(const juce::String& deviceName)
{
    const auto name = deviceName.trim().toLowerCase();
    if (name.isEmpty())
        return std::numeric_limits<int>::min();

    int score = 0;
    if (name.contains("fizzle mic"))
        score += 200;
    if (name.contains("vb-audio") || name.contains("vb audio"))
        score += 90;
    if (name.contains("virtual cable"))
        score += 80;
    if (name.contains("cable input"))
        score += 70;
    if (name.contains("cable in"))
        score += 60;

    if (name.contains("virtual desktop"))
        score -= 160;
    if (name.contains("fxsound"))
        score -= 120;
    if (name.contains("speaker") || name.contains("speakers")
        || name.contains("headphone") || name.contains("headphones")
        || name.contains("headset") || name.contains("airpods")
        || name.contains("bluetooth"))
    {
        score -= 45;
    }

    return score;
}

inline bool isPreferredVirtualMicOutputName(const juce::String& deviceName)
{
    return scoreVirtualMicOutput(deviceName) >= 100;
}

inline juce::String findPreferredVirtualMicOutput(const juce::StringArray& outputs)
{
    juce::String best;
    int bestScore = std::numeric_limits<int>::min();
    for (const auto& output : outputs)
    {
        const auto score = scoreVirtualMicOutput(output);
        if (score > bestScore)
        {
            bestScore = score;
            best = output;
        }
    }

    return bestScore >= 100 ? best : juce::String {};
}

inline int scoreMonitorOutput(const juce::String& deviceName, const juce::String& excludedOutput = {})
{
    if (deviceName.isEmpty())
        return std::numeric_limits<int>::min();
    if (excludedOutput.isNotEmpty() && deviceName.equalsIgnoreCase(excludedOutput))
        return std::numeric_limits<int>::min();
    if (isPreferredVirtualMicOutputName(deviceName))
        return std::numeric_limits<int>::min();

    const auto name = deviceName.trim().toLowerCase();
    int score = 0;

    if (name.contains("speaker") || name.contains("speakers"))
        score += 70;
    if (name.contains("headphone") || name.contains("headphones") || name.contains("headset"))
        score += 75;
    if (name.contains("airpods") || name.contains("bluetooth") || name.contains("buds") || name.contains("earbud"))
        score += 65;
    if (name.contains("realtek"))
        score += 40;
    if (name.contains("output"))
        score += 18;
    if (name.contains("virtual desktop"))
        score += 12;

    return score;
}

inline juce::String findPreferredMonitorOutput(const juce::StringArray& outputs, const juce::String& excludedOutput = {})
{
    juce::String best;
    int bestScore = std::numeric_limits<int>::min();
    for (const auto& output : outputs)
    {
        const auto score = scoreMonitorOutput(output, excludedOutput);
        if (score > bestScore)
        {
            bestScore = score;
            best = output;
        }
    }

    if (bestScore > 0)
        return best;

    for (const auto& output : outputs)
    {
        if (excludedOutput.isNotEmpty() && output.equalsIgnoreCase(excludedOutput))
            continue;
        if (! isPreferredVirtualMicOutputName(output))
            return output;
    }

    return {};
}
}
