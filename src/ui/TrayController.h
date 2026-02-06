#pragma once

#include <JuceHeader.h>

namespace fizzle
{
class TrayController : public juce::SystemTrayIconComponent
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void trayOpenRequested() = 0;
        virtual void trayToggleBypass() = 0;
        virtual void trayToggleMute() = 0;
        virtual void trayRestartAudio() = 0;
        virtual void trayExit() = 0;
        virtual void trayPresetSelected(const juce::String& name) = 0;
        virtual juce::StringArray trayPresets() const = 0;
    };

    explicit TrayController(Listener& l);

    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    Listener& listener;
    std::unique_ptr<juce::LookAndFeel_V4> trayLookAndFeel;
    juce::Image createIcon() const;
    void showContextMenu();
};
}
