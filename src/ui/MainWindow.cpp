#include "MainWindow.h"
#include "MainComponent.h"
#include <BinaryData.h>

namespace fizzle
{
namespace
{
const juce::Colour kWindowTop(0xff1a2538);
const juce::Colour kWindowBottom(0xff121b2a);
const juce::Colour kWindowBorder(0xffff9aa6);
const juce::Colour kWindowText(0xfff3f7ff);
const juce::Colour kWindowButtonSalmon(0xffff8d92);
const juce::Colour kWindowButtonSalmonSoft(0xffffadb2);

class WindowChromeButton final : public juce::Button
{
public:
    explicit WindowChromeButton(int buttonKind)
        : juce::Button("windowButton"), kind(buttonKind)
    {
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.5f);
        auto fill = kind == juce::DocumentWindow::closeButton ? kWindowButtonSalmon : kWindowButtonSalmonSoft;
        const float hoverAmount = isMouseOverButton ? 1.0f : 0.0f;
        if (isMouseOverButton)
            fill = fill.brighter(0.2f);
        if (isButtonDown)
            fill = fill.darker(0.22f);

        if (isMouseOverButton)
        {
            g.setColour(fill.withAlpha(0.26f + (isButtonDown ? 0.14f : 0.0f)));
            g.fillEllipse(b.expanded(1.7f));
        }

        g.setColour(fill.withAlpha(0.98f));
        g.fillEllipse(b);
        g.setColour(juce::Colours::white.withAlpha(0.55f + hoverAmount * 0.2f));
        g.drawEllipse(b, isButtonDown ? 1.8f : 1.2f);

        g.setColour(juce::Colour(0xff2a1f26).withAlpha(isMouseOverButton ? 0.96f : 0.84f));
        const auto cx = b.getCentreX();
        const auto cy = b.getCentreY();
        const auto w = b.getWidth();
        const auto h = b.getHeight();

        if (kind == juce::DocumentWindow::minimiseButton)
        {
            g.drawLine(cx - w * 0.22f, cy + h * 0.14f, cx + w * 0.22f, cy + h * 0.14f, 1.5f);
        }
        else if (kind == juce::DocumentWindow::maximiseButton)
        {
            g.drawLine(cx - w * 0.21f, cy, cx + w * 0.21f, cy, 1.4f);
            g.drawLine(cx, cy - h * 0.21f, cx, cy + h * 0.21f, 1.4f);
        }
        else
        {
            g.drawLine(cx - w * 0.2f, cy - h * 0.2f, cx + w * 0.2f, cy + h * 0.2f, 1.45f);
            g.drawLine(cx + w * 0.2f, cy - h * 0.2f, cx - w * 0.2f, cy + h * 0.2f, 1.45f);
        }
    }

private:
    int kind;
};
}

class MainWindow::WindowLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    WindowLookAndFeel()
    {
        logo = juce::ImageFileFormat::loadFrom(BinaryData::app_png, BinaryData::app_pngSize);
    }

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window,
                                    juce::Graphics& g,
                                    int width,
                                    int height,
                                    int titleSpaceX,
                                    int titleSpaceW,
                                    const juce::Image* icon,
                                    bool drawTitleTextOnLeft) override
    {
        juce::ignoreUnused(icon, drawTitleTextOnLeft);
        auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

        juce::Path chrome;
        chrome.addRoundedRectangle(r.getX() + 1.0f, r.getY() + 1.0f, r.getWidth() - 2.0f, r.getHeight() + 12.0f,
                                   18.0f, 18.0f, true, true, false, false);
        juce::ColourGradient fill(kWindowTop.withAlpha(0.97f), r.getX(), r.getY(),
                                  kWindowBottom.withAlpha(0.95f), r.getX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(chrome);

        g.setColour(kWindowBorder.withAlpha(0.10f));
        g.drawLine(12.0f, r.getBottom() - 1.0f, r.getRight() - 12.0f, r.getBottom() - 1.0f, 1.1f);

        auto iconBounds = juce::Rectangle<int>(titleSpaceX + 4, juce::jmax(2, (height - 20) / 2), 20, 20);
        if (logo.isValid())
            g.drawImageWithin(logo, iconBounds.getX(), iconBounds.getY(), iconBounds.getWidth(), iconBounds.getHeight(),
                              juce::RectanglePlacement::centred);
        else
        {
            g.setColour(kWindowButtonSalmon.withAlpha(0.95f));
            g.fillEllipse(iconBounds.toFloat());
        }

        juce::Font f(juce::FontOptions(17.0f, juce::Font::bold));
        f.setExtraKerningFactor(0.015f);
        g.setFont(f);
        g.setColour(kWindowText);
        auto titleBounds = juce::Rectangle<int>(iconBounds.getRight() + 8, 0, titleSpaceW - 24, height);
        g.drawText(window.getName(), titleBounds, juce::Justification::centredLeft, true);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        return new WindowChromeButton(buttonType);
    }

    void positionDocumentWindowButtons(juce::DocumentWindow& window,
                                       int titleBarX,
                                       int titleBarY,
                                       int titleBarW,
                                       int titleBarH,
                                       juce::Button* minimiseButton,
                                       juce::Button* maximiseButton,
                                       juce::Button* closeButton,
                                       bool buttonsOnLeft) override
    {
        juce::ignoreUnused(window, buttonsOnLeft);
        const int buttonSize = 20;
        const int y = titleBarY + (titleBarH - buttonSize) / 2;
        int x = titleBarX + titleBarW - 16;

        auto place = [&](juce::Button* b)
        {
            if (b == nullptr)
                return;
            x -= buttonSize;
            b->setBounds(x, y, buttonSize, buttonSize);
            x -= 8;
        };

        place(closeButton);
        place(maximiseButton);
        place(minimiseButton);
    }

private:
    juce::Image logo;
};

MainWindow::MainWindow(std::unique_ptr<MainComponent> content)
    : juce::DocumentWindow("Fizzle",
                           juce::Colours::black,
                           juce::DocumentWindow::allButtons),
      ownedContent(std::move(content))
{
    lookAndFeel = std::make_unique<WindowLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    setUsingNativeTitleBar(false);
    setTitleBarHeight(36);
    setResizable(true, false);
    setResizeLimits(980, 620, 1500, 900);
    setOpaque(false);
    setDropShadowEnabled(false);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::transparentBlack);
    setContentOwned(ownedContent.release(), true);
    centreWithSize(1100, 700);
}

MainWindow::~MainWindow()
{
    setLookAndFeel(nullptr);
}

MainComponent* MainWindow::getMainComponent() const
{
    return dynamic_cast<MainComponent*>(getContentComponent());
}

void MainWindow::closeButtonPressed()
{
    setVisible(false);
}
}
