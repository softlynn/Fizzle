#include <JuceHeader.h>
#include "../src/AppConfig.h"

namespace
{
class AppConfigTest final : public juce::UnitTest
{
public:
    AppConfigTest() : juce::UnitTest("App config buffer sanitization", "Config") {}

    void runTest() override
    {
        beginTest("Main audio buffers clamp into the supported range");
        expectEquals(fizzle::sanitizeAudioBufferSize(0), fizzle::kDefaultBlockSize);
        expectEquals(fizzle::sanitizeAudioBufferSize(8), fizzle::kMinAudioBufferSize);
        expectEquals(fizzle::sanitizeAudioBufferSize(4096), fizzle::kMaxAudioBufferSize);

        beginTest("Optional listen buffers allow match-main mode");
        expectEquals(fizzle::sanitizeOptionalAudioBufferSize(0), 0);
        expectEquals(fizzle::sanitizeOptionalAudioBufferSize(-64), 0);
        expectEquals(fizzle::sanitizeOptionalAudioBufferSize(48), 48);
    }
};

AppConfigTest appConfigTest;
}
