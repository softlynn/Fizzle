#pragma once

#include <JuceHeader.h>

namespace fizzle
{
class MainComponent;

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(std::unique_ptr<MainComponent> content);
    ~MainWindow() override = default;
    MainComponent* getMainComponent() const;
    void closeButtonPressed() override;
    juce::BorderSize<int> getContentComponentBorder() const override;
    juce::BorderSize<int> getBorderThickness() const override;
    void setTransparentBackgroundEnabled(bool enabled);

private:
    void applyWindowsBackdrop(bool transparent);

    std::unique_ptr<MainComponent> ownedContent;
    bool transparentBackgroundEnabled { false };
};
}
