#pragma once

#include <JuceHeader.h>

namespace fizzle
{
class Logger
{
public:
    static Logger& instance();

    void initialise();
    void log(const juce::String& message);

    juce::File getLogDirectory() const;

private:
    juce::CriticalSection lock;
    juce::File currentLogFile;
};
}