#include "ProcessorChain.h"

namespace fizzle
{
void ProcessorChain::prepare(double sampleRate, int channels)
{
    juce::ignoreUnused(sampleRate, channels);
}

void ProcessorChain::reset()
{
}

void ProcessorChain::process(juce::AudioBuffer<float>& buffer, const EffectParameters& params)
{
    juce::ignoreUnused(buffer, params);
}
}
