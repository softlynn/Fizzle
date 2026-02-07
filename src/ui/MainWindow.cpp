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

// Undocumented Windows composition API used as fallback when DWM backdrop alone
// does not provide true transparent+blurred client composition.
enum ACCENT_STATE
{
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY
{
    int accentState;
    int accentFlags;
    int gradientColor;
    int animationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    int attrib;
    PVOID data;
    SIZE_T dataSize;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

void applyAccentPolicy(HWND hwnd, bool transparent)
{
    auto* user32 = GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr)
        return;

    auto* setWindowCompositionAttribute = reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (setWindowCompositionAttribute == nullptr)
        return;

    ACCENT_POLICY accent {};
    accent.accentState = transparent ? ACCENT_ENABLE_BLURBEHIND : ACCENT_DISABLED;
    accent.accentFlags = transparent ? 2 : 0;
    accent.gradientColor = transparent ? static_cast<int>(0x10203040) : 0;
    accent.animationId = 0;

    WINDOWCOMPOSITIONATTRIBDATA data {};
    data.attrib = 19; // WCA_ACCENT_POLICY
    data.data = &accent;
    data.dataSize = sizeof(accent);
    setWindowCompositionAttribute(hwnd, &data);
}
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
    // Keep JUCE corner resizer disabled; MainComponent draws/handles the
    // internal grip so there is only one visible resize affordance.
    setResizable(true, false);
    setResizeLimits(860, 560, 1500, 900);
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
    return {};
}

void MainWindow::setTransparentBackgroundEnabled(bool enabled)
{
    transparentBackgroundEnabled = enabled;
    applyWindowsBackdrop(transparentBackgroundEnabled);
    repaint();
}

void MainWindow::visibilityChanged()
{
    juce::DocumentWindow::visibilityChanged();
    if (isShowing())
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

            MARGINS margins {};
            if (transparent)
            {
                margins.cxLeftWidth = -1;
                margins.cxRightWidth = -1;
                margins.cyTopHeight = -1;
                margins.cyBottomHeight = -1;
            }
            DwmExtendFrameIntoClientArea(hwnd, &margins);

            const BOOL hostBackdrop = transparent ? TRUE : FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_HOSTBACKDROPBRUSH, &hostBackdrop, sizeof(hostBackdrop));

            const int backdrop = transparent ? DWMSBT_MAINWINDOW : DWMSBT_NONE;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

            // Keep layered mode only in transparent mode to avoid extra
            // composition overhead while dragging in opaque mode.
            auto exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if (transparent)
                exStyle |= WS_EX_LAYERED;
            else
                exStyle &= ~WS_EX_LAYERED;
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
            if (transparent)
                SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

            applyAccentPolicy(hwnd, transparent);

            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
#else
    juce::ignoreUnused(transparent);
#endif
}
}
