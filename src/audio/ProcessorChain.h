#pragma once

#include "../AppConfig.h"

namespace fizzle
{
class ProcessorChain
{
public:
    void prepare(double sampleRate, int channels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const EffectParameters& params);
};
}
