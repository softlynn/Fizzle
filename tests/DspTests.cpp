#include <JuceHeader.h>
#include "../src/audio/BuiltInProcessors.h"

namespace
{
class HpfTest final : public juce::UnitTest
{
public:
    HpfTest() : juce::UnitTest("HPF reduces low frequency", "DSP") {}

    void runTest() override
    {
        beginTest("HPF attenuates 30Hz more than 1kHz");

        fizzle::BuiltInProcessors proc;
        proc.prepare(48000.0, 1);

        fizzle::EffectParameters params;
        params.hpfHz.store(120.0f);

        juce::AudioBuffer<float> low(1, 4800);
        juce::AudioBuffer<float> high(1, 4800);

        for (int i = 0; i < 4800; ++i)
        {
            low.setSample(0, i, std::sin(juce::MathConstants<float>::twoPi * 30.0f * static_cast<float>(i) / 48000.0f));
            high.setSample(0, i, std::sin(juce::MathConstants<float>::twoPi * 1000.0f * static_cast<float>(i) / 48000.0f));
        }

        proc.process(low, params);
        proc.process(high, params);

        const auto lowRms = low.getRMSLevel(0, 0, low.getNumSamples());
        const auto highRms = high.getRMSLevel(0, 0, high.getNumSamples());
        expect(lowRms < highRms * 0.6f);
    }
};

class ExpanderTest final : public juce::UnitTest
{
public:
    ExpanderTest() : juce::UnitTest("Expander reduces noise floor", "DSP") {}

    void runTest() override
    {
        beginTest("Quiet signal gets attenuated");

        fizzle::BuiltInProcessors proc;
        proc.prepare(48000.0, 1);

        fizzle::EffectParameters params;
        params.gateThresholdDb.store(-40.0f);
        params.gateRatio.store(4.0f);

        juce::AudioBuffer<float> buf(1, 2048);
        buf.clear();
        buf.applyGain(0.01f);

        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample(0, i, 0.01f * std::sin(juce::MathConstants<float>::twoPi * 200.0f * static_cast<float>(i) / 48000.0f));

        const auto before = buf.getRMSLevel(0, 0, buf.getNumSamples());
        proc.process(buf, params);
        const auto after = buf.getRMSLevel(0, 0, buf.getNumSamples());

        expect(after < before);
    }
};

class CompressorTest final : public juce::UnitTest
{
public:
    CompressorTest() : juce::UnitTest("Compressor taming peaks", "DSP") {}

    void runTest() override
    {
        beginTest("High signal is limited");

        fizzle::BuiltInProcessors proc;
        proc.prepare(48000.0, 1);

        fizzle::EffectParameters params;
        params.compThresholdDb.store(-24.0f);
        params.compRatio.store(4.0f);
        params.compMakeupDb.store(0.0f);
        params.limiterCeilDb.store(-3.0f);

        juce::AudioBuffer<float> buf(1, 4096);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample(0, i, 0.9f * std::sin(juce::MathConstants<float>::twoPi * 1000.0f * static_cast<float>(i) / 48000.0f));

        proc.process(buf, params);
        auto peak = 0.0f;
        for (int i = 0; i < buf.getNumSamples(); ++i)
            peak = juce::jmax(peak, std::abs(buf.getSample(0, i)));

        expect(peak <= juce::Decibels::decibelsToGain(-3.0f) + 0.01f);
    }
};

HpfTest hpfTest;
ExpanderTest expanderTest;
CompressorTest compressorTest;
}