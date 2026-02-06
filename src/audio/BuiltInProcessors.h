#pragma once

#include "Biquad.h"
#include "../AppConfig.h"

namespace fizzle
{
class BuiltInProcessors
{
public:
    void prepare(double sampleRate, int channels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const EffectParameters& params);

private:
    double sr { kInternalSampleRate };
    std::array<Biquad, 2> hpf;
    std::array<Biquad, 2> deEss;
    std::array<float, 2> gateEnv { 0.0f, 0.0f };
    std::array<float, 2> compEnv { 0.0f, 0.0f };

    static float dbToLin(float db);
    static float linToDb(float lin);
};
}