#pragma once

#include <JuceHeader.h>
#include "../audio/AudioEngine.h"
#include "Theme.h"

namespace fizzle
{
class DiagnosticsPanel : public juce::Component, private juce::Timer
{
public:
    explicit DiagnosticsPanel(AudioEngine& e) : engine(e)
    {
        addAndMakeVisible(text);
        text.setMultiLine(true);
        text.setReadOnly(true);
        text.setScrollbarsShown(true);
        text.setColour(juce::TextEditor::backgroundColourId, theme::panel.withAlpha(0.55f));
        text.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x00000000));
        text.setColour(juce::TextEditor::highlightColourId, theme::accent.withAlpha(0.22f));
        text.setColour(juce::TextEditor::textColourId, theme::text);
        juce::Font font(juce::FontOptions(15.0f));
        font.setExtraKerningFactor(0.014f);
        text.applyFontToAllText(font);
        startTimerHz(4);
    }

    void resized() override
    {
        text.setBounds(getLocalBounds().reduced(10, 8));
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(theme::panel.withAlpha(0.86f));
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(theme::accent.withAlpha(0.22f));
        g.drawRoundedRectangle(r, 12.0f, 1.0f);
    }

private:
    AudioEngine& engine;
    juce::TextEditor text;

    void timerCallback() override
    {
        const auto d = engine.getDiagnostics();
        const auto levelToText = [](float v)
        {
            if (v <= 0.00001f)
                return juce::String("-inf dB");
            return juce::String(juce::Decibels::gainToDecibels(v), 1) + " dB";
        };
        juce::String s;
        s << "Input Device    : " << d.inputDevice << "\n";
        s << "Output Device   : " << d.outputDevice << "\n";
        s << "Sample Rate     : " << juce::String(d.sampleRate, 1) << " Hz\n";
        s << "Buffer Size     : " << d.bufferSize << "\n";
        s << "CPU Load        : " << juce::String(d.cpuPercent, 2) << "%\n";
        s << "Latency (Dry)   : " << juce::String(d.dryLatencyMs, 1) << " ms\n";
        s << "Latency (Post)  : " << juce::String(d.postFxLatencyMs, 1) << " ms\n";
        s << "Input Level     : " << levelToText(d.inputLevel) << "\n";
        s << "Output Level    : " << levelToText(d.outputLevel) << "\n";
        s << "Dropped Buffers : " << juce::String(static_cast<int64_t>(d.droppedBuffers)) << "\n";
        text.setText(s, false);
    }
};
}
