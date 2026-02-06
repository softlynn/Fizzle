#include "BuiltInProcessors.h"

namespace fizzle
{
void BuiltInProcessors::prepare(double sampleRate, int)
{
    sr = sampleRate;
    for (auto& biquad : hpf)
        biquad.setHighPass(static_cast<float>(sr), 80.0f);

    for (auto& biquad : deEss)
        biquad.setHighPass(static_cast<float>(sr), 4200.0f, 0.8f);

    reset();
}

void BuiltInProcessors::reset()
{
    for (auto& biquad : hpf)
        biquad.reset();

    for (auto& biquad : deEss)
        biquad.reset();

    gateEnv = { 0.0f, 0.0f };
    compEnv = { 0.0f, 0.0f };
}

float BuiltInProcessors::dbToLin(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

float BuiltInProcessors::linToDb(float lin)
{
    constexpr float floorVal = 1.0e-7f;
    return 20.0f * std::log10(std::max(lin, floorVal));
}

void BuiltInProcessors::process(juce::AudioBuffer<float>& buffer, const EffectParameters& params)
{
    if (params.bypass.load())
        return;

    const auto channels = std::min(2, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();

    const auto hpfHz = juce::jlimit(40.0f, 300.0f, params.hpfHz.load());
    const auto gateThresh = params.gateThresholdDb.load();
    const auto gateRatio = juce::jmax(1.0f, params.gateRatio.load());
    const auto deEssAmount = juce::jlimit(0.0f, 1.0f, params.deEssAmount.load());

    const auto low = dbToLin(params.lowGainDb.load());
    const auto mid = dbToLin(params.midGainDb.load());
    const auto high = dbToLin(params.highGainDb.load());

    const auto compThresh = params.compThresholdDb.load();
    const auto compRatio = juce::jmax(1.0f, params.compRatio.load());
    const auto compMakeup = dbToLin(params.compMakeupDb.load());
    const auto limiterCeil = dbToLin(params.limiterCeilDb.load());

    for (int c = 0; c < channels; ++c)
    {
        hpf[static_cast<size_t>(c)].setHighPass(static_cast<float>(sr), hpfHz);
        auto* data = buffer.getWritePointer(c);

        for (int i = 0; i < samples; ++i)
        {
            auto x = hpf[static_cast<size_t>(c)].process(data[i]);

            const auto absx = std::abs(x);
            gateEnv[static_cast<size_t>(c)] = 0.995f * gateEnv[static_cast<size_t>(c)] + 0.005f * absx;
            const auto gateDb = linToDb(gateEnv[static_cast<size_t>(c)]);
            float gateGain = 1.0f;
            if (gateDb < gateThresh)
            {
                const auto delta = gateThresh - gateDb;
                gateGain = dbToLin(-delta * (1.0f - (1.0f / gateRatio)));
            }
            x *= gateGain;

            const auto sibilance = std::abs(deEss[static_cast<size_t>(c)].process(x));
            const auto deEssGain = juce::jlimit(0.5f, 1.0f, 1.0f - (sibilance * deEssAmount * 0.8f));
            x *= deEssGain;

            const auto lowMid = x * low;
            const auto midBand = x * mid;
            const auto highBand = x * high;
            x = (lowMid + midBand + highBand) * (1.0f / 3.0f);

            const auto level = std::abs(x);
            const auto atk = 0.01f;
            const auto rel = 0.001f;
            compEnv[static_cast<size_t>(c)] = juce::jmax(level, compEnv[static_cast<size_t>(c)] * (1.0f - rel) + level * rel);
            const auto levelDb = linToDb(compEnv[static_cast<size_t>(c)]);

            float compGain = 1.0f;
            if (levelDb > compThresh)
            {
                const auto over = levelDb - compThresh;
                const auto reduced = over - (over / compRatio);
                compGain = dbToLin(-reduced * (1.0f - atk));
            }

            x *= compGain;
            x *= compMakeup;
            x = juce::jlimit(-limiterCeil, limiterCeil, x);

            if (params.mute.load())
                x = 0.0f;

            data[i] = x;
        }
    }
}
}
