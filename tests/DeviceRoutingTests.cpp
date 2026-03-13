#include <JuceHeader.h>
#include "../src/audio/DeviceRouting.h"

namespace
{
class DeviceRoutingTest final : public juce::UnitTest
{
public:
    DeviceRoutingTest() : juce::UnitTest("Device routing prefers real virtual cable", "Routing") {}

    void runTest() override
    {
        beginTest("Strict virtual mic detection ignores unrelated virtual outputs");
        expect(fizzle::isPreferredVirtualMicOutputName("CABLE In 16ch (VB-Audio Virtual Cable)"));
        expect(fizzle::isPreferredVirtualMicOutputName("CABLE Input (VB-Audio Virtual Cable)"));
        expect(! fizzle::isPreferredVirtualMicOutputName("Speakers (Virtual Desktop Audio)"));
        expect(! fizzle::isPreferredVirtualMicOutputName("Speakers (FxSound Audio Enhancer)"));

        beginTest("Preferred virtual mic chooses the cable device");
        juce::StringArray outputs { "Speakers (Virtual Desktop Audio)",
                                    "CABLE In 16ch (VB-Audio Virtual Cable)",
                                    "Headphones (Realtek Audio)" };
        expectEquals(fizzle::findPreferredVirtualMicOutput(outputs),
                     juce::String("CABLE In 16ch (VB-Audio Virtual Cable)"));

        beginTest("Preferred monitor avoids the virtual mic output");
        expectEquals(fizzle::findPreferredMonitorOutput(outputs, "CABLE In 16ch (VB-Audio Virtual Cable)"),
                     juce::String("Headphones (Realtek Audio)"));
    }
};

DeviceRoutingTest deviceRoutingTest;
}
