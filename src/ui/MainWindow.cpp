#include "MainWindow.h"
#include "MainComponent.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#endif

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
    applyWindowsBackdrop(transparentBackgroundEnabled);
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

juce::BorderSize<int> MainWindow::getBorderThickness() const
{
    return { 10, 10, 10, 10 };
}

void MainWindow::setTransparentBackgroundEnabled(bool enabled)
{
    if (transparentBackgroundEnabled == enabled)
        return;

    transparentBackgroundEnabled = enabled;
    applyWindowsBackdrop(transparentBackgroundEnabled);
}

void MainWindow::applyWindowsBackdrop(bool transparent)
{
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
        {
            DWM_BLURBEHIND blur {};
            blur.dwFlags = DWM_BB_ENABLE;
            blur.fEnable = transparent ? TRUE : FALSE;
            DwmEnableBlurBehindWindow(hwnd, &blur);

            const int backdrop = transparent ? DWMSBT_MAINWINDOW : DWMSBT_NONE;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        }
    }
#else
    juce::ignoreUnused(transparent);
#endif
}
}
