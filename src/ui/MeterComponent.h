#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace fizzle
{
class MeterComponent : public juce::Component, private juce::Timer
{
public:
    MeterComponent()
    {
        startTimerHz(30);
    }

    void setLevel(float newLevel)
    {
        target.store(juce::jlimit(0.0f, 1.0f, newLevel));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(theme::panel);
        g.setColour(theme::knobTrack);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

        const auto level = displayed.load();
        auto area = getLocalBounds().reduced(2).toFloat();
        area.setWidth(area.getWidth() * level);
        g.setColour(theme::accent);
        g.fillRoundedRectangle(area, 3.0f);
    }

private:
    std::atomic<float> target { 0.0f };
    std::atomic<float> displayed { 0.0f };

    void timerCallback() override
    {
        const auto t = target.load();
        const auto d = displayed.load();
        displayed.store(d + (t - d) * 0.2f);
        repaint();
    }
};
}