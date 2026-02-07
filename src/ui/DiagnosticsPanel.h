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
        setInterceptsMouseClicks(false, false);
        text.setInterceptsMouseClicks(false, false);
        text.setMultiLine(true);
        text.setReadOnly(true);
        text.setScrollbarsShown(true);
        applyStyle(1.0f);
        startTimerHz(4);
    }

    void applyStyle(float uiScale)
    {
        text.setColour(juce::TextEditor::backgroundColourId, theme::panel.withAlpha(0.55f));
        text.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x00000000));
        text.setColour(juce::TextEditor::highlightColourId, theme::accent.withAlpha(0.22f));
        text.setColour(juce::TextEditor::textColourId, theme::text);
        juce::Font font(juce::FontOptions(14.6f * uiScale));
        font.setTypefaceName("Consolas");
        font.setExtraKerningFactor(0.014f);
        text.applyFontToAllText(font);
        repaint();
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
    juce::String lastText;

    void timerCallback() override
    {
        const auto d = engine.getDiagnostics();
        const auto levelToText = [](float v)
        {
            if (v <= 0.00001f)
                return juce::String("-inf dB");
            return juce::String(juce::Decibels::gainToDecibels(v), 1) + " dB";
        };
        const auto line = [](juce::String key, juce::String value)
        {
            key = key.paddedRight(' ', 14);
            return key + " : " + value + "\n";
        };
        juce::String s;
        s << line("Input Device", d.inputDevice);
        s << line("Output Device", d.outputDevice);
        s << line("Sample Rate", juce::String(d.sampleRate, 1) + " Hz");
        s << line("Buffer Size", juce::String(d.bufferSize));
        s << line("CPU Load", juce::String(d.cpuPercent, 2) + "%");
        s << line("Latency (Dry)", juce::String(d.dryLatencyMs, 1) + " ms");
        s << line("Latency (Post)", juce::String(d.postFxLatencyMs, 1) + " ms");
        s << line("Input Level", levelToText(d.inputLevel));
        s << line("Output Level", levelToText(d.outputLevel));
        s << line("Dropped Buffers", juce::String(static_cast<int64_t>(d.droppedBuffers)));
        if (s != lastText)
        {
            lastText = s;
            text.setText(lastText, false);
        }
    }
};
}
