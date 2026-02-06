#pragma once

#include <JuceHeader.h>

namespace fizzle
{
class MainComponent;

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(std::unique_ptr<MainComponent> content);
    ~MainWindow() override;
    MainComponent* getMainComponent() const;
    void closeButtonPressed() override;

private:
    class WindowLookAndFeel;
    std::unique_ptr<MainComponent> ownedContent;
    std::unique_ptr<WindowLookAndFeel> lookAndFeel;
};
}
