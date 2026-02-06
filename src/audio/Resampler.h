#pragma once

#include <JuceHeader.h>

namespace fizzle
{
class Resampler
{
public:
    void process(const juce::AudioBuffer<float>& input,
                 juce::AudioBuffer<float>& output,
                 double inputRate,
                 double outputRate)
    {
        const auto inSamples = input.getNumSamples();
        const auto channels = juce::jmin(input.getNumChannels(), output.getNumChannels());
        if (inSamples <= 0 || channels <= 0)
            return;

        if (std::abs(inputRate - outputRate) < 0.01)
        {
            const auto copyCount = juce::jmin(inSamples, output.getNumSamples());
            for (int c = 0; c < channels; ++c)
                output.copyFrom(c, 0, input, c, 0, copyCount);
            if (copyCount < output.getNumSamples())
                output.clear(copyCount, output.getNumSamples() - copyCount);
            return;
        }

        const auto ratio = inputRate / outputRate;
        const auto outSamples = output.getNumSamples();
        for (int c = 0; c < channels; ++c)
        {
            const auto* in = input.getReadPointer(c);
            auto* out = output.getWritePointer(c);
            for (int i = 0; i < outSamples; ++i)
            {
                const auto pos = static_cast<double>(i) * ratio;
                const auto i0 = juce::jlimit(0, inSamples - 1, static_cast<int>(pos));
                const auto i1 = juce::jlimit(0, inSamples - 1, i0 + 1);
                const auto frac = static_cast<float>(pos - static_cast<double>(i0));
                out[i] = in[i0] + (in[i1] - in[i0]) * frac;
            }
        }
    }
};
}