#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace fizzle
{
class Biquad
{
public:
    void reset()
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void setHighPass(float sampleRate, float frequency, float q = 0.7071f)
    {
        const auto w0 = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
        const auto alpha = std::sin(w0) / (2.0f * q);
        const auto cosW0 = std::cos(w0);

        const auto b0 = (1.0f + cosW0) * 0.5f;
        const auto b1 = -(1.0f + cosW0);
        const auto b2 = (1.0f + cosW0) * 0.5f;
        const auto a0 = 1.0f + alpha;
        const auto a1 = -2.0f * cosW0;
        const auto a2 = 1.0f - alpha;

        coeffs[0] = b0 / a0;
        coeffs[1] = b1 / a0;
        coeffs[2] = b2 / a0;
        coeffs[3] = a1 / a0;
        coeffs[4] = a2 / a0;
    }

    float process(float x)
    {
        const auto y = coeffs[0] * x + z1;
        z1 = coeffs[1] * x - coeffs[3] * y + z2;
        z2 = coeffs[2] * x - coeffs[4] * y;
        return y;
    }

private:
    std::array<float, 5> coeffs { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float z1 { 0.0f };
    float z2 { 0.0f };
};
}
