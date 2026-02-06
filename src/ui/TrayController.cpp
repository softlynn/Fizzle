#include "TrayController.h"
#include <BinaryData.h>

namespace fizzle
{
namespace
{
const juce::Colour kTrayBg(0xff111a2a);
const juce::Colour kTrayPanel(0xff1b2940);
const juce::Colour kTrayText(0xffe9f1ff);
const juce::Colour kTrayMuted(0xff8aa1c8);
const juce::Colour kTrayAccent(0xff73c0ff);
const juce::Colour kTrayHover(0x3058a8ff);

class TrayMenuLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        juce::ColourGradient fill(kTrayPanel, r.getX(), r.getY(),
                                  kTrayBg, r.getX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(r.reduced(1.0f), 10.0f);
        g.setColour(kTrayAccent.withAlpha(0.24f));
        g.drawRoundedRectangle(r.reduced(1.2f), 9.6f, 1.2f);
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override
    {
        juce::ignoreUnused(icon, shortcutKeyText);
        auto r = area.reduced(4, 1).toFloat();

        if (isSeparator)
        {
            g.setColour(kTrayAccent.withAlpha(0.20f));
            g.drawLine(r.getX() + 6.0f, r.getCentreY(), r.getRight() - 6.0f, r.getCentreY(), 1.0f);
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(kTrayHover);
            g.fillRoundedRectangle(r, 7.5f);
            g.setColour(kTrayAccent.withAlpha(0.35f));
            g.drawRoundedRectangle(r, 7.5f, 1.0f);
        }

        auto colour = textColour != nullptr ? *textColour : (isActive ? kTrayText : kTrayMuted);
        if (! isActive)
            colour = colour.withAlpha(0.75f);
        g.setColour(colour);

        juce::Font f(juce::FontOptions(13.0f, juce::Font::plain));
        f.setExtraKerningFactor(0.012f);
        g.setFont(f);

        auto textArea = area.withTrimmedLeft(12).withTrimmedRight(12);
        if (isTicked)
        {
            juce::Path tick;
            const auto tx = static_cast<float>(textArea.getX());
            const auto ty = static_cast<float>(textArea.getCentreY());
            tick.startNewSubPath(tx, ty);
            tick.lineTo(tx + 4.0f, ty + 4.0f);
            tick.lineTo(tx + 10.0f, ty - 3.0f);
            g.strokePath(tick, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            textArea = textArea.withTrimmedLeft(14);
        }

        g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);

        if (hasSubMenu)
        {
            juce::Path p;
            const auto cx = static_cast<float>(area.getRight() - 12);
            const auto cy = static_cast<float>(area.getCentreY());
            p.startNewSubPath(cx - 3.0f, cy - 4.0f);
            p.lineTo(cx + 1.8f, cy);
            p.lineTo(cx - 3.0f, cy + 4.0f);
            g.strokePath(p, juce::PathStrokeType(1.5f));
        }
    }

    juce::Font getPopupMenuFont() override
    {
        juce::Font f(juce::FontOptions(13.0f, juce::Font::plain));
        f.setExtraKerningFactor(0.012f);
        return f;
    }
};
}

TrayController::TrayController(Listener& l) : listener(l)
{
    trayLookAndFeel = std::make_unique<TrayMenuLookAndFeel>();
    auto icon = createIcon();
    setIconImage(icon, icon);
}

juce::Image TrayController::createIcon() const
{
    auto logo = juce::ImageFileFormat::loadFrom(BinaryData::app_png, BinaryData::app_pngSize);
    if (logo.isValid())
        return logo.rescaled(32, 32, juce::Graphics::highResamplingQuality);

    juce::Image fallback(juce::Image::ARGB, 32, 32, true);
    juce::Graphics g(fallback);
    g.fillAll(juce::Colours::transparentBlack);
    g.setColour(juce::Colour(0xff2f99ff));
    g.fillEllipse(2.0f, 2.0f, 28.0f, 28.0f);
    return fallback;
}

void TrayController::showContextMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel(trayLookAndFeel.get());
    m.addItem("Fizzle v" + juce::String(FIZZLE_VERSION), false, false, nullptr);
    m.addSeparator();
    m.addItem("Open", [this] { listener.trayOpenRequested(); });
    m.addItem("Enable/Bypass", [this] { listener.trayToggleBypass(); });
    m.addItem("Mute", [this] { listener.trayToggleMute(); });

    juce::PopupMenu presetMenu;
    for (const auto& preset : listener.trayPresets())
        presetMenu.addItem(preset, [this, preset] { listener.trayPresetSelected(preset); });
    m.addSubMenu("Preset", presetMenu);

    m.addItem("Restart Audio", [this] { listener.trayRestartAudio(); });
    m.addSeparator();
    m.addItem("Quit Fizzle", [this] { listener.trayExit(); });

    m.showMenuAsync(juce::PopupMenu::Options(), [this](int) { juce::ignoreUnused(this); });
}

void TrayController::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void TrayController::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    listener.trayOpenRequested();
}
}
