#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace fizzle
{
class Knob : public juce::Component
{
public:
    explicit Knob(juce::Slider& sliderRef) : slider(sliderRef)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        addAndMakeVisible(slider);
    }

    void resized() override
    {
        slider.setBounds(getLocalBounds());
    }

private:
    juce::Slider& slider;
};
}