#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

namespace fizzle
{
class MeterComponent : public juce::Component, private juce::Timer
{
public:
    MeterComponent()
    {
        startTimerHz(20);
    }

    void setLevel(float newLevel)
    {
        target.store(juce::jlimit(0.0f, 1.0f, newLevel));
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(theme::panel.withAlpha(0.78f));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(theme::knobTrack.withAlpha(0.85f));
        g.fillRoundedRectangle(b.reduced(0.8f), 5.2f);

        const auto level = displayed.load();
        auto area = getLocalBounds().reduced(2).toFloat();
        area.setWidth(area.getWidth() * level);
        juce::ColourGradient fill(theme::accent, area.getX(), area.getY(),
                                  theme::mint, area.getRight(), area.getY(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, 4.0f);
    }

private:
    std::atomic<float> target { 0.0f };
    std::atomic<float> displayed { 0.0f };

    void timerCallback() override
    {
        const auto t = target.load();
        const auto d = displayed.load();
        const auto next = d + (t - d) * 0.18f;
        if (std::abs(next - d) < 0.0008f)
            return;
        displayed.store(next);
        repaint();
    }
};
}
