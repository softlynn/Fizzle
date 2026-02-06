#include "MainWindow.h"
#include "MainComponent.h"

namespace fizzle
{
MainWindow::MainWindow(std::unique_ptr<MainComponent> content)
    : juce::DocumentWindow("Fizzle",
                           juce::Colours::black,
                           juce::DocumentWindow::allButtons),
      ownedContent(std::move(content))
{
    setUsingNativeTitleBar(false);
    setTitleBarHeight(0);
    setResizable(true, false);
    setResizeLimits(980, 620, 1500, 900);
    setOpaque(false);
    setDropShadowEnabled(false);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::transparentBlack);
    setContentOwned(ownedContent.release(), true);
    centreWithSize(1100, 700);
}

MainComponent* MainWindow::getMainComponent() const
{
    return dynamic_cast<MainComponent*>(getContentComponent());
}

void MainWindow::closeButtonPressed()
{
    setVisible(false);
}

juce::BorderSize<int> MainWindow::getContentComponentBorder() const
{
    return {};
}
}
