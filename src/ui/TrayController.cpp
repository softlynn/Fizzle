#include "TrayController.h"
#include <BinaryData.h>

namespace fizzle
{
namespace
{
const juce::Image& getMenuNoiseTile()
{
    static juce::Image tile = []()
    {
        juce::Image img(juce::Image::ARGB, 48, 48, true);
        juce::Random random(0xF17713);
        for (int y = 0; y < img.getHeight(); ++y)
        {
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto v = static_cast<uint8_t>(130 + random.nextInt(21) - 10);
                img.setPixelAt(x, y, juce::Colour(v, v, v));
            }
        }
        return img;
    }();
    return tile;
}

class TrayMenuLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TrayMenuLookAndFeel()
    {
        applyAppearance({});
    }

    void applyAppearance(TrayController::Appearance appearance)
    {
        const auto lightMode = appearance.lightMode;
        const auto salmonTheme = appearance.themeVariant == 1;

        if (lightMode)
        {
            trayBg = juce::Colour(0xffe5ebf5);
            trayPanel = juce::Colour(0xffedf2f9);
            trayText = juce::Colour(0xff17253a);
            trayMuted = juce::Colour(0xff49617f);
            trayAccent = salmonTheme ? juce::Colour(0xffd9829d) : juce::Colour(0xff62a8e6);
        }
        else
        {
            trayBg = juce::Colour(0xff111a2a);
            trayPanel = juce::Colour(0xff1b2940);
            trayText = juce::Colour(0xffe9f1ff);
            trayMuted = juce::Colour(0xff8aa1c8);
            trayAccent = salmonTheme ? juce::Colour(0xffe78ea9) : juce::Colour(0xff73c0ff);
        }

        trayHover = trayAccent.withAlpha(lightMode ? 0.20f : 0.26f);
        switch (juce::jlimit(0, 2, appearance.uiDensity))
        {
            case 0: uiScale = 0.95f; break;
            case 2: uiScale = 1.22f; break;
            default: uiScale = 1.08f; break;
        }

        setColour(juce::PopupMenu::backgroundColourId, trayBg);
        setColour(juce::PopupMenu::textColourId, trayText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, trayHover.withAlpha(0.95f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    int getPopupMenuBorderSize() override
    {
        return 0;
    }

    int getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&) override
    {
        return 0;
    }

    int getMenuWindowFlags() override
    {
        return juce::ComponentPeer::windowIsTemporary
             | juce::ComponentPeer::windowIgnoresKeyPresses
             | juce::ComponentPeer::windowHasDropShadow
             | juce::ComponentPeer::windowIsSemiTransparent;
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        drawBackground(g, width, height);
    }

    void drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                            int width,
                                            int height,
                                            const juce::PopupMenu::Options&) override
    {
        drawBackground(g, width, height);
    }

    void drawBackground(juce::Graphics& g, int width, int height) const
    {
        g.fillAll(trayBg);
        auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

        juce::ColourGradient fill(trayPanel, r.getX(), r.getY(),
                                  trayBg, r.getX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRect(r);
        g.setTiledImageFill(getMenuNoiseTile(), 0, 0, 0.05f);
        g.fillRect(r);
        g.setColour(trayAccent.withAlpha(0.12f));
        g.drawRect(r.toNearestInt(), 1);
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
            g.setColour(trayAccent.withAlpha(0.20f));
            g.drawLine(r.getX() + 6.0f, r.getCentreY(), r.getRight() - 6.0f, r.getCentreY(), 1.0f);
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(trayHover);
            g.fillRoundedRectangle(r, 7.5f);
            g.setColour(trayAccent.withAlpha(0.35f));
            g.drawRoundedRectangle(r, 7.5f, 1.0f);
        }

        auto colour = textColour != nullptr ? *textColour : (isActive ? trayText : trayMuted);
        if (! isActive)
            colour = colour.withAlpha(0.75f);
        g.setColour(colour);

        juce::Font f(juce::FontOptions(13.0f * uiScale, juce::Font::plain));
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
        juce::Font f(juce::FontOptions(13.0f * uiScale, juce::Font::plain));
        f.setExtraKerningFactor(0.012f);
        return f;
    }

private:
    juce::Colour trayBg { 0xff111a2a };
    juce::Colour trayPanel { 0xff1b2940 };
    juce::Colour trayText { 0xffe9f1ff };
    juce::Colour trayMuted { 0xff8aa1c8 };
    juce::Colour trayAccent { 0xff73c0ff };
    juce::Colour trayHover { 0x3058a8ff };
    float uiScale { 1.0f };
};
}

TrayController::TrayController(Listener& l) : listener(l)
{
    trayLookAndFeel = std::make_unique<TrayMenuLookAndFeel>();
    if (auto* lf = dynamic_cast<TrayMenuLookAndFeel*>(trayLookAndFeel.get()))
        lf->applyAppearance(listener.trayAppearance());
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
    if (auto* lf = dynamic_cast<TrayMenuLookAndFeel*>(trayLookAndFeel.get()))
        lf->applyAppearance(listener.trayAppearance());

    juce::PopupMenu m;
    m.setLookAndFeel(trayLookAndFeel.get());
    m.addItem("Fizzle v" + juce::String(FIZZLE_VERSION), false, false, nullptr);
    m.addSeparator();
    m.addItem("Open", [this] { listener.trayOpenRequested(); });
    const auto effectsOn = listener.trayEffectsEnabled();
    const auto muted = listener.trayMuted();
    m.addItem(effectsOn ? "Effects: On" : "Effects: Off", true, effectsOn, [this] { listener.trayToggleBypass(); });
    m.addItem("Mute", true, muted, [this] { listener.trayToggleMute(); });

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
