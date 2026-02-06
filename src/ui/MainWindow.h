#pragma once

#include <JuceHeader.h>

namespace fizzle
{
class MainComponent;

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(std::unique_ptr<MainComponent> content);
    MainComponent* getMainComponent() const;
    void closeButtonPressed() override;

private:
    std::unique_ptr<MainComponent> ownedContent;
};
}
