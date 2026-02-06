#include "Logger.h"

namespace fizzle
{
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::initialise()
{
    const auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("Fizzle")
                             .getChildFile("logs");
    appData.createDirectory();

    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    currentLogFile = appData.getChildFile("fizzle-" + timestamp + ".log");
    currentLogFile.replaceWithText("Fizzle log start\n");
}

void Logger::log(const juce::String& message)
{
    const juce::ScopedLock scoped(lock);
    if (! currentLogFile.existsAsFile())
        initialise();

    const auto line = juce::Time::getCurrentTime().toString(true, true, true, true) + " | " + message + "\n";
    currentLogFile.appendText(line);
}

juce::File Logger::getLogDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Fizzle")
        .getChildFile("logs");
}
}