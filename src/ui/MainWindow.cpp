#include "MainWindow.h"
#include "MainComponent.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
#define DWMWA_USE_HOSTBACKDROPBRUSH 17
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4
#endif
#ifndef WCA_ACCENT_POLICY
#define WCA_ACCENT_POLICY 19
#endif

struct ACCENT_POLICY
{
    int accentState;
    int accentFlags;
    int gradientColour;
    int animationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    int attrib;
    PVOID pvData;
    SIZE_T cbData;
};

enum ACCENT_STATE
{
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5
};

#endif

namespace fizzle
{
#if JUCE_WINDOWS
namespace
{
using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

SetWindowCompositionAttributeFn getSetWindowCompositionAttribute()
{
    static auto fn = reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    return fn;
}

void applyAccentBackdrop(HWND hwnd, bool transparent)
{
    const auto fn = getSetWindowCompositionAttribute();
    if (fn == nullptr || hwnd == nullptr)
        return;

    ACCENT_POLICY policy {};
    if (transparent)
    {
        policy.accentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
        policy.accentFlags = 0;
        policy.gradientColour = 0xC0281D14;
    }
    else
    {
        policy.accentState = ACCENT_DISABLED;
    }

    WINDOWCOMPOSITIONATTRIBDATA data {};
    data.attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);
    fn(hwnd, &data);
}
}
#endif

MainWindow::MainWindow(std::unique_ptr<MainComponent> content)
    : juce::DocumentWindow("Fizzle",
                           juce::Colours::black,
                           juce::DocumentWindow::allButtons),
      ownedContent(std::move(content))
{
    setUsingNativeTitleBar(false);
    setTitleBarHeight(0);
    // Keep JUCE corner resizer disabled; MainComponent draws/handles the
    // internal grip so there is only one visible resize affordance.
    setResizable(true, false);
    setResizeLimits(860, 560, 1500, 900);
    setOpaque(false);
    setDropShadowEnabled(false);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::transparentBlack);
    setContentOwned(ownedContent.release(), true);
    centreWithSize(1100, 700);
    applyRoundedWindowRegion();
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
    return {};
}

void MainWindow::setTransparentBackgroundEnabled(bool enabled)
{
    transparentBackgroundEnabled = enabled;
    applyRoundedWindowRegion();
    applyWindowsBackdrop(transparentBackgroundEnabled);
    repaint();
}

void MainWindow::visibilityChanged()
{
    juce::DocumentWindow::visibilityChanged();
    if (auto* main = getMainComponent())
    {
        if (isShowing())
            main->onWindowVisible();
        else
            main->onWindowHidden();
    }

    if (isShowing())
    {
        applyRoundedWindowRegion();
        applyWindowsBackdrop(transparentBackgroundEnabled);
    }
}

void MainWindow::resized()
{
    juce::DocumentWindow::resized();
    applyRoundedWindowRegion();
}

void MainWindow::inputAttemptWhenModal()
{
    if (auto* main = getMainComponent())
        main->inputAttemptWhenModal();
}

void MainWindow::applyWindowsBackdrop(bool transparent)
{
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
        {
            BOOL compositionEnabled = FALSE;
            DwmIsCompositionEnabled(&compositionEnabled);
            const bool useTransparent = transparent && compositionEnabled;

            DWM_BLURBEHIND blur {};
            blur.dwFlags = DWM_BB_ENABLE;
            blur.fEnable = useTransparent ? TRUE : FALSE;
            DwmEnableBlurBehindWindow(hwnd, &blur);

            MARGINS margins {};
            if (useTransparent)
            {
                margins.cxLeftWidth = -1;
                margins.cxRightWidth = -1;
                margins.cyTopHeight = -1;
                margins.cyBottomHeight = -1;
            }
            DwmExtendFrameIntoClientArea(hwnd, &margins);

            const BOOL hostBackdrop = useTransparent ? TRUE : FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_HOSTBACKDROPBRUSH, &hostBackdrop, sizeof(hostBackdrop));

            const int backdrop = useTransparent ? DWMSBT_TABBEDWINDOW : DWMSBT_NONE;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
            applyAccentBackdrop(hwnd, useTransparent);

            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
#else
    juce::ignoreUnused(transparent);
#endif
}

void MainWindow::applyRoundedWindowRegion()
{
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
        {
            RECT clientRect {};
            if (! GetClientRect(hwnd, &clientRect))
                return;

            const int width = clientRect.right - clientRect.left;
            const int height = clientRect.bottom - clientRect.top;
            if (width <= 0 || height <= 0)
                return;

            const int radius = 24;
            auto rgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
            if (rgn != nullptr)
                SetWindowRgn(hwnd, rgn, TRUE);
        }
    }
#endif
}
}
