#include "MainComponent.h"
#include "MainWindow.h"
#include "Theme.h"
#include "../core/Logger.h"
#include <BinaryData.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <thread>
#if JUCE_WINDOWS
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#endif

namespace fizzle
{
namespace
{
juce::Colour kUiBg(0xff0a111d);
juce::Colour kUiPanel(0xff172338);
juce::Colour kUiPanelSoft(0xff1e2d46);
juce::Colour kUiText(0xffeaf1ff);
juce::Colour kUiTextMuted(0xffa8bbdd);
juce::Colour kUiAccent(0xff6dbbff);
juce::Colour kUiAccentSoft(0xffa4ddff);
juce::Colour kUiMint(0xff9af7d8);
juce::Colour kUiSalmon(0xfff08ea2);
float kUiControlScale = 1.0f;
float kUiBackgroundAlphaScale = 0.84f;
const juce::String kGithubRepoUrl("https://github.com/softlynn/Fizzle");
const juce::String kWebsiteUrl("https://softlynn.github.io/Fizzle/");
constexpr std::array<int, 8> kBufferSizeOptions { 64, 96, 128, 192, 256, 384, 512, 1024 };

void populateBufferBox(juce::ComboBox& comboBox, int selectedBuffer)
{
    comboBox.clear();

    auto target = selectedBuffer > 0 ? selectedBuffer : kDefaultBlockSize;
    int selectedId = 0;
    int id = 1;
    for (const auto option : kBufferSizeOptions)
    {
        comboBox.addItem(juce::String(option), id);
        if (option == target)
            selectedId = id;
        ++id;
    }

    if (selectedId == 0)
    {
        comboBox.addItem(juce::String(target), id);
        selectedId = id;
    }

    comboBox.setSelectedId(selectedId, juce::dontSendNotification);
}

void syncBufferBoxSelection(juce::ComboBox& comboBox, int selectedBuffer)
{
    const auto target = juce::String(selectedBuffer > 0 ? selectedBuffer : kDefaultBlockSize);
    int existingId = 0;
    for (int i = 0; i < comboBox.getNumItems(); ++i)
    {
        if (comboBox.getItemText(i) == target)
        {
            existingId = comboBox.getItemId(i);
            break;
        }
    }
    if (existingId > 0)
    {
        comboBox.setSelectedId(existingId, juce::dontSendNotification);
    }
    else
    {
        const auto newId = comboBox.getNumItems() + 1;
        comboBox.addItem(target, newId);
        comboBox.setSelectedId(newId, juce::dontSendNotification);
    }
}

juce::Image createDitherNoiseTile()
{
    juce::Image img(juce::Image::ARGB, 96, 96, true);
    juce::Random random(0x5F177E);
    for (int y = 0; y < img.getHeight(); ++y)
    {
        for (int x = 0; x < img.getWidth(); ++x)
        {
            const auto n = random.nextInt(33) - 16;
            const auto v = static_cast<uint8_t>(128 + n);
            const auto b = static_cast<uint8_t>(126 + (n / 2));
            const auto a = static_cast<uint8_t>(18 + random.nextInt(24));
            img.setPixelAt(x, y, juce::Colour(v, v, b, a));
        }
    }
    return img;
}

const juce::Image& getDitherNoiseTile()
{
    static juce::Image tile = createDitherNoiseTile();
    return tile;
}

juce::String normalizeVersionTag(juce::String value)
{
    value = value.trim();
    while (value.startsWithChar('v') || value.startsWithChar('V'))
        value = value.substring(1);
    return value;
}

int compareSemver(const juce::String& lhs, const juce::String& rhs)
{
    auto a = normalizeVersionTag(lhs);
    auto b = normalizeVersionTag(rhs);
    auto pa = juce::StringArray::fromTokens(a, ".", {});
    auto pb = juce::StringArray::fromTokens(b, ".", {});
    const auto n = juce::jmax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i)
    {
        const auto va = i < pa.size() ? pa[i].getIntValue() : 0;
        const auto vb = i < pb.size() ? pb[i].getIntValue() : 0;
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

#if JUCE_WINDOWS
bool setRunAtStartupEnabled(bool enabled, bool launchToTray)
{
    constexpr auto subkey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr auto valueName = L"Fizzle";
    HKEY key = nullptr;
    const auto createResult = RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (createResult != ERROR_SUCCESS || key == nullptr)
        return false;

    bool ok = true;
    if (enabled)
    {
        auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName().replace("\"", "\\\"");
        auto command = "\"" + exe + "\"";
        if (launchToTray)
            command << " --tray";
        const auto data = command.toWideCharPointer();
        const auto bytes = static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t));
        ok = (RegSetValueExW(key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(data), bytes) == ERROR_SUCCESS);
    }
    else
    {
        const auto removeResult = RegDeleteValueW(key, valueName);
        ok = (removeResult == ERROR_SUCCESS || removeResult == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(key);
    return ok;
}
#endif

class ModernLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        refreshThemeColours();
    }

    void refreshThemeColours()
    {
        setColour(juce::ComboBox::backgroundColourId, kUiPanelSoft);
        setColour(juce::ComboBox::outlineColourId, kUiAccent.withAlpha(0.26f));
        setColour(juce::ComboBox::textColourId, kUiText);
        setColour(juce::TextButton::buttonColourId, kUiPanel);
        setColour(juce::TextButton::textColourOffId, kUiText);
        setColour(juce::TextButton::textColourOnId, kUiText);
        setColour(juce::Label::textColourId, kUiText);
        setColour(juce::ToggleButton::textColourId, kUiText);
        setColour(juce::Slider::trackColourId, kUiAccent.withAlpha(0.68f));
        setColour(juce::Slider::thumbColourId, kUiAccentSoft);
        setColour(juce::Slider::textBoxTextColourId, kUiText);
        setColour(juce::Slider::textBoxBackgroundColourId, kUiPanelSoft.withAlpha(0.9f));
        setColour(juce::Slider::textBoxOutlineColourId, kUiAccent.withAlpha(0.30f));
        setColour(juce::Slider::textBoxHighlightColourId, kUiAccent.withAlpha(0.20f));
        setColour(juce::TextEditor::backgroundColourId, kUiPanel.withAlpha(0.7f));
        setColour(juce::TextEditor::outlineColourId, kUiAccent.withAlpha(0.22f));
        setColour(juce::TextEditor::textColourId, kUiText);
        setColour(juce::AlertWindow::backgroundColourId, kUiPanel);
        setColour(juce::AlertWindow::outlineColourId, kUiAccent.withAlpha(0.35f));
        setColour(juce::AlertWindow::textColourId, kUiText);
        setColour(juce::PopupMenu::backgroundColourId, kUiPanel);
        setColour(juce::PopupMenu::textColourId, kUiText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kUiAccent.withAlpha(0.22f));
        setColour(juce::PopupMenu::highlightedTextColourId, kUiText);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool isMouseOver,
                              bool isButtonDown) override
    {
        const auto id = button.getComponentID();
        if (id.startsWith("win-"))
        {
            auto b = button.getLocalBounds().toFloat().reduced(1.5f, 4.0f);
            const auto radius = b.getHeight() * 0.46f;
            auto base = kUiPanelSoft.withAlpha(0.82f);
            auto border = kUiAccent.withAlpha(0.24f);
            if (id == "win-close")
            {
                base = kUiAccent.interpolatedWith(kUiSalmon, 0.35f).withAlpha(0.88f);
                border = kUiAccentSoft.withAlpha(0.36f);
            }
            if (isMouseOver)
            {
                base = base.brighter(0.10f);
                border = border.withAlpha(border.getFloatAlpha() + 0.14f);
            }
            if (isButtonDown)
                base = base.darker(0.12f);

            if (isMouseOver)
            {
                g.setColour(base.withAlpha(isButtonDown ? 0.16f : 0.11f));
                g.fillRoundedRectangle(b.expanded(2.0f, 1.4f), radius + 1.8f);
            }
            g.setColour(base);
            g.fillRoundedRectangle(b, radius);
            g.setColour(border);
            g.drawRoundedRectangle(b.reduced(0.2f), radius - 0.2f, isMouseOver ? 1.15f : 0.95f);
            return;
        }

        auto c = kUiPanel.withAlpha(0.94f);
        const bool lightPalette = kUiBg.getPerceivedBrightness() > 0.47f;
        if (button.getToggleState())
            c = kUiAccent.withAlpha(0.32f);
        if (isMouseOver)
        {
            if (lightPalette)
                c = c.interpolatedWith(kUiAccentSoft, button.getToggleState() ? 0.24f : 0.17f);
            else
                c = c.brighter(0.08f);
        }
        if (isButtonDown)
        {
            if (lightPalette)
                c = c.interpolatedWith(kUiAccent, button.getToggleState() ? 0.34f : 0.22f);
            else
                c = c.brighter(0.14f);
        }
        auto b = button.getLocalBounds().toFloat();
        juce::ColourGradient fill(c.brighter(0.03f), b.getX(), b.getY(),
                                  c.darker(0.1f), b.getX(), b.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(b, 11.0f);
        if (lightPalette && isMouseOver)
        {
            g.setColour(kUiAccent.withAlpha(isButtonDown ? 0.20f : 0.13f));
            g.fillRoundedRectangle(b.reduced(0.8f), 10.2f);
        }
        g.setColour(juce::Colours::white.withAlpha(lightPalette ? 0.12f : 0.05f));
        g.drawRoundedRectangle(b.reduced(0.7f), 10.3f, 0.9f);
        const auto outlineAlpha = button.getToggleState()
                                      ? (lightPalette ? 0.66f : 0.30f)
                                      : (lightPalette ? (isMouseOver ? 0.56f : 0.36f)
                                                      : (isMouseOver ? 0.24f : 0.14f));
        g.setColour(kUiAccent.withAlpha(outlineAlpha));
        g.drawRoundedRectangle(b.reduced(0.3f), 10.8f, isMouseOver ? (lightPalette ? 1.7f : 1.4f)
                                                                    : (lightPalette ? 1.25f : 1.1f));
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        const auto id = button.getComponentID();
        auto bounds = button.getLocalBounds();
        auto r = bounds.reduced(12, 0);
        const auto iconSize = 12.0f * kUiControlScale;
        auto icon = juce::Rectangle<float>(static_cast<float>(r.getX()),
                                           static_cast<float>(bounds.getCentreY() - iconSize * 0.5f),
                                           iconSize,
                                           iconSize);
        if (id.startsWith("win-"))
            icon = juce::Rectangle<float>(static_cast<float>(bounds.getCentreX() - iconSize * 0.5f),
                                          static_cast<float>(bounds.getCentreY() - iconSize * 0.5f),
                                          iconSize,
                                          iconSize);
        r.removeFromLeft(18);

        constexpr float stroke = 1.45f;

        if (! id.startsWith("win-"))
        {
            g.setColour(button.findColour(juce::TextButton::textColourOffId));
            juce::Font f(juce::FontOptions(13.0f * kUiControlScale, juce::Font::bold));
            f.setExtraKerningFactor(0.018f);
            g.setFont(f);
            g.drawFittedText(button.getButtonText(), r, juce::Justification::centredLeft, 1);
        }

        g.setColour(kUiAccentSoft);
        const auto x = icon.getX();
        const auto y = icon.getY();
        const auto w = icon.getWidth();
        const auto h = icon.getHeight();
        const auto cx = icon.getCentreX();
        const auto cy = icon.getCentreY();

        const auto baseGlyph = kUiText.contrasting(0.55f);
        const auto winGlyph = baseGlyph.withAlpha(button.isDown() ? 0.95f : (button.isOver() ? 0.90f : 0.78f));

        if (id == "save")
        {
            g.drawRoundedRectangle(icon.reduced(0.9f), 1.8f, stroke);
            g.fillRect(x + 3.0f, y + h * 0.58f, w - 6.0f, h * 0.24f);
        }
        else if (id == "restart" || id == "refresh" || id == "reset")
        {
            juce::Path arc;
            arc.addCentredArc(cx, cy, w * 0.38f, h * 0.38f, 0.0f, 0.6f, 5.8f, true);
            g.strokePath(arc, juce::PathStrokeType(stroke));
            juce::Path head;
            head.addTriangle(x + w - 2.6f, y + 3.4f, x + w - 0.9f, y + 5.3f, x + w - 3.2f, y + 5.8f);
            g.fillPath(head);
        }
        else if (id == "about")
        {
            g.drawEllipse(icon.reduced(0.8f), stroke);
            g.fillEllipse(cx - 0.9f, y + 2.2f, 1.8f, 1.8f);
            g.fillRect(cx - 0.85f, y + 5.4f, 1.7f, 4.0f);
        }
        else if (id == "tone")
        {
            juce::Path speaker;
            speaker.addTriangle(x + 1.6f, cy, x + 5.2f, y + 3.4f, x + 5.2f, y + h - 3.4f);
            g.fillPath(speaker);
            juce::Path wave;
            wave.addCentredArc(x + 7.2f, cy, 3.2f, 3.8f, 0.0f, 4.85f, 1.45f, true);
            g.strokePath(wave, juce::PathStrokeType(stroke - 0.25f));
        }
        else if (id == "effects")
        {
            juce::Path check;
            check.startNewSubPath(x + 1.8f, y + 6.6f);
            check.lineTo(x + 4.8f, y + 9.8f);
            check.lineTo(x + 10.2f, y + 2.6f);
            g.strokePath(check, juce::PathStrokeType(stroke + 0.15f));
        }
        else if (id == "listen")
        {
            juce::Path band;
            band.addCentredArc(cx, cy - 0.4f, w * 0.37f, h * 0.37f, 0.0f,
                               juce::MathConstants<float>::pi * 1.02f, juce::MathConstants<float>::twoPi * 0.98f, true);
            g.strokePath(band, juce::PathStrokeType(stroke));
            g.drawRoundedRectangle(x + 1.0f, cy - 0.5f, 2.6f, 4.0f, 0.8f, stroke);
            g.drawRoundedRectangle(x + w - 3.6f, cy - 0.5f, 2.6f, 4.0f, 0.8f, stroke);
            g.drawLine(x + 3.6f, y + h - 1.9f, x + w - 3.6f, y + h - 1.9f, 0.9f);
        }
        else if (id == "remove")
        {
            g.drawLine(x + 2.3f, y + 2.3f, x + w - 2.3f, y + h - 2.3f, stroke + 0.15f);
            g.drawLine(x + w - 2.3f, y + 2.3f, x + 2.3f, y + h - 2.3f, stroke + 0.15f);
        }
        else if (id == "settings")
        {
            g.drawEllipse(icon.reduced(1.2f), stroke);
            g.fillEllipse(cx - 1.25f, cy - 1.25f, 2.5f, 2.5f);
            g.drawLine(cx, y + 0.9f, cx, y + 2.1f, 0.9f);
            g.drawLine(cx, y + h - 2.1f, cx, y + h - 0.9f, 0.9f);
        }
        else if (id == "edit")
        {
            juce::Path pen;
            pen.startNewSubPath(x + 2.0f, y + h - 2.3f);
            pen.lineTo(x + w - 3.0f, y + 2.2f);
            g.strokePath(pen, juce::PathStrokeType(stroke));
            juce::Path nib;
            nib.addTriangle(x + w - 3.0f, y + 2.1f, x + w - 1.4f, y + 3.8f, x + w - 3.7f, y + 4.2f);
            g.fillPath(nib);
        }
        else if (id == "win-min")
        {
            g.setColour(winGlyph);
            g.drawLine(cx - 2.8f, cy + 1.8f, cx + 2.8f, cy + 1.8f, 1.5f);
        }
        else if (id == "win-max")
        {
            g.setColour(winGlyph);
            g.drawLine(cx - 2.8f, cy, cx + 2.8f, cy, 1.45f);
            g.drawLine(cx, cy - 2.8f, cx, cy + 2.8f, 1.45f);
        }
        else if (id == "win-close")
        {
            g.setColour(winGlyph);
            g.drawLine(cx - 2.8f, cy - 2.8f, cx + 2.8f, cy + 2.8f, 1.55f);
            g.drawLine(cx + 2.8f, cy - 2.8f, cx - 2.8f, cy + 2.8f, 1.55f);
        }
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override
    {
        juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);
        auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        auto base = box.isEnabled() ? kUiPanelSoft.withAlpha(0.85f) : kUiPanelSoft.withAlpha(0.65f);
        if (box.hasKeyboardFocus(true) || isButtonDown)
            base = base.brighter(0.08f);

        juce::ColourGradient fill(base.brighter(0.03f), r.getX(), r.getY(),
                                  base.darker(0.12f), r.getX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(r, 9.0f);

        g.setColour(kUiAccent.withAlpha(box.hasKeyboardFocus(true) ? 0.45f : 0.22f));
        g.drawRoundedRectangle(r.reduced(0.4f), 8.6f, box.hasKeyboardFocus(true) ? 1.6f : 1.0f);

        auto arrow = juce::Path();
        const auto cx = r.getRight() - 22.0f;
        const auto cy = r.getCentreY();
        arrow.startNewSubPath(cx - 6.0f, cy - 2.0f);
        arrow.lineTo(cx, cy + 3.8f);
        arrow.lineTo(cx + 6.0f, cy - 2.0f);
        g.setColour(kUiText.withAlpha(0.88f));
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        auto b = box.getLocalBounds().reduced(12, 0);
        label.setBounds(b.withTrimmedRight(28));
        juce::Font f(juce::FontOptions(14.0f * kUiControlScale, juce::Font::plain));
        f.setExtraKerningFactor(0.01f);
        label.setFont(f);
    }

    void drawTickBox(juce::Graphics& g,
                     juce::Component&,
                     float x,
                     float y,
                     float w,
                     float h,
                     bool ticked,
                     bool isEnabled,
                     bool isMouseOverButton,
                     bool isButtonDown) override
    {
        juce::ignoreUnused(isButtonDown);
        auto box = juce::Rectangle<float>(x, y, w, h).reduced(1.4f);
        auto fill = kUiPanelSoft.withAlpha(isEnabled ? 0.92f : 0.6f);
        if (isMouseOverButton) fill = fill.brighter(0.08f);
        g.setColour(fill);
        g.fillRoundedRectangle(box, 4.0f);
        g.setColour(kUiAccent.withAlpha(ticked ? 0.62f : 0.24f));
        g.drawRoundedRectangle(box, 4.0f, ticked ? 1.6f : 1.0f);

        if (ticked)
        {
            juce::Path p;
            p.startNewSubPath(box.getX() + 3.0f, box.getCentreY());
            p.lineTo(box.getX() + 6.0f, box.getBottom() - 3.0f);
            p.lineTo(box.getRight() - 3.0f, box.getY() + 3.0f);
            g.setColour(kUiAccentSoft);
            g.strokePath(p, juce::PathStrokeType(1.65f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos, style, slider);
        const bool lightPalette = kUiBg.getPerceivedBrightness() > 0.47f;
        auto track = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y + height / 2 - 3), static_cast<float>(width), 6.0f);
        g.setColour((lightPalette ? kUiPanelSoft.darker(0.12f) : kUiPanelSoft).withAlpha(0.95f));
        g.fillRoundedRectangle(track, 3.0f);
        g.setColour(kUiAccent.withAlpha(lightPalette ? 0.30f : 0.20f));
        g.drawRoundedRectangle(track, 3.0f, 1.0f);

        auto active = track.withRight(sliderPos);
        juce::ColourGradient fill(kUiAccent, active.getX(), active.getY(),
                                  kUiMint, active.getRight(), active.getY(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(active, 3.0f);

        auto knob = juce::Rectangle<float>(sliderPos - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour(kUiAccentSoft);
        g.fillEllipse(knob);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawEllipse(knob, 1.1f);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        juce::ignoreUnused(slider);
        auto r = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
        auto radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
        auto centre = r.getCentre();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        auto knob = juce::Rectangle<float>(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(kUiPanel.withAlpha(0.95f));
        g.fillEllipse(knob);
        g.setColour(kUiAccent.withAlpha(0.26f));
        g.drawEllipse(knob, 1.1f);

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, radius - 2.6f, radius - 2.6f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(kUiAccentSoft);
        g.strokePath(valueArc, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path thumb;
        thumb.addRectangle(-1.3f, -radius + 5.5f, 2.6f, radius * 0.44f);
        g.setColour(kUiMint);
        g.fillPath(thumb, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    }
};

class SettingsOverlay final : public juce::Component
{
public:
    explicit SettingsOverlay(std::function<void()> dismissFn, std::function<void(int)> scrollFn)
        : dismiss(std::move(dismissFn)), scroll(std::move(scrollFn))
    {
    }

    void setPanelBounds(juce::Rectangle<int> bounds)
    {
        panelBounds = bounds.toFloat();
        repaint();
    }

    void setSidebarWidth(float width)
    {
        sidebarWidth = juce::jmax(120.0f, width);
        repaint();
    }

    juce::Rectangle<float> getPanelBounds() const
    {
        if (! panelBounds.isEmpty())
            return panelBounds;
        return getLocalBounds().toFloat().reduced(80.0f, 56.0f);
    }

    void paint(juce::Graphics& g) override
    {
        auto shell = getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        juce::Path shellPath;
        shellPath.addRoundedRectangle(shell, 24.0f);
        g.reduceClipRegion(shellPath);

        g.setColour(juce::Colours::black.withAlpha(0.36f));
        g.fillPath(shellPath);

        auto panel = getPanelBounds();
        juce::ColourGradient fill(kUiPanelSoft.withAlpha(0.96f), panel.getX(), panel.getY(),
                                  kUiPanel.withAlpha(0.93f), panel.getX(), panel.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(panel, 18.0f);
        g.setColour(kUiAccent.withAlpha(0.58f));
        g.drawRoundedRectangle(panel, 18.0f, 1.4f);
        auto sidebar = panel.removeFromLeft(juce::jmin(sidebarWidth, panel.getWidth() * 0.48f));
        g.setColour(kUiBg.withAlpha(0.78f));
        g.fillRoundedRectangle(sidebar, 16.0f);
        g.setColour(kUiAccent.withAlpha(0.22f));
        g.drawLine(sidebar.getRight(), panel.getY() + 8.0f, sidebar.getRight(), panel.getBottom() - 8.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! getPanelBounds().contains(e.position) && dismiss != nullptr)
            dismiss();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (! getPanelBounds().contains(e.position) || scroll == nullptr)
            return;

        const auto delta = juce::roundToInt(-wheel.deltaY * 96.0f);
        if (delta != 0)
            scroll(delta);
    }

private:
    std::function<void()> dismiss;
    std::function<void(int)> scroll;
    juce::Rectangle<float> panelBounds;
    float sidebarWidth { 180.0f };
};

class PluginEditorWindow final : public juce::DocumentWindow
{
public:
    explicit PluginEditorWindow(juce::AudioProcessorEditor* editor, std::function<void()> onCloseFn)
        : juce::DocumentWindow("Plugin Editor", juce::Colour(0xff0f141d), juce::DocumentWindow::closeButton)
        , onClose(std::move(onCloseFn))
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(editor, true);
        centreWithSize(juce::jmax(500, editor->getWidth()), juce::jmax(300, editor->getHeight()));
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        if (onClose != nullptr)
            juce::MessageManager::callAsync([cb = onClose] { cb(); });
    }

private:
    std::function<void()> onClose;
};

class VstRowComponent final : public juce::Component,
                              private juce::Button::Listener,
                              private juce::Slider::Listener,
                              private juce::Timer
{
public:
    using DragStart = std::function<void(int)>;
    using DragHover = std::function<void(int)>;
    using DragEnd = std::function<void(int)>;
    using OpenFn = std::function<void(int)>;
    using MixFn = std::function<void(int, float)>;
    using ToggleFn = std::function<void(int)>;
    using MenuFn = std::function<void(int, juce::Point<int>)>;

    VstRowComponent(DragStart ds, DragHover dh, DragEnd de, OpenFn op, MixFn mx, ToggleFn tg, MenuFn mf)
        : dragStart(std::move(ds)), dragHover(std::move(dh)), dragEnd(std::move(de)), open(std::move(op)), setMix(std::move(mx)), toggle(std::move(tg)), showMenu(std::move(mf))
    {
        addAndMakeVisible(name);
        addAndMakeVisible(mix);
        addAndMakeVisible(mixLabel);
        addAndMakeVisible(enabled);
        addAndMakeVisible(openButton);

        mix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        mix.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        mix.setRange(0.0, 1.0, 0.01);
        mix.addListener(this);
        mix.onDragStart = [this]
        {
            if (auto* lb = findParentComponentOfClass<juce::ListBox>())
                lb->selectRow(row);
        };

        enabled.setButtonText("On");
        enabled.addListener(this);
        openButton.setButtonText("Edit");
        openButton.setComponentID("edit");
        openButton.addListener(this);
        mixLabel.setText("Wet/Dry", juce::dontSendNotification);
        mixLabel.setJustificationType(juce::Justification::centredRight);
        mixLabel.setColour(juce::Label::textColourId, kUiTextMuted);
        mixLabel.setFont(juce::FontOptions(10.5f, juce::Font::plain));
        name.setJustificationType(juce::Justification::centredLeft);
        name.setInterceptsMouseClicks(false, false);
        addMouseListener(this, true);
    }

    void setRowData(int rowIn, const juce::String& pluginName, bool isEnabled, float mixAmount, bool selectedIn)
    {
        row = rowIn;
        selected = selectedIn;
        name.setText(pluginName, juce::dontSendNotification);
        enabled.setToggleState(isEnabled, juce::dontSendNotification);
        mix.setValue(mixAmount, juce::dontSendNotification);
        juce::Font nameFont(juce::FontOptions(14.8f * kUiControlScale, juce::Font::plain));
        nameFont.setExtraKerningFactor(0.014f);
        name.setFont(nameFont);
        juce::Font mixFont(juce::FontOptions(11.4f * kUiControlScale, juce::Font::plain));
        mixFont.setExtraKerningFactor(0.01f);
        mixLabel.setFont(mixFont);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6, 3);
        enabled.setBounds(r.removeFromLeft(juce::roundToInt(52.0f * kUiControlScale)));
        auto knobArea = r.removeFromRight(juce::roundToInt(142.0f * kUiControlScale));
        auto knobBox = knobArea.removeFromRight(juce::roundToInt(54.0f * kUiControlScale));
        mix.setBounds(knobBox.withSizeKeepingCentre(juce::roundToInt(48.0f * kUiControlScale),
                                                    juce::roundToInt(48.0f * kUiControlScale)));
        mixLabel.setBounds(knobArea.withTrimmedTop(10));
        openButton.setBounds(r.removeFromRight(juce::roundToInt(76.0f * kUiControlScale)));
        name.setBounds(r);
    }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool lightPalette = kUiBg.getPerceivedBrightness() > 0.47f;
        auto fill = (lightPalette ? kUiPanel.brighter(0.08f) : kUiPanelSoft).withAlpha(lightPalette ? 0.92f : 0.78f);
        auto border = kUiAccent.withAlpha(lightPalette ? 0.24f : 0.18f);
        if (selected)
        {
            fill = (lightPalette ? kUiPanelSoft.brighter(0.18f) : kUiPanel).withAlpha(0.94f);
            border = kUiAccent.withAlpha(lightPalette ? 0.64f : 0.85f);
        }
        if (dragging)
        {
            fill = fill.brighter(0.08f);
            border = kUiAccentSoft;
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRoundedRectangle(b.translated(0.0f, 2.0f), 10.0f);
        }
        g.setColour(fill);
        g.fillRoundedRectangle(b, 10.0f);
        g.setColour(border);
        g.drawRoundedRectangle(b, 10.0f, dragging ? 2.2f : (selected ? 2.0f : 1.0f));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (auto* lb = findParentComponentOfClass<juce::ListBox>())
            lb->selectRow(row);
        pointerDownWasRight = e.mods.isRightButtonDown();
        if (pointerDownWasRight)
        {
            if (! isEventFromInteractiveControl(e) && showMenu)
                showMenu(row, e.getScreenPosition());
            return;
        }
        pointerDownWasLeft = e.mods.isLeftButtonDown();
        if (! pointerDownWasLeft)
            return;
        if (isEventFromInteractiveControl(e))
            return;
        dragging = true;
        setInterceptsMouseClicks(true, true);
        setAlwaysOnTop(true);
        toFront(true);
        startTimerHz(60);
        if (dragStart)
            dragStart(row);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;
        dragOffsetY = static_cast<float>(e.getDistanceFromDragStartY());
        targetScale = 0.94f;
        setTransform(juce::AffineTransform::translation(0.0f, dragOffsetY)
                        .scaled(currentScale, currentScale, getWidth() * 0.5f, getHeight() * 0.5f));
        if (auto* lb = findParentComponentOfClass<juce::ListBox>())
        {
            const auto rel = e.getEventRelativeTo(lb);
            const auto r = lb->getRowContainingPosition(static_cast<int>(rel.position.x),
                                                        static_cast<int>(rel.position.y));
            if (r >= 0 && dragHover)
                dragHover(r);
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (! pointerDownWasLeft)
            return;

        if (! dragging)
            return;
        const auto dragDistance = std::abs(e.getDistanceFromDragStartY());
        if (dragDistance < 4)
        {
            dragging = false;
            dragOffsetY = 0.0f;
            targetScale = 1.0f;
            setAlwaysOnTop(false);
            startTimerHz(60);
            repaint();
            return;
        }
        auto targetRow = row;
        if (auto* lb = findParentComponentOfClass<juce::ListBox>())
        {
            const auto rel = e.getEventRelativeTo(lb);
            const auto r = lb->getRowContainingPosition(static_cast<int>(rel.position.x),
                                                        static_cast<int>(rel.position.y));
            if (r >= 0)
                targetRow = r;
        }
        dragging = false;
        dragOffsetY = 0.0f;
        targetScale = 1.0f;
        setAlwaysOnTop(false);
        startTimerHz(60);
        if (dragEnd)
            dragEnd(targetRow);
        repaint();
    }

private:
    int row { -1 };
    bool selected { false };
    bool dragging { false };
    bool pointerDownWasLeft { false };
    bool pointerDownWasRight { false };
    float dragOffsetY { 0.0f };
    float currentScale { 1.0f };
    float targetScale { 1.0f };
    juce::Label name;
    juce::Label mixLabel;
    juce::Slider mix;
    juce::ToggleButton enabled;
    juce::TextButton openButton;

    DragStart dragStart;
    DragHover dragHover;
    DragEnd dragEnd;
    OpenFn open;
    MixFn setMix;
    ToggleFn toggle;
    MenuFn showMenu;

    bool isEventFromInteractiveControl(const juce::MouseEvent& e) const
    {
        for (auto* c = e.eventComponent; c != nullptr; c = c->getParentComponent())
        {
            if (c == this)
                break;
            if (c == &mix || c == &enabled || c == &openButton)
                return true;
        }
        return false;
    }

    void buttonClicked(juce::Button* b) override
    {
        if (row < 0)
            return;

        if (auto* lb = findParentComponentOfClass<juce::ListBox>())
            lb->selectRow(row);

        if (b == &openButton)
            open(row);
        else if (b == &enabled)
            toggle(row);
    }

    void sliderValueChanged(juce::Slider* s) override
    {
        if (row >= 0 && s == &mix)
            setMix(row, static_cast<float>(mix.getValue()));
    }

    void timerCallback() override
    {
        currentScale += (targetScale - currentScale) * 0.28f;
        if (std::abs(targetScale - currentScale) < 0.001f)
            currentScale = targetScale;
        if (dragging || currentScale != 1.0f)
        {
            if (dragging)
                toFront(false);
            setTransform(juce::AffineTransform::translation(0.0f, dragOffsetY)
                            .scaled(currentScale, currentScale, getWidth() * 0.5f, getHeight() * 0.5f));
            repaint();
        }
        else
        {
            setTransform(juce::AffineTransform());
            stopTimer();
        }
    }
};
}

class MainComponent::ProgramListModel final : public juce::ListBoxModel
{
public:
    std::function<int()> rowCount;
    std::function<void(int, juce::Graphics&, int, int, bool)> paintRow;
    std::function<void(int)> rowClicked;

    int getNumRows() override
    {
        return rowCount != nullptr ? rowCount() : 0;
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (paintRow != nullptr)
            paintRow(rowNumber, g, width, height, rowIsSelected);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (rowClicked != nullptr)
            rowClicked(row);
    }
};

MainComponent::MainComponent(AudioEngine& engineRef, SettingsStore& settingsRef, PresetStore& presetsRef, EffectParameters& paramsRef)
    : engine(engineRef), settingsStore(settingsRef), presetStore(presetsRef), params(paramsRef), diagnostics(engineRef)
{
    static ModernLookAndFeel modern;
    setLookAndFeel(&modern);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    addMouseListener(this, true);
    appLogo = juce::ImageFileFormat::loadFrom(BinaryData::app_png, BinaryData::app_pngSize);

    title.setText("fizzle", juce::dontSendNotification);
    juce::Font titleFont(juce::FontOptions(25.0f, juce::Font::plain));
    titleFont.setTypefaceStyle("SemiBold");
    titleFont.setExtraKerningFactor(0.042f);
    title.setFont(titleFont);
    title.setColour(juce::Label::textColourId, kUiText);
    addAndMakeVisible(title);
    title.setVisible(true);

    inputLabel.setText("Input Mic", juce::dontSendNotification);
    outputLabel.setText("Fizzle Mic Output", juce::dontSendNotification);
    bufferLabel.setText("Buffer Size", juce::dontSendNotification);
    sampleRateLabel.setText("Sample Rate", juce::dontSendNotification);
    sampleRateValueLabel.setText("48.0 kHz", juce::dontSendNotification);
    presetLabel.setText("Preset", juce::dontSendNotification);
    appEnableLabel.setText("Program Auto-Enable", juce::dontSendNotification);
    appearanceLabel.setText("Appearance", juce::dontSendNotification);
    appSearchLabel.setText("Search Programs", juce::dontSendNotification);
    appListLabel.setText("Programs", juce::dontSendNotification);
    enabledProgramsLabel.setText("Enabled Programs", juce::dontSendNotification);
    updatesLabel.setText("Updates", juce::dontSendNotification);
    updatesLinksLabel.setText("Quick links", juce::dontSendNotification);
    updatesLinksLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    startupLabel.setText("Behavior", juce::dontSendNotification);
    startupHintLabel.setText("Listen routing, plugin search folders, and startup controls.", juce::dontSendNotification);
    startupHintLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    startupHintLabel.setJustificationType(juce::Justification::topLeft);
    appearanceModeLabel.setText("Mode", juce::dontSendNotification);
    appearanceThemeLabel.setText("Theme", juce::dontSendNotification);
    appearanceBackgroundLabel.setText("Background Opacity", juce::dontSendNotification);
    appearanceSizeLabel.setText("UI Size", juce::dontSendNotification);
    updateStatusLabel.setText("Auto-install is off.", juce::dontSendNotification);
    updateStatusLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    updateStatusLabel.setJustificationType(juce::Justification::topLeft);
    updateStatusLabel.setMinimumHorizontalScale(0.82f);
    settingsNavLabel.setText("Settings", juce::dontSendNotification);
    juce::Font navFont(juce::FontOptions(16.0f, juce::Font::bold));
    navFont.setExtraKerningFactor(0.02f);
    settingsNavLabel.setFont(navFont);
    updatesLabel.setColour(juce::Label::textColourId, kUiText);
    juce::Font sectionLabelFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabelFont.setExtraKerningFactor(0.01f);
    updatesLabel.setFont(sectionLabelFont);
    updatesLinksLabel.setFont(juce::FontOptions(12.5f));
    startupLabel.setFont(sectionLabelFont);
    startupHintLabel.setFont(juce::FontOptions(12.5f));
    currentPresetLabel.setText("Current: Default", juce::dontSendNotification);
    virtualMicStatusLabel.setText("Fizzle Mic: not routed", juce::dontSendNotification);
    effectsHintLabel.setText({}, juce::dontSendNotification);
    effectsHintLabel.setColour(juce::Label::textColourId, kUiAccentSoft);
    juce::Font hintStatusFont(juce::FontOptions(12.5f, juce::Font::plain));
    hintStatusFont.setExtraKerningFactor(0.01f);
    effectsHintLabel.setFont(hintStatusFont);
    effectsHintLabel.setJustificationType(juce::Justification::centredRight);
    dragHintLabel.setText("Drag VST rows to reorder", juce::dontSendNotification);
    dragHintLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    juce::Font hintFont(juce::FontOptions(12.0f));
    hintFont.setExtraKerningFactor(0.01f);
    dragHintLabel.setFont(hintFont);
    for (auto* l : { &inputLabel, &outputLabel, &bufferLabel, &sampleRateLabel, &sampleRateValueLabel, &presetLabel, &currentPresetLabel, &virtualMicStatusLabel, &effectsHintLabel })
    {
        juce::Font labelFont(juce::FontOptions(15.0f));
        labelFont.setExtraKerningFactor(0.012f);
        l->setFont(labelFont);
        addAndMakeVisible(l);
    }
    effectsHintLabel.setFont(hintStatusFont);
    addAndMakeVisible(dragHintLabel);

    juce::Font statusFont(juce::FontOptions(12.0f));
    statusFont.setExtraKerningFactor(0.01f);
    updateStatusLabel.setFont(statusFont);

    versionLabel.setText("v" + juce::String(FIZZLE_VERSION), juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.18f));
    juce::Font versionFont(juce::FontOptions(11.0f));
    versionFont.setExtraKerningFactor(0.07f);
    versionLabel.setFont(versionFont);
    addAndMakeVisible(versionLabel);

    for (auto* c : { &inputBox, &outputBox, &bufferBox, &presetBox, &vstAvailableBox })
    {
        addAndMakeVisible(c);
        c->addListener(this);
    }

    addAndMakeVisible(vstChainList);
    vstChainList.setModel(this);
    vstChainList.setRowHeight(56);
    vstChainList.setTooltip("Drag plugin rows to reorder");

    savePresetButton.setButtonText("Save Preset");
    deletePresetButton.setButtonText("Delete Preset");
    removeVstButton.setButtonText("Remove VST");
    restartButton.setButtonText("Restart Audio");
    aboutButton.setButtonText("About");
    toneButton.setButtonText("Test Tone");
    effectsToggle.setButtonText("Effects On");
    routeSpeakersButton.setButtonText("Listen");
    settingsButton.setButtonText("Settings");
    windowMinButton.setButtonText({});
    windowMaxButton.setButtonText({});
    windowCloseButton.setButtonText({});
    savePresetButton.setComponentID("save");
    deletePresetButton.setComponentID("remove");
    restartButton.setComponentID("restart");
    aboutButton.setComponentID("about");
    toneButton.setComponentID("tone");
    effectsToggle.setComponentID("effects");
    routeSpeakersButton.setComponentID("listen");
    removeVstButton.setComponentID("remove");
    settingsButton.setComponentID("settings");
    windowMinButton.setComponentID("win-min");
    windowMaxButton.setComponentID("win-max");
    windowCloseButton.setComponentID("win-close");

    const std::array<juce::Button*, 14> buttons {
        &savePresetButton, &deletePresetButton, &rescanVstButton, &autoScanVstButton, &removeVstButton,
        &routeSpeakersButton, &restartButton, &aboutButton, &toneButton, &effectsToggle, &settingsButton,
        &windowMinButton, &windowMaxButton, &windowCloseButton
    };
    for (auto* b : buttons)
    {
        addAndMakeVisible(b);
        b->addListener(this);
    }
    refreshAppsButton.setButtonText("Refresh");
    refreshAppsButton.setComponentID("refresh");
    removeProgramButton.setButtonText("Remove Selected");
    removeProgramButton.setComponentID("remove");
    checkUpdatesButton.setButtonText("Check Now");
    checkUpdatesButton.setComponentID("refresh");
    updatesGithubButton.setButtonText("GitHub Repo");
    updatesWebsiteButton.setButtonText("Website");
    updatesGithubButton.setComponentID("about");
    updatesWebsiteButton.setComponentID("about");
    settingsNavAutoEnableButton.setClickingTogglesState(true);
    settingsNavAppearanceButton.setClickingTogglesState(true);
    settingsNavUpdatesButton.setClickingTogglesState(true);
    settingsNavStartupButton.setClickingTogglesState(true);
    settingsNavStartupButton.setButtonText("Behavior");
    settingsNavAutoEnableButton.setToggleState(true, juce::dontSendNotification);
    settingsNavAppearanceButton.setToggleState(false, juce::dontSendNotification);
    settingsNavUpdatesButton.setToggleState(false, juce::dontSendNotification);
    settingsNavStartupButton.setToggleState(false, juce::dontSendNotification);
    autoEnableToggle.setButtonText("Auto Enable by Program");
    autoDownloadUpdatesToggle.setButtonText("Auto-install new updates");
    startWithWindowsToggle.setButtonText("Start with Windows");
    startMinimizedToggle.setButtonText("Start minimized to tray");
    followAutoEnableWindowToggle.setButtonText("Open/close window with Program Auto-Enable");
    behaviorListenDeviceLabel.setText("Listen Output Device", juce::dontSendNotification);
    behaviorVstFoldersLabel.setText("VST Search Folders", juce::dontSendNotification);
    lightModeToggle.setButtonText("Light mode");

    appearanceThemeBox.addItem("Aqua", 1);
    appearanceThemeBox.addItem("Salmon", 2);
    appearanceBackgroundBox.addItem("Transparent (Beta)", 1);
    appearanceBackgroundBox.addItem("Opaque", 2);
    appearanceSizeBox.addItem("Small (Compact)", 1);
    appearanceSizeBox.addItem("Normal", 2);
    appearanceSizeBox.addItem("Large", 3);
    behaviorListenDeviceBox.setTextWhenNothingSelected("Select device...");
    behaviorListenDeviceBox.addListener(this);
    appSearchEditor.setTextToShowWhenEmpty("Type to search programs...", juce::Colour(0xff9aa7b6));
    appSearchEditor.onTextChange = [this]
    {
        refreshProgramsList();
    };
    appPathEditor.setTextToShowWhenEmpty("Or set exact executable path (e.g. C:\\Games\\VRChat\\VRChat.exe)", juce::Colour(0xff9aa7b6));
    appPathEditor.onTextChange = [this]
    {
        cachedSettings.autoEnableProcessPath = appPathEditor.getText().trim();
        saveCachedSettings();
    };
    appPathEditor.onReturnKey = [this]
    {
        addAutoEnableProgramFromPath(appPathEditor.getText());
    };
    browseAppPathButton.addListener(this);
    removeProgramButton.addListener(this);
    closeSettingsButton.addListener(this);
    autoEnableToggle.addListener(this);
    refreshAppsButton.addListener(this);
    autoDownloadUpdatesToggle.addListener(this);
    lightModeToggle.addListener(this);
    checkUpdatesButton.addListener(this);
    settingsNavAutoEnableButton.addListener(this);
    settingsNavAppearanceButton.addListener(this);
    settingsNavUpdatesButton.addListener(this);
    settingsNavStartupButton.addListener(this);
    updatesGithubButton.addListener(this);
    updatesWebsiteButton.addListener(this);
    startWithWindowsToggle.addListener(this);
    startMinimizedToggle.addListener(this);
    followAutoEnableWindowToggle.addListener(this);
    appearanceThemeBox.addListener(this);
    appearanceBackgroundBox.addListener(this);
    appearanceSizeBox.addListener(this);
    behaviorAddVstFolderButton.addListener(this);
    behaviorRemoveVstFolderButton.addListener(this);

    programListModel = std::make_unique<ProgramListModel>();
    programListModel->rowCount = [this]() { return static_cast<int>(filteredProgramIndices.size()); };
    programListModel->paintRow = [this](int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= static_cast<int>(filteredProgramIndices.size()))
            return;
        const auto idx = filteredProgramIndices[static_cast<size_t>(row)];
        if (idx < 0 || idx >= static_cast<int>(runningApps.size()))
            return;
        const auto& a = runningApps[static_cast<size_t>(idx)];
        auto r = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);
        g.setColour(selected ? kUiPanel.withAlpha(0.95f) : kUiPanelSoft.withAlpha(0.8f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        if (selected)
        {
            g.setColour(kUiAccentSoft);
            g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.4f);
        }
        if (a.icon.isValid())
            g.drawImageWithin(a.icon, r.getX() + 6, r.getY() + 4, 16, 16, juce::RectanglePlacement::centred);
        g.setColour(kUiText);
        juce::Font rowFont(juce::FontOptions(13.4f));
        rowFont.setExtraKerningFactor(0.014f);
        g.setFont(rowFont);
        g.drawText(a.name, r.withTrimmedLeft(28), juce::Justification::centredLeft, true);
    };
    programListModel->rowClicked = [this](int row)
    {
        selectProgramByRow(row);
    };
    appListBox.setModel(programListModel.get());
    appListBox.setRowHeight(24);
    appListBox.setColour(juce::ListBox::backgroundColourId, kUiPanel.withAlpha(0.7f));
    appListBox.setOutlineThickness(1);
    appListBox.setColour(juce::ListBox::outlineColourId, kUiAccent.withAlpha(0.25f));
    appListBox.setMultipleSelectionEnabled(false);

    enabledProgramListModel = std::make_unique<ProgramListModel>();
    enabledProgramListModel->rowCount = [this]() { return cachedSettings.autoEnableProcessNames.size(); };
    enabledProgramListModel->paintRow = [this](int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= cachedSettings.autoEnableProcessNames.size())
            return;
        const auto name = cachedSettings.autoEnableProcessNames[row];
        auto r = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);
        g.setColour(selected ? kUiPanel.withAlpha(0.95f) : kUiPanelSoft.withAlpha(0.8f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        if (selected)
        {
            g.setColour(kUiAccentSoft);
            g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.2f);
        }
        g.setColour(kUiText);
        juce::Font rowFont(juce::FontOptions(13.0f));
        rowFont.setExtraKerningFactor(0.012f);
        g.setFont(rowFont);
        g.drawText(name, r.withTrimmedLeft(10), juce::Justification::centredLeft, true);
    };
    enabledProgramListModel->rowClicked = [this](int row)
    {
        selectedEnabledProgramRow = row;
        enabledProgramsListBox.selectRow(row);
    };
    enabledProgramsListBox.setModel(enabledProgramListModel.get());
    enabledProgramsListBox.setRowHeight(24);
    enabledProgramsListBox.setColour(juce::ListBox::backgroundColourId, kUiPanel.withAlpha(0.7f));
    enabledProgramsListBox.setOutlineThickness(1);
    enabledProgramsListBox.setColour(juce::ListBox::outlineColourId, kUiAccent.withAlpha(0.25f));

    vstSearchPathListModel = std::make_unique<ProgramListModel>();
    vstSearchPathListModel->rowCount = [this]() { return cachedSettings.vstSearchPaths.size(); };
    vstSearchPathListModel->paintRow = [this](int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= cachedSettings.vstSearchPaths.size())
            return;
        auto r = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);
        g.setColour(selected ? kUiPanel.withAlpha(0.95f) : kUiPanelSoft.withAlpha(0.8f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        if (selected)
        {
            g.setColour(kUiAccentSoft);
            g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.2f);
        }
        g.setColour(kUiText);
        juce::Font rowFont(juce::FontOptions(12.8f));
        rowFont.setExtraKerningFactor(0.01f);
        g.setFont(rowFont);
        g.drawText(cachedSettings.vstSearchPaths[row], r.withTrimmedLeft(10), juce::Justification::centredLeft, true);
    };
    vstSearchPathListModel->rowClicked = [this](int row)
    {
        behaviorVstFoldersListBox.selectRow(row);
    };
    behaviorVstFoldersListBox.setModel(vstSearchPathListModel.get());
    behaviorVstFoldersListBox.setRowHeight(24);
    behaviorVstFoldersListBox.setColour(juce::ListBox::backgroundColourId, kUiPanel.withAlpha(0.7f));
    behaviorVstFoldersListBox.setOutlineThickness(1);
    behaviorVstFoldersListBox.setColour(juce::ListBox::outlineColourId, kUiAccent.withAlpha(0.25f));

    settingsPanel = std::make_unique<SettingsOverlay>([this]
    {
        setSettingsPanelVisible(false);
    },
    [this](int deltaPixels)
    {
        scrollSettingsContent(deltaPixels);
    });
    settingsPanel->addAndMakeVisible(settingsNavLabel);
    settingsPanel->addAndMakeVisible(settingsNavAutoEnableButton);
    settingsPanel->addAndMakeVisible(settingsNavAppearanceButton);
    settingsPanel->addAndMakeVisible(settingsNavUpdatesButton);
    settingsPanel->addAndMakeVisible(settingsNavStartupButton);
    settingsPanel->addAndMakeVisible(appEnableLabel);
    settingsPanel->addAndMakeVisible(appearanceLabel);
    settingsPanel->addAndMakeVisible(appearanceModeLabel);
    settingsPanel->addAndMakeVisible(lightModeToggle);
    settingsPanel->addAndMakeVisible(appearanceThemeLabel);
    settingsPanel->addAndMakeVisible(appearanceThemeBox);
    settingsPanel->addAndMakeVisible(appearanceBackgroundLabel);
    settingsPanel->addAndMakeVisible(appearanceBackgroundBox);
    settingsPanel->addAndMakeVisible(appearanceSizeLabel);
    settingsPanel->addAndMakeVisible(appearanceSizeBox);
    settingsPanel->addAndMakeVisible(appSearchLabel);
    settingsPanel->addAndMakeVisible(appListLabel);
    settingsPanel->addAndMakeVisible(enabledProgramsLabel);
    settingsPanel->addAndMakeVisible(appSearchEditor);
    settingsPanel->addAndMakeVisible(autoEnableToggle);
    settingsPanel->addAndMakeVisible(refreshAppsButton);
    settingsPanel->addAndMakeVisible(appListBox);
    settingsPanel->addAndMakeVisible(enabledProgramsListBox);
    settingsPanel->addAndMakeVisible(appPathEditor);
    settingsPanel->addAndMakeVisible(browseAppPathButton);
    settingsPanel->addAndMakeVisible(removeProgramButton);
    settingsPanel->addAndMakeVisible(updatesLabel);
    settingsPanel->addAndMakeVisible(autoDownloadUpdatesToggle);
    settingsPanel->addAndMakeVisible(checkUpdatesButton);
    settingsPanel->addAndMakeVisible(updatesLinksLabel);
    settingsPanel->addAndMakeVisible(updatesGithubButton);
    settingsPanel->addAndMakeVisible(updatesWebsiteButton);
    settingsPanel->addAndMakeVisible(updateStatusLabel);
    settingsPanel->addAndMakeVisible(startupLabel);
    settingsPanel->addAndMakeVisible(behaviorListenDeviceLabel);
    settingsPanel->addAndMakeVisible(behaviorListenDeviceBox);
    settingsPanel->addAndMakeVisible(behaviorVstFoldersLabel);
    settingsPanel->addAndMakeVisible(behaviorVstFoldersListBox);
    settingsPanel->addAndMakeVisible(behaviorAddVstFolderButton);
    settingsPanel->addAndMakeVisible(behaviorRemoveVstFolderButton);
    settingsPanel->addAndMakeVisible(startWithWindowsToggle);
    settingsPanel->addAndMakeVisible(startMinimizedToggle);
    settingsPanel->addAndMakeVisible(followAutoEnableWindowToggle);
    settingsPanel->addAndMakeVisible(startupHintLabel);
    settingsPanel->addAndMakeVisible(closeSettingsButton);

    addAndMakeVisible(*settingsPanel);
    settingsPanel->setInterceptsMouseClicks(true, true);
    settingsPanel->setVisible(false);
    settingsPanel->setAlpha(0.0f);
    settingsPanelTargetVisible = false;
    settingsPanelAlpha = 0.0f;
    activeSettingsTab = 0;

    outputGainLabel.setText("Virtual Mic Gain", juce::dontSendNotification);
    addAndMakeVisible(outputGainLabel);
    addAndMakeVisible(outputGain);
    addAndMakeVisible(outputGainResetButton);
    outputGainResetButton.addListener(this);
    outputGainResetButton.setComponentID("reset");
    outputGain.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 18);
    outputGain.setRange(-24.0, 24.0, 0.1);
    outputGain.setSkewFactorFromMidPoint(0.0);
    outputGain.setDoubleClickReturnValue(true, 0.0);
    outputGain.setColour(juce::Slider::trackColourId, kUiAccent.withAlpha(0.72f));
    outputGain.setColour(juce::Slider::thumbColourId, kUiAccentSoft);
    outputGain.setColour(juce::Slider::textBoxTextColourId, kUiText);
    outputGain.setColour(juce::Slider::textBoxBackgroundColourId, kUiPanelSoft.withAlpha(0.92f));
    outputGain.setColour(juce::Slider::textBoxOutlineColourId, kUiAccent.withAlpha(0.30f));
    outputGain.onValueChange = [this]
    {
        params.outputGainDb.store(static_cast<float>(outputGain.getValue()));
    };

    addAndMakeVisible(meterIn);
    addAndMakeVisible(meterOut);
    addAndMakeVisible(diagnostics);

    populateBufferBox(bufferBox, kDefaultBlockSize);

    vstAvailableBox.setTextWhenNothingSelected("Detected VST3 plugins");
    const auto fxEnabled = ! params.bypass.load();
    effectsToggle.setToggleState(fxEnabled, juce::dontSendNotification);
    effectsToggle.setButtonText(fxEnabled ? "Effects On" : "Effects Off");
    outputGain.setValue(params.outputGainDb.load(), juce::dontSendNotification);

    cachedSettings = settingsStore.loadEngineSettings();
    cachedSettings.themeVariant = juce::jlimit(0, 1, cachedSettings.themeVariant);
    cachedSettings.uiDensity = juce::jlimit(0, 2, cachedSettings.uiDensity);
    const auto persistedLastPreset = loadPersistedLastPresetName();
    if (persistedLastPreset.isNotEmpty())
        cachedSettings.lastPresetName = persistedLastPreset;
    engine.getVstHost().importScannedPaths(cachedSettings.scannedVstPaths);
    if (cachedSettings.vstSearchPaths.isEmpty())
    {
        for (const auto& folder : getDefaultVst3Folders())
            cachedSettings.vstSearchPaths.addIfNotAlreadyThere(folder.getFullPathName());
    }
    autoEnableToggle.setToggleState(cachedSettings.autoEnableByApp, juce::dontSendNotification);
    autoDownloadUpdatesToggle.setToggleState(cachedSettings.autoInstallUpdates, juce::dontSendNotification);
    lightModeToggle.setToggleState(cachedSettings.lightMode, juce::dontSendNotification);
    appearanceThemeBox.setSelectedId(cachedSettings.themeVariant + 1, juce::dontSendNotification);
    appearanceBackgroundBox.setSelectedId(cachedSettings.transparentBackground ? 1 : 2, juce::dontSendNotification);
    appearanceSizeBox.setSelectedId(cachedSettings.uiDensity + 1, juce::dontSendNotification);
    startWithWindowsToggle.setToggleState(cachedSettings.startWithWindows, juce::dontSendNotification);
    startMinimizedToggle.setToggleState(cachedSettings.startMinimizedToTray, juce::dontSendNotification);
    followAutoEnableWindowToggle.setToggleState(cachedSettings.followAutoEnableWindowState, juce::dontSendNotification);
    applyThemePalette();
    applyUiDensity();
    refreshAppearanceControls();
    setUpdateStatus(cachedSettings.autoInstallUpdates ? "Auto-install is enabled." : "Auto-install is off.");
    setActiveSettingsTab(0);
    applyWindowsStartupSetting();
    refreshRunningApps();
    refreshEnabledProgramsList();
    if (cachedSettings.autoEnableProcessPath.isNotEmpty())
        appPathEditor.setText(cachedSettings.autoEnableProcessPath, juce::dontSendNotification);

    loadDeviceLists();
    refreshListenOutputDevices();
    refreshVstSearchPathList();
    refreshKnownPlugins();
    refreshPresets();
    if (cachedSettings.lastPresetName.isNotEmpty() && cachedSettings.lastPresetName != "Default")
        loadPresetByName(cachedSettings.lastPresetName);
    markCurrentPresetSnapshot();

    if (! cachedSettings.hasSeenFirstLaunchGuide)
    {
        cachedSettings.hasSeenFirstLaunchGuide = true;
        saveCachedSettings();

        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis == nullptr)
                return;
            safeThis->showFirstLaunchGuide();
        });
    }

    if (cachedSettings.autoInstallUpdates)
    {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::Timer::callAfterDelay(1200, [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->triggerUpdateCheck(false);
        });
    }

    uiTimerHz = 30;
    startTimerHz(uiTimerHz);
}

void MainComponent::onWindowVisible()
{
    applyThemePalette();

    if (scannedOnVisible)
        return;

    scannedOnVisible = true;
    if (! cachedSettings.hasCompletedInitialVstScan || cachedSettings.scannedVstPaths.isEmpty())
    {
        autoScanVstFolders();
        cachedSettings.hasCompletedInitialVstScan = true;
        cachedSettings.scannedVstPaths = engine.getVstHost().getScannedPaths();
        saveCachedSettings();
    }
}

bool MainComponent::areEffectsEnabled() const
{
    return ! params.bypass.load();
}

bool MainComponent::isMuted() const
{
    return params.mute.load();
}

bool MainComponent::isLightModeEnabled() const
{
    return cachedSettings.lightMode;
}

int MainComponent::getThemeVariant() const
{
    return juce::jlimit(0, 1, cachedSettings.themeVariant);
}

int MainComponent::getUiDensity() const
{
    return juce::jlimit(0, 2, cachedSettings.uiDensity);
}

void MainComponent::trayToggleEffectsBypass()
{
    const auto enableEffects = params.bypass.load();
    applyEffectsEnabledState(enableEffects, true);
    setEffectsHint(enableEffects ? "Effects enabled from tray" : "Effects bypassed from tray", 50);
}

void MainComponent::trayToggleMute()
{
    const auto next = ! params.mute.load();
    params.mute.store(next);
    setEffectsHint(next ? "Muted from tray" : "Unmuted from tray", next ? -1 : 45);
    repaint(effectsToggle.getBounds().expanded(180, 20));
}

void MainComponent::trayRestartAudio()
{
    runAudioRestartWithOverlay(true);
}

void MainComponent::trayLoadPreset(const juce::String& name)
{
    if (name.trim().isEmpty())
        return;

    loadPresetByName(name);
    refreshPresets();
    presetBox.setText(currentPresetName, juce::dontSendNotification);
}

juce::Array<juce::File> MainComponent::getDefaultVst3Folders() const
{
    juce::Array<juce::File> out;
    out.add(juce::File(R"(C:\Program Files\Common Files\VST3)"));
    out.add(juce::File(R"(C:\Program Files (x86)\Common Files\VST3)"));
    out.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("AppData")
                .getChildFile("Roaming")
                .getChildFile("VST3"));
    return out;
}

void MainComponent::autoScanVstFolders()
{
    int total = 0;
    juce::Array<juce::File> folders;
    if (cachedSettings.vstSearchPaths.isEmpty())
    {
        folders = getDefaultVst3Folders();
    }
    else
    {
        for (const auto& p : cachedSettings.vstSearchPaths)
            folders.add(juce::File(p));
    }

    for (const auto& folder : folders)
    {
        if (! folder.isDirectory())
            continue;
        total += engine.getVstHost().scanFolder(folder).size();
    }
    refreshKnownPlugins();
    cachedSettings.scannedVstPaths = engine.getVstHost().getScannedPaths();
    saveCachedSettings();
    Logger::instance().log("Visible VST scan complete. Found " + juce::String(total));
}

void MainComponent::refreshRunningApps()
{
    runningApps.clear();
#if JUCE_WINDOWS
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe {};
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe))
        {
            do
            {
                juce::String exe(pe.szExeFile);
                if (exe.isNotEmpty())
                {
                    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                    juce::String path;
                    if (h != nullptr)
                    {
                        WCHAR buf[MAX_PATH];
                        DWORD len = MAX_PATH;
                        if (QueryFullProcessImageNameW(h, 0, buf, &len))
                            path = juce::String(buf);
                        CloseHandle(h);
                    }

                    if (path.isNotEmpty())
                    {
                        bool exists = false;
                        for (const auto& a : runningApps)
                        {
                            if (a.path.equalsIgnoreCase(path))
                            {
                                exists = true;
                                break;
                            }
                        }
                        if (! exists)
                        {
                            RunningAppEntry entry;
                            entry.name = exe;
                            entry.path = path;
                            entry.icon = extractAppIcon(path);
                            runningApps.push_back(std::move(entry));
                        }
                    }
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
#endif
    std::sort(runningApps.begin(), runningApps.end(), [](const RunningAppEntry& a, const RunningAppEntry& b)
    {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    refreshProgramsList();
}

std::vector<int> MainComponent::filterRunningAppIndices(const juce::String& query) const
{
    std::vector<int> out;
    const auto q = query.trim().toLowerCase();
    for (int i = 0; i < static_cast<int>(runningApps.size()); ++i)
    {
        const auto& a = runningApps[static_cast<size_t>(i)];
        if (q.isNotEmpty() && ! a.name.toLowerCase().contains(q) && ! a.path.toLowerCase().contains(q))
            continue;
        out.push_back(i);
    }
    return out;
}

void MainComponent::refreshProgramsList()
{
    filteredProgramIndices.clear();
    for (int i = 0; i < static_cast<int>(runningApps.size()); ++i)
        filteredProgramIndices.push_back(i);
    appListBox.updateContent();

    if (filteredProgramIndices.empty())
    {
        selectedProgramRow = -1;
        appEnableLabel.setText("Program Auto-Enable", juce::dontSendNotification);
        return;
    }

    int rowToSelect = 0;
    const auto q = appSearchEditor.getText().trim().toLowerCase();
    if (q.isNotEmpty())
    {
        for (int r = 0; r < static_cast<int>(filteredProgramIndices.size()); ++r)
        {
            const auto idx = filteredProgramIndices[static_cast<size_t>(r)];
            const auto& a = runningApps[static_cast<size_t>(idx)];
            if (a.name.toLowerCase().contains(q) || a.path.toLowerCase().contains(q))
            {
                rowToSelect = r;
                break;
            }
        }
    }
    else if (cachedSettings.autoEnableProcessName.isNotEmpty())
    {
        for (int r = 0; r < static_cast<int>(filteredProgramIndices.size()); ++r)
        {
            const auto idx = filteredProgramIndices[static_cast<size_t>(r)];
            if (idx >= 0 && idx < static_cast<int>(runningApps.size())
                && runningApps[static_cast<size_t>(idx)].name.equalsIgnoreCase(cachedSettings.autoEnableProcessName))
            {
                rowToSelect = r;
                break;
            }
        }
    }

    selectedProgramRow = rowToSelect;
    appListBox.selectRow(rowToSelect);
    appListBox.scrollToEnsureRowIsOnscreen(rowToSelect);
}

void MainComponent::refreshEnabledProgramsList()
{
    enabledProgramsListBox.updateContent();
    if (cachedSettings.autoEnableProcessNames.isEmpty())
    {
        selectedEnabledProgramRow = -1;
        appEnableLabel.setText("Program Auto-Enable", juce::dontSendNotification);
        return;
    }

    if (selectedEnabledProgramRow < 0 || selectedEnabledProgramRow >= cachedSettings.autoEnableProcessNames.size())
        selectedEnabledProgramRow = 0;

    enabledProgramsListBox.selectRow(selectedEnabledProgramRow);
    enabledProgramsListBox.scrollToEnsureRowIsOnscreen(selectedEnabledProgramRow);
    appEnableLabel.setText("Program Auto-Enable (" + juce::String(cachedSettings.autoEnableProcessNames.size()) + " selected)", juce::dontSendNotification);
}

void MainComponent::refreshListenOutputDevices()
{
    const juce::ScopedValueSetter<bool> svs(suppressControlCallbacks, true);
    behaviorListenDeviceBox.clear();
    auto* currentType = engine.getDeviceManager().getCurrentDeviceTypeObject();
    if (currentType == nullptr)
        return;

    const auto outputNames = currentType->getDeviceNames(false);
    int id = 1;
    for (const auto& name : outputNames)
    {
        behaviorListenDeviceBox.addItem(name, id++);
    }

    juce::String selected = cachedSettings.listenMonitorDeviceName;
    if (selected.isEmpty())
        selected = findOutputByMatch({ "speaker", "headphones", "airpods", "bluetooth", "buds", "earbud", "headset", "realtek", "output" });

    if (selected.isNotEmpty())
    {
        const auto idx = outputNames.indexOf(selected);
        if (idx >= 0)
            behaviorListenDeviceBox.setSelectedId(idx + 1, juce::dontSendNotification);
    }
}

void MainComponent::refreshVstSearchPathList()
{
    if (vstSearchPathListModel != nullptr)
    {
        behaviorVstFoldersListBox.updateContent();
        behaviorVstFoldersListBox.repaint();
    }
}

void MainComponent::addVstSearchPath(const juce::String& path)
{
    const auto clean = path.trim();
    if (clean.isEmpty())
        return;
    cachedSettings.vstSearchPaths.addIfNotAlreadyThere(clean);
    saveCachedSettings();
    refreshVstSearchPathList();
}

void MainComponent::removeSelectedVstSearchPath()
{
    const auto selected = behaviorVstFoldersListBox.getSelectedRow();
    if (! juce::isPositiveAndBelow(selected, cachedSettings.vstSearchPaths.size()))
        return;
    cachedSettings.vstSearchPaths.remove(selected);
    saveCachedSettings();
    refreshVstSearchPathList();
}

void MainComponent::selectProgramByRow(int row)
{
    if (row < 0 || row >= static_cast<int>(filteredProgramIndices.size()))
        return;
    const auto idx = filteredProgramIndices[static_cast<size_t>(row)];
    if (idx < 0 || idx >= static_cast<int>(runningApps.size()))
        return;

    const auto& a = runningApps[static_cast<size_t>(idx)];
    selectedProgramRow = row;
    cachedSettings.autoEnableProcessName = a.name; // legacy compatibility
    if (! cachedSettings.autoEnableProcessNames.contains(a.name))
        cachedSettings.autoEnableProcessNames.add(a.name);
    saveCachedSettings();
    refreshEnabledProgramsList();
}

bool MainComponent::addAutoEnableProgramFromPath(const juce::String& pathText)
{
    auto normalized = pathText.trim();
    normalized = normalized.unquoted();
    if (normalized.isEmpty())
        return false;

    juce::File exeFile(normalized);
    auto fullPath = normalized;
    juce::String processName;

    if (exeFile.existsAsFile())
    {
        fullPath = exeFile.getFullPathName();
        processName = exeFile.getFileName();
    }
    else
    {
        processName = juce::File(normalized).getFileName();
        if (processName.isEmpty())
            processName = normalized;
        if (! processName.endsWithIgnoreCase(".exe"))
            processName << ".exe";
    }

    bool alreadyAdded = false;
    for (const auto& n : cachedSettings.autoEnableProcessNames)
    {
        if (n.equalsIgnoreCase(processName))
        {
            alreadyAdded = true;
            break;
        }
    }

    if (! alreadyAdded)
        cachedSettings.autoEnableProcessNames.add(processName);

    cachedSettings.autoEnableProcessName = processName;
    cachedSettings.autoEnableProcessPath = fullPath;
    appPathEditor.setText(fullPath, juce::dontSendNotification);
    saveCachedSettings();
    refreshEnabledProgramsList();
    return true;
}

void MainComponent::removeSelectedProgram()
{
    if (selectedEnabledProgramRow < 0 || selectedEnabledProgramRow >= cachedSettings.autoEnableProcessNames.size())
        return;

    cachedSettings.autoEnableProcessNames.remove(selectedEnabledProgramRow);
    if (! cachedSettings.autoEnableProcessNames.isEmpty())
        cachedSettings.autoEnableProcessName = cachedSettings.autoEnableProcessNames[0];
    else
        cachedSettings.autoEnableProcessName = {};

    saveCachedSettings();
    refreshEnabledProgramsList();
}

juce::Image MainComponent::extractAppIcon(const juce::String& exePath) const
{
#if JUCE_WINDOWS
    HICON smallIcon = nullptr;
    if (ExtractIconExW(exePath.toWideCharPointer(), 0, nullptr, &smallIcon, 1) > 0 && smallIcon != nullptr)
    {
        juce::Image image = juce::Image(juce::Image::ARGB, 16, 16, true);
        juce::Image::BitmapData bd(image, juce::Image::BitmapData::writeOnly);

        BITMAPV5HEADER bi {};
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = 16;
        bi.bV5Height = -16;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;

        void* bits = nullptr;
        auto hdc = GetDC(nullptr);
        auto hbm = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
        auto mem = CreateCompatibleDC(hdc);
        auto old = SelectObject(mem, hbm);
        DrawIconEx(mem, 0, 0, smallIcon, 16, 16, 0, nullptr, DI_NORMAL);

        auto* src = static_cast<const uint8_t*>(bits);
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                const auto i = (y * 16 + x) * 4;
                const juce::Colour c(src[i + 2], src[i + 1], src[i + 0], src[i + 3]);
                bd.setPixelColour(x, y, c);
            }
        }

        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(hbm);
        ReleaseDC(nullptr, hdc);
        DestroyIcon(smallIcon);
        return image;
    }
#endif
    return {};
}

juce::String MainComponent::getForegroundProcessName() const
{
#if JUCE_WINDOWS
    const auto path = getForegroundProcessPath();
    if (path.isEmpty())
        return {};
    return juce::File(path).getFileName();
#else
    return {};
#endif
}

juce::String MainComponent::getForegroundProcessPath() const
{
#if JUCE_WINDOWS
    auto hwnd = GetForegroundWindow();
    if (hwnd == nullptr)
        return {};

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return {};

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h == nullptr)
        return {};

    WCHAR buf[MAX_PATH];
    DWORD len = MAX_PATH;
    juce::String result;
    if (QueryFullProcessImageNameW(h, 0, buf, &len))
        result = juce::String(buf);
    CloseHandle(h);
    return result;
#else
    return {};
#endif
}

void MainComponent::saveCachedSettings()
{
    settingsStore.saveEngineSettings(cachedSettings);
}

void MainComponent::setSettingsPanelVisible(bool visible)
{
    if (settingsPanel == nullptr)
        return;

    settingsPanelTargetVisible = visible;
    if (visible)
    {
        settingsScrollY = 0;
        settingsScrollMax = 0;
        refreshRunningApps();
        refreshProgramsList();
        refreshListenOutputDevices();
        refreshVstSearchPathList();
        updateSettingsTabVisibility();
        settingsPanel->setVisible(true);
        settingsPanel->toFront(true);
    }
    settingsPanel->setInterceptsMouseClicks(visible, visible);
}

void MainComponent::updateSettingsTabVisibility()
{
    const bool autoEnableTab = (activeSettingsTab == 0);
    const bool appearanceTab = (activeSettingsTab == 1);
    const bool updatesTab = (activeSettingsTab == 2);
    const bool startupTab = (activeSettingsTab == 3);
    auto setVisible = [](juce::Component& component, bool visible)
    {
        component.setVisible(visible);
        component.setEnabled(visible);
    };

    for (auto* c : { static_cast<juce::Component*>(&appEnableLabel),
                     static_cast<juce::Component*>(&appSearchLabel),
                     static_cast<juce::Component*>(&appListLabel),
                     static_cast<juce::Component*>(&enabledProgramsLabel),
                     static_cast<juce::Component*>(&appSearchEditor),
                     static_cast<juce::Component*>(&autoEnableToggle),
                     static_cast<juce::Component*>(&refreshAppsButton),
                     static_cast<juce::Component*>(&appListBox),
                     static_cast<juce::Component*>(&enabledProgramsListBox),
                     static_cast<juce::Component*>(&appPathEditor),
                     static_cast<juce::Component*>(&browseAppPathButton),
                     static_cast<juce::Component*>(&removeProgramButton) })
    {
        if (c != nullptr)
            setVisible(*c, autoEnableTab);
    }

    for (auto* c : { static_cast<juce::Component*>(&appearanceLabel),
                     static_cast<juce::Component*>(&appearanceModeLabel),
                     static_cast<juce::Component*>(&lightModeToggle),
                     static_cast<juce::Component*>(&appearanceThemeLabel),
                     static_cast<juce::Component*>(&appearanceThemeBox),
                     static_cast<juce::Component*>(&appearanceBackgroundLabel),
                     static_cast<juce::Component*>(&appearanceBackgroundBox),
                     static_cast<juce::Component*>(&appearanceSizeLabel),
                     static_cast<juce::Component*>(&appearanceSizeBox) })
    {
        if (c != nullptr)
            setVisible(*c, appearanceTab);
    }

    for (auto* c : { static_cast<juce::Component*>(&updatesLabel),
                     static_cast<juce::Component*>(&autoDownloadUpdatesToggle),
                     static_cast<juce::Component*>(&checkUpdatesButton),
                     static_cast<juce::Component*>(&updateStatusLabel),
                     static_cast<juce::Component*>(&updatesLinksLabel),
                     static_cast<juce::Component*>(&updatesGithubButton),
                     static_cast<juce::Component*>(&updatesWebsiteButton) })
    {
        if (c != nullptr)
            setVisible(*c, updatesTab);
    }

    for (auto* c : { static_cast<juce::Component*>(&startupLabel),
                     static_cast<juce::Component*>(&behaviorListenDeviceLabel),
                     static_cast<juce::Component*>(&behaviorListenDeviceBox),
                     static_cast<juce::Component*>(&behaviorVstFoldersLabel),
                     static_cast<juce::Component*>(&behaviorVstFoldersListBox),
                     static_cast<juce::Component*>(&behaviorAddVstFolderButton),
                     static_cast<juce::Component*>(&behaviorRemoveVstFolderButton),
                     static_cast<juce::Component*>(&startWithWindowsToggle),
                     static_cast<juce::Component*>(&startMinimizedToggle),
                     static_cast<juce::Component*>(&followAutoEnableWindowToggle),
                     static_cast<juce::Component*>(&startupHintLabel) })
    {
        if (c != nullptr)
            setVisible(*c, startupTab);
    }
}

void MainComponent::setActiveSettingsTab(int tab)
{
    activeSettingsTab = juce::jlimit(0, 3, tab);
    settingsScrollY = 0;
    settingsNavAutoEnableButton.setToggleState(activeSettingsTab == 0, juce::dontSendNotification);
    settingsNavAppearanceButton.setToggleState(activeSettingsTab == 1, juce::dontSendNotification);
    settingsNavUpdatesButton.setToggleState(activeSettingsTab == 2, juce::dontSendNotification);
    settingsNavStartupButton.setToggleState(activeSettingsTab == 3, juce::dontSendNotification);
    updateSettingsTabVisibility();
    resized();
}

void MainComponent::scrollSettingsContent(int deltaPixels)
{
    if (! settingsPanelTargetVisible || settingsScrollMax <= 0)
        return;

    const auto newY = juce::jlimit(0, settingsScrollMax, settingsScrollY + deltaPixels);
    if (newY == settingsScrollY)
        return;

    settingsScrollY = newY;
    resized();
    repaint();
}

void MainComponent::triggerButtonFlash(juce::Button* button)
{
    if (button == &savePresetButton)
        savePresetFlashAlpha = 1.0f;
    else if (button == &aboutButton)
        aboutFlashAlpha = 1.0f;
}

void MainComponent::showFirstLaunchGuide()
{
    triggerLogoFizzAnimation();

    const juce::String guide =
        "Welcome to Fizzle!\n\n"
        "Quick setup:\n"
        "1) Select your microphone in Input Mic.\n"
        "2) Select Fizzle Mic (VB-Cable) in Fizzle Mic Output.\n"
        "3) Turn Effects On.\n"
        "4) Pick a VST3 from the detected list to add it.\n"
        "5) Use Listen if you want local monitoring.\n\n"
        "Tip: Save a preset after your first good setup.";

    auto* prompt = new juce::AlertWindow("First Launch Guide",
                                         guide,
                                         juce::AlertWindow::InfoIcon);
    prompt->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    prompt->setEscapeKeyCancels(false);
    prompt->enterModalState(true, nullptr, true);
}

void MainComponent::triggerLogoFizzAnimation()
{
    logoFizzBubbles.clear();
    logoFizzActive = true;
    logoFizzTicks = 180;

    juce::Random random(static_cast<juce::int64>(juce::Time::getMillisecondCounterHiRes()));
    const auto bounds = getLocalBounds().toFloat().reduced(16.0f, 12.0f);
    const auto startY = bounds.getBottom() - 16.0f;
    const int bubbleCount = 34;
    for (int i = 0; i < bubbleCount; ++i)
    {
        FizzBubble b;
        b.pos = { bounds.getX() + random.nextFloat() * bounds.getWidth(),
                  startY + random.nextFloat() * 90.0f };
        b.radius = (3.0f + random.nextFloat() * 10.0f) * kUiControlScale;
        b.speed = (0.8f + random.nextFloat() * 2.1f) * kUiControlScale;
        b.drift = (random.nextFloat() - 0.5f) * 0.9f;
        b.alpha = 0.30f + random.nextFloat() * 0.42f;
        logoFizzBubbles.push_back(b);
    }
}

void MainComponent::applyWindowsStartupSetting()
{
#if JUCE_WINDOWS
    if (! setRunAtStartupEnabled(cachedSettings.startWithWindows, cachedSettings.startMinimizedToTray))
        Logger::instance().log("Failed to update Windows startup registration.");
#endif
}

void MainComponent::toggleWindowMaximize()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        if (! windowMaximized)
        {
            windowRestoreBounds = window->getBounds();
            window->setBounds(window->getParentMonitorArea());
            windowMaximized = true;
        }
        else
        {
            if (! windowRestoreBounds.isEmpty())
                window->setBounds(windowRestoreBounds);
            windowMaximized = false;
        }
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    if (auto* modalMgr = juce::ModalComponentManager::getInstance())
    {
        if (modalMgr->getNumModalComponents() > 0)
        {
            if (auto* modal = modalMgr->getModalComponent(0))
            {
                if (modal != this)
                    return;
            }
        }
    }

    const auto localEvent = e.getEventRelativeTo(this);
    const auto localPos = localEvent.getPosition();
    draggingWindow = false;
    draggingResizeGrip = false;

    if (localEvent.mods.isLeftButtonDown() && resizeGripBounds.contains(localPos))
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        {
            draggingResizeGrip = true;
            resizeDragStartScreen = localEvent.getScreenPosition();
            resizeStartBounds = window->getBounds();
            return;
        }
    }

    auto isInteractiveClick = [this](juce::Component* c)
    {
        for (auto* p = c; p != nullptr; p = p->getParentComponent())
        {
            if (p == this)
                break;
            if (p == settingsPanel.get())
                return true;
            if (dynamic_cast<juce::Button*>(p) != nullptr
                || dynamic_cast<juce::ComboBox*>(p) != nullptr
                || dynamic_cast<juce::Slider*>(p) != nullptr
                || dynamic_cast<juce::TextEditor*>(p) != nullptr
                || dynamic_cast<juce::ListBox*>(p) != nullptr)
            {
                return true;
            }
        }
        return false;
    };

    const bool inHeader = headerBounds.contains(localPos);
    const bool allowAnimationDrag = logoFizzActive && ! isInteractiveClick(localEvent.originalComponent);
    const bool inDragZone = inHeader || allowAnimationDrag;
    if (! inDragZone)
        return;

    if (localEvent.originalComponent == &windowMinButton
        || localEvent.originalComponent == &windowMaxButton
        || localEvent.originalComponent == &windowCloseButton)
    return;

    if (headerLogoBounds.contains(localPos))
        triggerLogoFizzAnimation();

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
#if JUCE_WINDOWS
        if (auto* peer = window->getPeer())
        {
            if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
            {
                draggingWindow = true;
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                draggingWindow = false;
                return;
            }
        }
#endif
        windowDragger.startDraggingComponent(window, localEvent);
        draggingWindow = true;
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    const auto localEvent = e.getEventRelativeTo(this);
    if (draggingResizeGrip)
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        {
            auto delta = localEvent.getScreenPosition() - resizeDragStartScreen;
            auto newBounds = resizeStartBounds;
            newBounds.setWidth(juce::jmax(420, resizeStartBounds.getWidth() + delta.x));
            newBounds.setHeight(juce::jmax(320, resizeStartBounds.getHeight() + delta.y));
            window->setBoundsConstrained(newBounds);
        }
        return;
    }

    if (! draggingWindow)
        return;

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        windowDragger.dragComponent(window, localEvent, nullptr);
}

void MainComponent::mouseUp(const juce::MouseEvent&)
{
    draggingWindow = false;
    draggingResizeGrip = false;
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (headerBounds.contains(e.getEventRelativeTo(this).getPosition()))
        toggleWindowMaximize();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        restoreLastRemovedPlugin();
        return true;
    }
    return false;
}

void MainComponent::refreshKnownPlugins()
{
    suppressPluginAddFromSelection = true;
    vstAvailableBox.clear();
    int id = 1;
    for (const auto& desc : engine.getVstHost().getKnownPluginDescriptions())
        vstAvailableBox.addItem(desc.name, id++);
    // Do not auto-select after scan; user selecting an item is what should add it.
    vstAvailableBox.setSelectedId(0, juce::dontSendNotification);
    suppressPluginAddFromSelection = false;
}

void MainComponent::refreshPluginChainUi()
{
    vstChainList.updateContent();
    vstChainList.repaint();
}

void MainComponent::loadDeviceLists()
{
    const juce::ScopedValueSetter<bool> svs(suppressControlCallbacks, true);
    inputBox.clear();
    outputBox.clear();
    outputDeviceRealNames.clear();

    auto* currentType = engine.getDeviceManager().getCurrentDeviceTypeObject();
    if (currentType == nullptr)
        return;

    const auto inputNames = currentType->getDeviceNames(true);
    const auto outputNames = currentType->getDeviceNames(false);

    int id = 1;
    for (const auto& name : inputNames)
        inputBox.addItem(name, id++);

    id = 1;
    for (const auto& name : outputNames)
    {
        outputDeviceRealNames.add(name);
        outputBox.addItem(getDisplayOutputName(name), id++);
    }

    auto current = engine.currentSettings();
    cachedSettings.inputDeviceName = current.inputDeviceName;
    cachedSettings.outputDeviceName = current.outputDeviceName;
    cachedSettings.bufferSize = current.bufferSize;
    syncBufferBoxSelection(bufferBox, current.bufferSize);
    if (current.inputDeviceName.isNotEmpty())
        inputBox.setSelectedId(inputNames.indexOf(current.inputDeviceName) + 1, juce::dontSendNotification);
    if (current.outputDeviceName.isNotEmpty())
    {
        const auto outputIndex = outputDeviceRealNames.indexOf(current.outputDeviceName);
        if (outputIndex >= 0)
            outputBox.setSelectedId(outputIndex + 1, juce::dontSendNotification);
    }
    refreshListenOutputDevices();
}

void MainComponent::refreshPresets()
{
    presetBox.clear();
    int id = 1;
    for (const auto& p : presetStore.listPresets())
        presetBox.addItem(p, id++);
    if (presetBox.getNumItems() == 0)
        presetBox.addItem("Default", 1);
    int matchId = 0;
    for (int i = 0; i < presetBox.getNumItems(); ++i)
    {
        if (presetBox.getItemText(i) == currentPresetName)
        {
            matchId = presetBox.getItemId(i);
            break;
        }
    }
    presetBox.setSelectedId(matchId > 0 ? matchId : 1, juce::dontSendNotification);
}

void MainComponent::applySettingsFromControls()
{
    if (applyingAudioSettings)
        return;

    const juce::ScopedValueSetter<bool> applyingGuard(applyingAudioSettings, true);
    auto s = engine.currentSettings();
    const auto previous = s;
    s.inputDeviceName = inputBox.getText();
    s.outputDeviceName = getSelectedOutputDeviceName();
    const auto requestedBuffer = bufferBox.getText().getIntValue();
    s.bufferSize = requestedBuffer > 0 ? requestedBuffer : kDefaultBlockSize;

    if (s.inputDeviceName.isEmpty() || s.outputDeviceName.isEmpty())
        return;

    if (s.inputDeviceName == previous.inputDeviceName
        && s.outputDeviceName == previous.outputDeviceName
        && s.bufferSize == previous.bufferSize)
        return;

    juce::String error;
    closePluginEditorWindow();
    if (! engine.start(s, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Audio Error", error);
        juce::String rollbackError;
        if (! engine.start(previous, rollbackError))
            Logger::instance().log("Audio rollback failed: " + rollbackError);
    }

    const auto applied = engine.currentSettings();
    cachedSettings.inputDeviceName = applied.inputDeviceName;
    cachedSettings.outputDeviceName = applied.outputDeviceName;
    cachedSettings.bufferSize = applied.bufferSize;
    saveCachedSettings();
    loadDeviceLists();
}

juce::String MainComponent::findOutputByMatch(const juce::StringArray& candidates) const
{
    int bestIndex = -1;
    int bestScore = -1;
    for (int i = 0; i < outputDeviceRealNames.size(); ++i)
    {
        const auto itemText = outputDeviceRealNames[i].toLowerCase();
        int score = 0;
        for (const auto& c : candidates)
            if (itemText.contains(c.toLowerCase())) score += 2;
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = i;
        }
    }
    return (bestIndex >= 0 && bestScore > 0) ? outputDeviceRealNames[bestIndex] : juce::String{};
}

juce::String MainComponent::getSelectedOutputDeviceName() const
{
    const auto id = outputBox.getSelectedId();
    const auto index = id - 1;
    if (juce::isPositiveAndBelow(index, outputDeviceRealNames.size()))
        return outputDeviceRealNames[index];
    return outputBox.getText();
}

bool MainComponent::isVirtualMicName(const juce::String& outputName) const
{
    return outputName.containsIgnoreCase("vb")
        || outputName.containsIgnoreCase("cable")
        || outputName.containsIgnoreCase("virtual")
        || outputName.containsIgnoreCase("fizzle mic");
}

juce::String MainComponent::getDisplayOutputName(const juce::String& realOutputName) const
{
    if (isVirtualMicName(realOutputName))
        return "Fizzle Mic";
    return realOutputName;
}

void MainComponent::rowOpenEditor(int row)
{
    if (! juce::isPositiveAndBelow(row, getNumRows()))
        return;

    closePluginEditorWindow();

    if (auto* hosted = engine.getVstHost().getPlugin(row))
    {
        auto* instance = hosted->instance.get();
        if (instance == nullptr)
            return;

        if (! instance->hasEditor())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                   "No Plugin Editor",
                                                   "This plugin does not provide a custom editor.");
            return;
        }

        juce::AudioProcessorEditor* editor = nullptr;
        try
        {
            editor = instance->createEditorIfNeeded();
        }
        catch (...)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Plugin Editor Failed",
                                                   "The plugin editor failed to open.");
            return;
        }

        if (editor == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                   "Plugin Editor Failed",
                                                   "The plugin editor could not be created.");
            return;
        }

        juce::Component::SafePointer<MainComponent> safeThis(this);
        pluginEditorWindow = std::make_unique<PluginEditorWindow>(editor, [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->closePluginEditorWindow();
        });
        pluginEditorWindow->setVisible(true);
        pluginEditorWindow->toFront(true);
    }
}

void MainComponent::closePluginEditorWindow()
{
    if (pluginEditorWindow == nullptr)
        return;

    pluginEditorWindow->setVisible(false);
    pluginEditorWindow.reset();
}

bool MainComponent::computeAutoEnableShouldEnable(bool& hasCondition) const
{
    hasCondition = false;
    bool shouldEnable = true;

    if (! cachedSettings.autoEnableProcessNames.isEmpty() || cachedSettings.autoEnableProcessName.isNotEmpty())
    {
        hasCondition = true;
        const auto fg = getForegroundProcessName();
        bool matched = false;
        if (! cachedSettings.autoEnableProcessNames.isEmpty())
        {
            for (const auto& n : cachedSettings.autoEnableProcessNames)
            {
                if (fg.equalsIgnoreCase(n))
                {
                    matched = true;
                    break;
                }
            }
        }
        else
        {
            matched = fg.equalsIgnoreCase(cachedSettings.autoEnableProcessName);
        }
        shouldEnable = shouldEnable && matched;
    }

    if (cachedSettings.autoEnableProcessPath.isNotEmpty())
    {
        hasCondition = true;
        const auto fgPath = getForegroundProcessPath();
        shouldEnable = shouldEnable && fgPath.equalsIgnoreCase(cachedSettings.autoEnableProcessPath);
    }

    return shouldEnable;
}

void MainComponent::setEffectsHint(const juce::String& text, int ticks)
{
    effectsHintLabel.setText(text, juce::dontSendNotification);
    effectsHintTicks = ticks < 0 ? -1 : juce::jmax(0, ticks);
    effectsHintTargetAlpha = text.isNotEmpty() ? 1.0f : 0.0f;
    if (text.isNotEmpty() && effectsHintAlpha < 0.05f)
        effectsHintAlpha = 0.05f;
}

void MainComponent::applyEffectsEnabledState(bool enabled, bool fromUserToggle)
{
    juce::ignoreUnused(fromUserToggle);
    effectsToggle.setToggleState(enabled, juce::dontSendNotification);
    effectsToggle.setButtonText(enabled ? "Effects On" : "Effects Off");
    params.bypass.store(! enabled);
    manualEffectsPinnedOn = enabled && cachedSettings.autoEnableByApp;

    if (! cachedSettings.autoEnableByApp)
    {
        manualEffectsOverrideAutoEnable = false;
        setEffectsHint({}, 0);
        return;
    }

    bool hasCondition = false;
    const auto autoWantsEnabled = computeAutoEnableShouldEnable(hasCondition);

    if (enabled)
    {
        // Manual ON must always work, even while auto-enable is active.
        manualEffectsOverrideAutoEnable = false;
        setEffectsHint({}, 0);
    }
    else
    {
        if (hasCondition && autoWantsEnabled)
        {
            manualEffectsOverrideAutoEnable = true;
            setEffectsHint("Overriding auto-enable", 150);
        }
        else
        {
            manualEffectsOverrideAutoEnable = false;
            setEffectsHint({}, 0);
        }
    }
}

void MainComponent::runAudioRestartWithOverlay(bool fromTray)
{
    if (restartOverlayBusy)
        return;

    restartOverlayActive = true;
    restartOverlayBusy = true;
    restartOverlayAlpha = juce::jmax(restartOverlayAlpha, 0.25f);
    restartOverlayTargetAlpha = 1.0f;
    restartOverlayTicks = 0;
    restartOverlayText.clear();
    repaint();

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(20, [safeThis, fromTray]
    {
        if (safeThis == nullptr)
            return;

        juce::String error;
        safeThis->engine.restartAudio(error);
        safeThis->loadDeviceLists();
        safeThis->restartOverlayBusy = false;
        safeThis->restartOverlayTicks = 18;

        if (error.isNotEmpty())
        {
            Logger::instance().log("Restart error: " + error);
            safeThis->setEffectsHint("Audio restart failed", 90);
            return;
        }

        safeThis->setEffectsHint(fromTray ? "Restarted audio from tray" : "Restarted audio", 55);
    });
}

void MainComponent::applyThemePalette()
{
    const auto salmonTheme = (cachedSettings.themeVariant == 1);
    const auto lightMode = cachedSettings.lightMode;

    if (lightMode)
    {
        kUiBg = juce::Colour(0xffe2e9f3);
        kUiPanel = juce::Colour(0xfff0f4fa);
        kUiPanelSoft = juce::Colour(0xffdce5f2);
        kUiText = juce::Colour(0xff132138);
        kUiTextMuted = juce::Colour(0xff395473);
    }
    else
    {
        kUiBg = juce::Colour(0xff0a111d);
        kUiPanel = juce::Colour(0xff172338);
        kUiPanelSoft = juce::Colour(0xff1e2d46);
        kUiText = juce::Colour(0xffeaf1ff);
        kUiTextMuted = juce::Colour(0xffa8bbdd);
    }

    if (salmonTheme)
    {
        kUiAccent = lightMode ? juce::Colour(0xffd46b8c) : juce::Colour(0xffe97f9f);
        kUiAccentSoft = lightMode ? juce::Colour(0xfff2a6be) : juce::Colour(0xffffb3c7);
        kUiMint = lightMode ? juce::Colour(0xffdd738f) : juce::Colour(0xffff9fb9);
    }
    else
    {
        kUiAccent = lightMode ? juce::Colour(0xff4d91d8) : juce::Colour(0xff6dbbff);
        kUiAccentSoft = lightMode ? juce::Colour(0xff7ec0ff) : juce::Colour(0xffa4ddff);
        kUiMint = lightMode ? juce::Colour(0xff66aac8) : juce::Colour(0xff9af7d8);
    }

    kUiBackgroundAlphaScale = cachedSettings.transparentBackground ? (lightMode ? 0.82f : 0.78f) : 1.0f;

    theme::background = kUiBg;
    theme::panel = kUiPanel;
    theme::panelSoft = kUiPanelSoft;
    theme::text = kUiText;
    theme::textMuted = kUiTextMuted;
    theme::accent = kUiAccent;
    theme::accentSoft = kUiAccentSoft;
    theme::mint = kUiMint;
    theme::knobTrack = lightMode ? kUiPanelSoft.darker(0.14f) : kUiPanelSoft.brighter(0.18f);

    setColour(juce::ComboBox::backgroundColourId, kUiPanelSoft);
    setColour(juce::ComboBox::outlineColourId, kUiAccent.withAlpha(0.28f));
    setColour(juce::ComboBox::textColourId, kUiText);
    setColour(juce::TextButton::textColourOffId, kUiText);
    setColour(juce::TextButton::textColourOnId, kUiText);
    setColour(juce::Label::textColourId, kUiText);
    setColour(juce::ToggleButton::textColourId, kUiText);
    setColour(juce::Slider::trackColourId, kUiAccent.withAlpha(0.72f));
    setColour(juce::Slider::thumbColourId, kUiAccentSoft);
    setColour(juce::Slider::textBoxTextColourId, kUiText);
    setColour(juce::Slider::textBoxBackgroundColourId, kUiPanelSoft.withAlpha(0.92f));
    setColour(juce::Slider::textBoxOutlineColourId, kUiAccent.withAlpha(0.30f));
    setColour(juce::Slider::textBoxHighlightColourId, kUiAccent.withAlpha(0.20f));
    setColour(juce::TextEditor::backgroundColourId, kUiPanel.withAlpha(0.72f));
    setColour(juce::TextEditor::outlineColourId, kUiAccent.withAlpha(0.24f));
    setColour(juce::TextEditor::textColourId, kUiText);
    setColour(juce::AlertWindow::backgroundColourId, kUiPanel);
    setColour(juce::AlertWindow::outlineColourId, kUiAccent.withAlpha(0.35f));
    setColour(juce::AlertWindow::textColourId, kUiText);
    setColour(juce::PopupMenu::backgroundColourId, kUiPanel);
    setColour(juce::PopupMenu::textColourId, kUiText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kUiAccent.withAlpha(0.22f));
    setColour(juce::PopupMenu::highlightedTextColourId, lightMode ? juce::Colour(0xff102038) : juce::Colours::white);

    if (auto* lf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel()))
        lf->refreshThemeColours();

    for (auto* l : { static_cast<juce::Label*>(&inputLabel),
                     static_cast<juce::Label*>(&outputLabel),
                     static_cast<juce::Label*>(&bufferLabel),
                     static_cast<juce::Label*>(&sampleRateLabel),
                     static_cast<juce::Label*>(&sampleRateValueLabel),
                     static_cast<juce::Label*>(&presetLabel),
                     static_cast<juce::Label*>(&currentPresetLabel),
                     static_cast<juce::Label*>(&virtualMicStatusLabel),
                     static_cast<juce::Label*>(&appEnableLabel),
                     static_cast<juce::Label*>(&appearanceLabel),
                     static_cast<juce::Label*>(&appSearchLabel),
                     static_cast<juce::Label*>(&appListLabel),
                     static_cast<juce::Label*>(&enabledProgramsLabel),
                     static_cast<juce::Label*>(&updatesLabel),
                     static_cast<juce::Label*>(&updateStatusLabel),
                     static_cast<juce::Label*>(&settingsNavLabel),
                     static_cast<juce::Label*>(&updatesLinksLabel),
                     static_cast<juce::Label*>(&startupLabel),
                     static_cast<juce::Label*>(&startupHintLabel),
                     static_cast<juce::Label*>(&appearanceModeLabel),
                     static_cast<juce::Label*>(&appearanceThemeLabel),
                     static_cast<juce::Label*>(&appearanceBackgroundLabel),
                     static_cast<juce::Label*>(&appearanceSizeLabel),
                     static_cast<juce::Label*>(&behaviorListenDeviceLabel),
                     static_cast<juce::Label*>(&behaviorVstFoldersLabel),
                     static_cast<juce::Label*>(&dragHintLabel),
                     static_cast<juce::Label*>(&title),
                     static_cast<juce::Label*>(&outputGainLabel),
                     static_cast<juce::Label*>(&versionLabel) })
    {
        if (l != nullptr)
            l->setColour(juce::Label::textColourId, kUiText);
    }

    dragHintLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    effectsHintLabel.setColour(juce::Label::textColourId, params.mute.load() ? kUiSalmon.withAlpha(0.95f) : kUiAccentSoft);
    updatesLinksLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    startupHintLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    versionLabel.setColour(juce::Label::textColourId, kUiText.withAlpha(0.22f));

    for (auto* editor : { &appSearchEditor, &appPathEditor })
    {
        editor->setColour(juce::TextEditor::backgroundColourId, kUiPanel.withAlpha(0.72f));
        editor->setColour(juce::TextEditor::outlineColourId, kUiAccent.withAlpha(0.24f));
        editor->setColour(juce::TextEditor::textColourId, kUiText);
        editor->setColour(juce::TextEditor::highlightedTextColourId, lightMode ? juce::Colour(0xff102038) : juce::Colours::white);
        editor->setColour(juce::TextEditor::highlightColourId, kUiAccent.withAlpha(0.24f));
    }

    outputGain.setColour(juce::Slider::trackColourId, kUiAccent.withAlpha(0.78f));
    outputGain.setColour(juce::Slider::thumbColourId, kUiAccentSoft);
    outputGain.setColour(juce::Slider::textBoxTextColourId, kUiText);
    outputGain.setColour(juce::Slider::textBoxBackgroundColourId, (lightMode ? kUiPanel : kUiPanelSoft).withAlpha(lightMode ? 0.86f : 0.92f));
    outputGain.setColour(juce::Slider::textBoxOutlineColourId, kUiAccent.withAlpha(0.34f));
    for (auto* cb : { static_cast<juce::ComboBox*>(&inputBox),
                      static_cast<juce::ComboBox*>(&outputBox),
                      static_cast<juce::ComboBox*>(&bufferBox),
                      static_cast<juce::ComboBox*>(&presetBox),
                      static_cast<juce::ComboBox*>(&vstAvailableBox),
                      static_cast<juce::ComboBox*>(&appearanceThemeBox),
                      static_cast<juce::ComboBox*>(&appearanceBackgroundBox),
                      static_cast<juce::ComboBox*>(&appearanceSizeBox),
                      static_cast<juce::ComboBox*>(&behaviorListenDeviceBox) })
    {
        if (cb == nullptr)
            continue;
        cb->setColour(juce::ComboBox::textColourId, kUiText);
        cb->setColour(juce::Label::textColourId, kUiText);
        cb->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        cb->setColour(juce::ComboBox::backgroundColourId, kUiPanelSoft.withAlpha(lightMode ? 0.9f : 0.85f));
        cb->setColour(juce::ComboBox::outlineColourId, kUiAccent.withAlpha(0.28f));
        cb->sendLookAndFeelChange();
        cb->repaint();
    }

    for (auto* b : { static_cast<juce::TextButton*>(&savePresetButton),
                     static_cast<juce::TextButton*>(&deletePresetButton),
                     static_cast<juce::TextButton*>(&rescanVstButton),
                     static_cast<juce::TextButton*>(&autoScanVstButton),
                     static_cast<juce::TextButton*>(&removeVstButton),
                     static_cast<juce::TextButton*>(&routeSpeakersButton),
                     static_cast<juce::TextButton*>(&restartButton),
                     static_cast<juce::TextButton*>(&aboutButton),
                     static_cast<juce::TextButton*>(&settingsButton),
                     static_cast<juce::TextButton*>(&refreshAppsButton),
                     static_cast<juce::TextButton*>(&browseAppPathButton),
                     static_cast<juce::TextButton*>(&removeProgramButton),
                     static_cast<juce::TextButton*>(&behaviorAddVstFolderButton),
                     static_cast<juce::TextButton*>(&behaviorRemoveVstFolderButton),
                     static_cast<juce::TextButton*>(&closeSettingsButton),
                     static_cast<juce::TextButton*>(&checkUpdatesButton),
                     static_cast<juce::TextButton*>(&updatesGithubButton),
                     static_cast<juce::TextButton*>(&updatesWebsiteButton),
                     static_cast<juce::TextButton*>(&windowMinButton),
                     static_cast<juce::TextButton*>(&windowMaxButton),
                     static_cast<juce::TextButton*>(&windowCloseButton),
                     static_cast<juce::TextButton*>(&outputGainResetButton) })
    {
        if (b != nullptr)
        {
            b->setColour(juce::TextButton::textColourOffId, kUiText);
            b->setColour(juce::TextButton::textColourOnId, kUiText);
        }
    }

    for (auto* t : { static_cast<juce::ToggleButton*>(&toneButton),
                     static_cast<juce::ToggleButton*>(&effectsToggle),
                     static_cast<juce::ToggleButton*>(&autoEnableToggle),
                     static_cast<juce::ToggleButton*>(&autoDownloadUpdatesToggle),
                     static_cast<juce::ToggleButton*>(&startWithWindowsToggle),
                     static_cast<juce::ToggleButton*>(&startMinimizedToggle),
                     static_cast<juce::ToggleButton*>(&followAutoEnableWindowToggle),
                     static_cast<juce::ToggleButton*>(&lightModeToggle) })
    {
        if (t != nullptr)
            t->setColour(juce::ToggleButton::textColourId, kUiText);
    }

    diagnostics.applyStyle(kUiControlScale);
    appListBox.repaint();
    enabledProgramsListBox.repaint();
    behaviorVstFoldersListBox.repaint();
    if (auto* window = findParentComponentOfClass<MainWindow>())
        window->setTransparentBackgroundEnabled(cachedSettings.transparentBackground);
    styleTransitionAlpha = 0.42f;
    sendLookAndFeelChange();
    repaint();
}

void MainComponent::applyUiDensity()
{
    switch (cachedSettings.uiDensity)
    {
        case 0:
            uiScale = 0.96f;
            kUiControlScale = 0.98f;
            break;
        case 2:
            uiScale = 1.44f;
            kUiControlScale = 1.34f;
            break;
        default:
            uiScale = 1.16f;
            kUiControlScale = 1.14f;
            break;
    }

    juce::Font titleFont(juce::FontOptions(25.0f * uiScale, juce::Font::plain));
    titleFont.setTypefaceStyle("SemiBold");
    titleFont.setExtraKerningFactor(0.042f);
    title.setFont(titleFont);

    juce::Font baseLabelFont(juce::FontOptions(15.0f * uiScale));
    baseLabelFont.setExtraKerningFactor(0.012f);
    for (auto* l : { static_cast<juce::Label*>(&inputLabel),
                     static_cast<juce::Label*>(&outputLabel),
                     static_cast<juce::Label*>(&bufferLabel),
                     static_cast<juce::Label*>(&sampleRateLabel),
                     static_cast<juce::Label*>(&sampleRateValueLabel),
                     static_cast<juce::Label*>(&presetLabel),
                     static_cast<juce::Label*>(&currentPresetLabel),
                     static_cast<juce::Label*>(&virtualMicStatusLabel) })
    {
        l->setFont(baseLabelFont);
    }

    juce::Font hintFont(juce::FontOptions(12.0f * uiScale));
    hintFont.setExtraKerningFactor(0.01f);
    dragHintLabel.setFont(hintFont);
    effectsHintLabel.setFont(juce::Font(juce::FontOptions(12.5f * uiScale, juce::Font::plain)));
    updateStatusLabel.setFont(juce::Font(juce::FontOptions(12.0f * uiScale)));

    juce::Font navFont(juce::FontOptions(16.0f * uiScale, juce::Font::bold));
    navFont.setExtraKerningFactor(0.02f);
    settingsNavLabel.setFont(navFont);

    juce::Font sectionLabelFont(juce::FontOptions(14.0f * uiScale, juce::Font::bold));
    sectionLabelFont.setExtraKerningFactor(0.01f);
    updatesLabel.setFont(sectionLabelFont);
    startupLabel.setFont(sectionLabelFont);
    appearanceLabel.setFont(sectionLabelFont);
    appearanceModeLabel.setFont(sectionLabelFont);
    appearanceThemeLabel.setFont(sectionLabelFont);
    appearanceBackgroundLabel.setFont(sectionLabelFont);
    appearanceSizeLabel.setFont(sectionLabelFont);
    behaviorListenDeviceLabel.setFont(sectionLabelFont);
    behaviorVstFoldersLabel.setFont(sectionLabelFont);

    juce::Font versionFont(juce::FontOptions(11.0f * uiScale));
    versionFont.setExtraKerningFactor(0.07f);
    versionLabel.setFont(versionFont);

    vstChainList.setRowHeight(juce::jmax(44, juce::roundToInt(56.0f * uiScale)));
    appListBox.setRowHeight(juce::jmax(22, juce::roundToInt(24.0f * uiScale)));
    enabledProgramsListBox.setRowHeight(juce::jmax(22, juce::roundToInt(24.0f * uiScale)));
    behaviorVstFoldersListBox.setRowHeight(juce::jmax(22, juce::roundToInt(24.0f * uiScale)));
    diagnostics.applyStyle(kUiControlScale);

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        const int minW = juce::jmax(840, juce::roundToInt(980.0f * uiScale));
        const int minH = juce::jmax(520, juce::roundToInt(620.0f * uiScale));
        const int maxW = juce::jmax(1400, juce::roundToInt(1600.0f * uiScale));
        const int maxH = juce::jmax(860, juce::roundToInt(980.0f * uiScale));
        window->setResizeLimits(minW, minH, maxW, maxH);
    }

    styleTransitionAlpha = 0.35f;
    resized();
    repaint();
}

void MainComponent::refreshAppearanceControls()
{
    lightModeToggle.setToggleState(cachedSettings.lightMode, juce::dontSendNotification);
    appearanceThemeBox.setSelectedId(cachedSettings.themeVariant + 1, juce::dontSendNotification);
    appearanceBackgroundBox.setSelectedId(cachedSettings.transparentBackground ? 1 : 2, juce::dontSendNotification);
    appearanceSizeBox.setSelectedId(cachedSettings.uiDensity + 1, juce::dontSendNotification);
}

void MainComponent::setUpdateStatus(const juce::String& text, bool warning)
{
    updateStatusLabel.setColour(juce::Label::textColourId, warning ? juce::Colour(0xffffb3b3) : kUiTextMuted);
    updateStatusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::triggerUpdateCheck(bool manualTrigger)
{
    if (updateCheckInFlight.exchange(true))
    {
        if (manualTrigger)
            setUpdateStatus("Update check already running...");
        return;
    }

    hasCheckedUpdatesThisSession = true;
    setUpdateStatus("Checking for updates...");

    const auto autoInstall = cachedSettings.autoInstallUpdates;
    juce::Component::SafePointer<MainComponent> safeThis(this);
    std::thread([safeThis, autoInstall, manualTrigger]
    {
        auto finish = [safeThis](juce::String message, bool warn, juce::String latestVersion)
        {
            juce::MessageManager::callAsync([safeThis, message = std::move(message), warn, latestVersion = std::move(latestVersion)]()
            {
                if (safeThis == nullptr)
                    return;
                safeThis->latestAvailableVersion = latestVersion;
                safeThis->updateCheckInFlight.store(false);
                safeThis->setUpdateStatus(message, warn);
            });
        };

        try
        {
            juce::String latestVersion;
            juce::String installerUrl;
            bool hasUpdate = false;

            {
                juce::URL api("https://api.github.com/repos/softlynn/Fizzle/releases/latest");
                auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                   .withExtraHeaders("User-Agent: Fizzle\r\nAccept: application/vnd.github+json\r\n")
                                   .withConnectionTimeoutMs(12000)
                                   .withNumRedirectsToFollow(4);
                auto in = api.createInputStream(options);
                if (in == nullptr)
                {
                    finish("Could not contact update server.", true, {});
                    return;
                }

                const auto jsonText = in->readEntireStreamAsString();
                const auto parsed = juce::JSON::parse(jsonText);
                auto* root = parsed.getDynamicObject();
                if (root == nullptr)
                {
                    finish("Update response was invalid.", true, {});
                    return;
                }

                const auto tag = root->getProperty("tag_name").toString();
                latestVersion = normalizeVersionTag(tag);
                hasUpdate = compareSemver(latestVersion, FIZZLE_VERSION) > 0;
                Logger::instance().log("Update check current=" + juce::String(FIZZLE_VERSION)
                                       + " latest=" + latestVersion
                                       + " hasUpdate=" + juce::String(hasUpdate ? 1 : 0));

                if (hasUpdate)
                {
                    if (auto* assets = root->getProperty("assets").getArray())
                    {
                        for (const auto& av : *assets)
                        {
                            auto* ao = av.getDynamicObject();
                            if (ao == nullptr)
                                continue;
                            const auto name = ao->getProperty("name").toString();
                            const auto url = ao->getProperty("browser_download_url").toString();
                            if (name.endsWithIgnoreCase(".exe") && url.isNotEmpty())
                            {
                                installerUrl = url;
                                break;
                            }
                        }
                    }
                }
            }

            if (! hasUpdate)
            {
                const auto msgPrefix = manualTrigger ? "No update found. " : juce::String{};
                finish(msgPrefix + "You're up to date (" + juce::String(FIZZLE_VERSION) + ").", false, latestVersion);
                return;
            }

            if (! autoInstall || installerUrl.isEmpty())
            {
                auto msg = "Update available: v" + latestVersion;
                if (! autoInstall)
                    msg << " (enable auto-install to update automatically)";
                else if (installerUrl.isEmpty())
                    msg << " (installer asset missing on release)";
                finish(msg, false, latestVersion);
                return;
            }

            auto downloadsDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads");
            downloadsDir.createDirectory();
            auto outFile = downloadsDir.getChildFile("Fizzle-Setup-" + latestVersion + ".exe");

            juce::URL dl(installerUrl);
            auto dlOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                 .withExtraHeaders("User-Agent: Fizzle\r\n")
                                 .withConnectionTimeoutMs(30000)
                                 .withNumRedirectsToFollow(4);
            auto dlIn = dl.createInputStream(dlOptions);
            if (dlIn == nullptr)
            {
                finish("Update found, but download failed to start.", true, latestVersion);
                return;
            }

            juce::TemporaryFile temp(outFile);
            {
                juce::FileOutputStream out(temp.getFile());
                if (! out.openedOk())
                {
                    finish("Update download failed (cannot write file).", true, latestVersion);
                    return;
                }
                const auto bytesWritten = out.writeFromInputStream(*dlIn, -1);
                out.flush();
                if (bytesWritten <= 0)
                {
                    finish("Update download failed (empty response).", true, latestVersion);
                    return;
                }
            }

            if (! temp.overwriteTargetFileWithTemporary())
            {
                finish("Update download failed (finalize error).", true, latestVersion);
                return;
            }

            bool launchedInstaller = false;
#if JUCE_WINDOWS
            const auto params = juce::String("/VERYSILENT /SUPPRESSMSGBOXES /NOCANCEL /NORESTART /CLOSEAPPLICATIONS /FORCECLOSEAPPLICATIONS /RESTARTAPPLICATIONS");
            const auto launchResult = reinterpret_cast<intptr_t>(ShellExecuteW(nullptr,
                                                                               L"open",
                                                                               outFile.getFullPathName().toWideCharPointer(),
                                                                               params.toWideCharPointer(),
                                                                               nullptr,
                                                                               SW_SHOWNORMAL));
            launchedInstaller = (launchResult > 32);
#endif

            if (! launchedInstaller)
            {
                finish("Update downloaded, but installer launch failed. File: " + outFile.getFullPathName(), true, latestVersion);
                return;
            }

            finish("Installing update v" + latestVersion + "...", false, latestVersion);
        }
        catch (const std::exception& ex)
        {
            finish("Update check failed: " + juce::String(ex.what()), true, {});
        }
        catch (...)
        {
            finish("Update check failed.", true, {});
        }
    }).detach();
}

PresetData MainComponent::buildCurrentPresetData(const juce::String& name)
{
    PresetData preset;
    preset.name = name;
    preset.engine = engine.currentSettings();
    preset.values["outputGainDb"] = params.outputGainDb.load();
    for (auto* plugin : engine.getVstHost().getChain())
    {
        if (plugin == nullptr || plugin->instance == nullptr)
            continue;

        PluginPresetState state;
        state.identifier = plugin->description.fileOrIdentifier;
        state.name = plugin->description.name;
        state.enabled = plugin->enabled;
        state.mix = plugin->mix;
        juce::MemoryBlock block;
        plugin->instance->getStateInformation(block);
        state.base64State = block.toBase64Encoding();
        preset.plugins.add(state);
    }
    return preset;
}

juce::String MainComponent::buildCurrentPresetSnapshot()
{
    auto obj = new juce::DynamicObject();
    auto settings = engine.currentSettings();
    obj->setProperty("inputDevice", settings.inputDeviceName);
    obj->setProperty("outputDevice", settings.outputDeviceName);
    obj->setProperty("bufferSize", settings.bufferSize);
    obj->setProperty("sampleRate", settings.preferredSampleRate);
    obj->setProperty("outputGainDb", params.outputGainDb.load());

    juce::Array<juce::var> pluginsArray;
    for (auto* plugin : engine.getVstHost().getChain())
    {
        if (plugin == nullptr || plugin->instance == nullptr)
            continue;
        auto pluginObj = new juce::DynamicObject();
        pluginObj->setProperty("identifier", plugin->description.fileOrIdentifier);
        pluginObj->setProperty("enabled", plugin->enabled);
        pluginObj->setProperty("mix", plugin->mix);
        juce::MemoryBlock block;
        plugin->instance->getStateInformation(block);
        pluginObj->setProperty("state", block.toBase64Encoding());
        pluginsArray.add(pluginObj);
    }
    obj->setProperty("plugins", pluginsArray);

    return juce::JSON::toString(juce::var(obj), false);
}

bool MainComponent::hasUnsavedPresetChanges()
{
    return buildCurrentPresetSnapshot() != lastPresetSnapshot;
}

void MainComponent::markCurrentPresetSnapshot()
{
    lastPresetSnapshot = buildCurrentPresetSnapshot();
}

juce::String MainComponent::loadPersistedLastPresetName() const
{
    const auto file = settingsStore.getAppDirectory().getChildFile("last_preset.txt");
    if (! file.existsAsFile())
        return {};
    return file.loadFileAsString().trim();
}

void MainComponent::persistLastPresetName(const juce::String& presetName) const
{
    const auto file = settingsStore.getAppDirectory().getChildFile("last_preset.txt");
    file.replaceWithText(presetName.trim());
}

void MainComponent::startRowDrag(int row)
{
    if (! juce::isPositiveAndBelow(row, getNumRows()))
        return;
    dragFromRow = row;
    dragToRow = row;
    vstChainList.selectRow(row);
}

void MainComponent::dragHoverRow(int row)
{
    if (! juce::isPositiveAndBelow(dragFromRow, getNumRows()) || ! juce::isPositiveAndBelow(row, getNumRows()))
        return;
    dragToRow = row;
    if (row != vstChainList.getSelectedRow())
        vstChainList.selectRow(row);
}

void MainComponent::finishRowDrag(int row)
{
    if (juce::isPositiveAndBelow(row, getNumRows()))
        dragToRow = row;

    const auto from = dragFromRow;
    const auto to = dragToRow;
    dragFromRow = -1;
    dragToRow = -1;

    if (! juce::isPositiveAndBelow(from, getNumRows()) || ! juce::isPositiveAndBelow(to, getNumRows()) || from == to)
    {
        if (juce::isPositiveAndBelow(to, getNumRows()))
            vstChainList.selectRow(to);
        return;
    }

    closePluginEditorWindow();
    engine.getVstHost().swapPlugin(from, to);
    refreshPluginChainUi();
    vstChainList.selectRow(to);
}

void MainComponent::setRowMix(int row, float mix)
{
    engine.getVstHost().setMix(row, mix);
}

void MainComponent::setRowEnabled(int row, bool enabled)
{
    if (! juce::isPositiveAndBelow(row, getNumRows()))
        return;
    engine.getVstHost().setEnabled(row, enabled);
    refreshPluginChainUi();
}

void MainComponent::capturePluginSnapshotForUndo(int index)
{
    lastRemovedPlugin = {};
    if (! juce::isPositiveAndBelow(index, getNumRows()))
        return;
    auto* plugin = engine.getVstHost().getPlugin(index);
    if (plugin == nullptr || plugin->instance == nullptr)
        return;

    juce::MemoryBlock block;
    plugin->instance->getStateInformation(block);
    lastRemovedPlugin.description = plugin->description;
    lastRemovedPlugin.base64State = block.toBase64Encoding();
    lastRemovedPlugin.enabled = plugin->enabled;
    lastRemovedPlugin.mix = plugin->mix;
    lastRemovedPlugin.index = index;
    lastRemovedPlugin.valid = true;
}

bool MainComponent::removePluginAtIndex(int index, bool captureUndo)
{
    if (! juce::isPositiveAndBelow(index, getNumRows()))
        return false;
    if (captureUndo)
        capturePluginSnapshotForUndo(index);
    closePluginEditorWindow();
    engine.getVstHost().removePlugin(index);
    refreshPluginChainUi();
    const auto newCount = getNumRows();
    if (newCount > 0)
        vstChainList.selectRow(juce::jlimit(0, newCount - 1, index));
    return true;
}

void MainComponent::restoreLastRemovedPlugin()
{
    if (! lastRemovedPlugin.valid)
        return;

    juce::String error;
    const auto d = engine.getDiagnostics();
    const auto sampleRate = d.sampleRate > 0.0 ? d.sampleRate : 48000.0;
    const auto blockSize = d.bufferSize > 0 ? d.bufferSize : kDefaultBlockSize;
    if (! engine.getVstHost().addPluginWithState(lastRemovedPlugin.description,
                                                 sampleRate,
                                                 blockSize,
                                                 lastRemovedPlugin.base64State,
                                                 error))
    {
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Undo Remove Failed", error);
        return;
    }

    const auto newIndex = getNumRows() - 1;
    engine.getVstHost().setEnabled(newIndex, lastRemovedPlugin.enabled);
    engine.getVstHost().setMix(newIndex, lastRemovedPlugin.mix);

    const auto targetIndex = juce::jlimit(0, juce::jmax(0, getNumRows() - 1), lastRemovedPlugin.index);
    if (targetIndex != newIndex)
        engine.getVstHost().movePlugin(newIndex, targetIndex);

    refreshPluginChainUi();
    vstChainList.selectRow(targetIndex);
    setEffectsHint("Restored removed VST", 50);
}

void MainComponent::rowQuickActionMenu(int row, juce::Point<int> screenPosition)
{
    if (! juce::isPositiveAndBelow(row, getNumRows()))
        return;

    auto* plugin = engine.getVstHost().getPlugin(row);
    if (plugin == nullptr)
        return;

    juce::PopupMenu m;
    m.addItem(1, plugin->enabled ? "Disable" : "Enable");
    m.addItem(2, "Solo");
    m.addItem(3, "Remove");

    juce::Component::SafePointer<MainComponent> safeThis(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPosition.x, screenPosition.y, 1, 1)),
                    [safeThis, row](int result)
    {
        if (safeThis == nullptr)
            return;
        if (result == 1)
        {
            if (auto* p = safeThis->engine.getVstHost().getPlugin(row))
                safeThis->setRowEnabled(row, ! p->enabled);
        }
        else if (result == 2)
        {
            const auto chain = safeThis->engine.getVstHost().getChain();
            for (int i = 0; i < chain.size(); ++i)
                safeThis->engine.getVstHost().setEnabled(i, i == row);
            safeThis->refreshPluginChainUi();
            safeThis->setEffectsHint("Soloed VST", 45);
        }
        else if (result == 3)
        {
            safeThis->removePluginAtIndex(row, true);
        }
    });
}

void MainComponent::loadPresetByName(const juce::String& name)
{
    if (auto preset = presetStore.loadPreset(name))
    {
        auto engineSettings = engine.currentSettings();
        if (preset->engine.inputDeviceName.isNotEmpty())
            engineSettings.inputDeviceName = preset->engine.inputDeviceName;
        if (preset->engine.outputDeviceName.isNotEmpty())
            engineSettings.outputDeviceName = preset->engine.outputDeviceName;
        if (preset->engine.bufferSize > 0)
            engineSettings.bufferSize = preset->engine.bufferSize;
        if (preset->engine.preferredSampleRate > 0.0)
            engineSettings.preferredSampleRate = preset->engine.preferredSampleRate;

        inputBox.setText(engineSettings.inputDeviceName, juce::dontSendNotification);
        {
            const auto outputIndex = outputDeviceRealNames.indexOf(engineSettings.outputDeviceName);
            if (outputIndex >= 0)
                outputBox.setSelectedId(outputIndex + 1, juce::dontSendNotification);
            else
                outputBox.setText(getDisplayOutputName(engineSettings.outputDeviceName), juce::dontSendNotification);
        }
        syncBufferBoxSelection(bufferBox, engineSettings.bufferSize);

        juce::String startError;
        if (! engine.start(engineSettings, startError))
            Logger::instance().log("Preset audio apply failed: " + startError);
        cachedSettings.inputDeviceName = engineSettings.inputDeviceName;
        cachedSettings.outputDeviceName = engineSettings.outputDeviceName;
        cachedSettings.bufferSize = engineSettings.bufferSize;
        cachedSettings.preferredSampleRate = engineSettings.preferredSampleRate;
        saveCachedSettings();

        if (const auto it = preset->values.find("outputGainDb"); it != preset->values.end())
        {
            params.outputGainDb.store(it->second);
            outputGain.setValue(it->second, juce::dontSendNotification);
        }

        closePluginEditorWindow();
        engine.getVstHost().clear();
        juce::String error;
        const auto d = engine.getDiagnostics();
        const auto sampleRate = d.sampleRate > 0.0 ? d.sampleRate : 48000.0;
        const auto blockSize = d.bufferSize > 0 ? d.bufferSize : kDefaultBlockSize;
        for (const auto& p : preset->plugins)
        {
            juce::PluginDescription description;
            if (engine.getVstHost().findDescriptionByIdentifier(p.identifier, description))
            {
                if (engine.getVstHost().addPluginWithState(description, sampleRate, blockSize, p.base64State, error))
                {
                    auto chain = engine.getVstHost().getChain();
                    if (auto* last = chain.getLast())
                    {
                        last->enabled = p.enabled;
                        last->mix = p.mix;
                    }
                }
            }
        }
        currentPresetName = preset->name;
        cachedSettings.lastPresetName = currentPresetName;
        saveCachedSettings();
        persistLastPresetName(currentPresetName);
        currentPresetLabel.setText("Current: " + currentPresetName, juce::dontSendNotification);
        presetBox.setText(currentPresetName, juce::dontSendNotification);
        refreshPluginChainUi();
        markCurrentPresetSnapshot();
    }
    else
    {
        currentPresetName = "Default";
        cachedSettings.lastPresetName = currentPresetName;
        saveCachedSettings();
        persistLastPresetName(currentPresetName);
        currentPresetLabel.setText("Current: Default", juce::dontSendNotification);
        presetBox.setText("Default", juce::dontSendNotification);
        markCurrentPresetSnapshot();
    }
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &windowMinButton)
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setMinimised(true);
    }
    else if (button == &windowMaxButton)
    {
        toggleWindowMaximize();
    }
    else if (button == &windowCloseButton)
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->closeButtonPressed();
    }
    else if (button == &restartButton)
    {
        runAudioRestartWithOverlay(false);
    }
    else if (button == &aboutButton)
    {
        triggerButtonFlash(button);
        juce::Component::SafePointer<MainComponent> safeThis(this);
        auto* aboutPrompt = new juce::AlertWindow("About Fizzle",
                                                  "Fizzle " + juce::String(FIZZLE_VERSION)
                                                    + "\n\nGitHub: " + kGithubRepoUrl
                                                    + "\nWebsite: " + kWebsiteUrl,
                                                  juce::AlertWindow::InfoIcon);
        aboutPrompt->addButton("Open GitHub", 1);
        aboutPrompt->addButton("Open Website", 2);
        aboutPrompt->addButton("First-time Tips", 3);
        aboutPrompt->addButton("Close", 0);
        aboutPrompt->enterModalState(true, juce::ModalCallbackFunction::create([safeThis](int result)
        {
            if (safeThis == nullptr)
                return;
            if (result == 1)
                juce::URL(kGithubRepoUrl).launchInDefaultBrowser();
            else if (result == 2)
                juce::URL(kWebsiteUrl).launchInDefaultBrowser();
            else if (result == 3)
                safeThis->showFirstLaunchGuide();
        }), true);
    }
    else if (button == &routeSpeakersButton)
    {
        if (! engine.isListenEnabled())
        {
            auto monitor = cachedSettings.listenMonitorDeviceName.trim();
            if (monitor.isEmpty())
                monitor = behaviorListenDeviceBox.getText().trim();
            if (monitor.isEmpty())
                monitor = findOutputByMatch({ "speaker", "headphones", "airpods", "bluetooth", "buds", "earbud", "headset", "realtek", "output" });
            if (monitor.isEmpty())
                return;
            if (monitor == getSelectedOutputDeviceName())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                       "Listen Output",
                                                       "Listen output must be different from the Virtual Mic output.");
                return;
            }
            cachedSettings.listenMonitorDeviceName = monitor;
            saveCachedSettings();
            engine.setMonitorOutputDevice(monitor);
            engine.setListenEnabled(true);
            routeSpeakersButton.setButtonText("Listening");
            routeSpeakersButton.setToggleState(true, juce::dontSendNotification);
        }
        else
        {
            engine.setListenEnabled(false);
            routeSpeakersButton.setButtonText("Listen");
            routeSpeakersButton.setToggleState(false, juce::dontSendNotification);
        }
    }
    else if (button == &outputGainResetButton)
    {
        outputGain.setValue(0.0, juce::sendNotificationSync);
    }
    else if (button == &autoScanVstButton)
    {
        autoScanVstFolders();
    }
    else if (button == &removeVstButton)
    {
        const auto selected = vstChainList.getSelectedRow();
        if (selected >= 0)
        {
            removePluginAtIndex(selected, true);
        }
        refreshPluginChainUi();
    }
    else if (button == &savePresetButton)
    {
        triggerButtonFlash(button);
        auto* prompt = new juce::AlertWindow("Save Preset", "Preset name:", juce::AlertWindow::NoIcon);
        prompt->addTextEditor("name", currentPresetName, "Name:");
        prompt->addButton("Save", 1);
        prompt->addButton("Cancel", 0);
        prompt->enterModalState(true, juce::ModalCallbackFunction::create([this, prompt](int result)
        {
            if (result != 1)
                return;

            auto name = prompt->getTextEditorContents("name").trim();
            if (name.isEmpty())
                name = "Preset-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");

            auto preset = buildCurrentPresetData(name);
            presetStore.savePreset(preset);
            currentPresetName = preset.name;
            cachedSettings.lastPresetName = currentPresetName;
            saveCachedSettings();
            persistLastPresetName(currentPresetName);
            currentPresetLabel.setText("Current: " + currentPresetName, juce::dontSendNotification);
            refreshPresets();
            markCurrentPresetSnapshot();
        }), true);
    }
    else if (button == &deletePresetButton)
    {
        const auto name = presetBox.getText().trim();
        if (name.isEmpty() || name == "Default")
            return;

        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                           "Delete Preset",
                                           "Delete preset \"" + name + "\"?",
                                           "Delete",
                                           "Cancel",
                                           this,
                                           juce::ModalCallbackFunction::create([this, name](int result)
        {
            if (result == 0)
                return;
            if (presetStore.deletePreset(name))
            {
                currentPresetName = "Default";
                cachedSettings.lastPresetName = currentPresetName;
                saveCachedSettings();
                persistLastPresetName(currentPresetName);
                currentPresetLabel.setText("Current: Default", juce::dontSendNotification);
                refreshPresets();
                presetBox.setText("Default", juce::dontSendNotification);
                markCurrentPresetSnapshot();
            }
        }));
    }
    else if (button == &rescanVstButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Select VST3 folder");
        const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            const auto folder = chooser.getResult();
            if (! folder.isDirectory())
                return;
            addVstSearchPath(folder.getFullPathName());
            engine.getVstHost().scanFolder(folder);
            cachedSettings.scannedVstPaths = engine.getVstHost().getScannedPaths();
            saveCachedSettings();
            refreshKnownPlugins();
        });
    }
    else if (button == &toneButton)
    {
        engine.setTestToneEnabled(toneButton.getToggleState());
    }
    else if (button == &effectsToggle)
    {
        applyEffectsEnabledState(effectsToggle.getToggleState(), true);
    }
    else if (button == &settingsButton)
    {
        setSettingsPanelVisible(! settingsPanelTargetVisible);
    }
    else if (button == &settingsNavAutoEnableButton)
    {
        setActiveSettingsTab(0);
    }
    else if (button == &settingsNavAppearanceButton)
    {
        setActiveSettingsTab(1);
    }
    else if (button == &settingsNavUpdatesButton)
    {
        setActiveSettingsTab(2);
    }
    else if (button == &settingsNavStartupButton)
    {
        setActiveSettingsTab(3);
    }
    else if (button == &refreshAppsButton)
    {
        refreshRunningApps();
    }
    else if (button == &removeProgramButton)
    {
        removeSelectedProgram();
    }
    else if (button == &closeSettingsButton)
    {
        setSettingsPanelVisible(false);
    }
    else if (button == &browseAppPathButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Select application executable", juce::File(), "*.exe");
        const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            const auto f = chooser.getResult();
            if (! f.existsAsFile())
                return;
            addAutoEnableProgramFromPath(f.getFullPathName());
        });
    }
    else if (button == &behaviorAddVstFolderButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Select VST3 folder");
        const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            const auto folder = chooser.getResult();
            if (! folder.isDirectory())
                return;
            addVstSearchPath(folder.getFullPathName());
        });
    }
    else if (button == &behaviorRemoveVstFolderButton)
    {
        removeSelectedVstSearchPath();
    }
    else if (button == &autoEnableToggle)
    {
        cachedSettings.autoEnableByApp = autoEnableToggle.getToggleState();
        saveCachedSettings();
        if (! cachedSettings.autoEnableByApp)
        {
            manualEffectsOverrideAutoEnable = false;
            manualEffectsPinnedOn = false;
            setEffectsHint({}, 0);
        }
        else
        {
            bool hasCondition = false;
            const auto shouldEnable = computeAutoEnableShouldEnable(hasCondition);
            if (hasCondition && shouldEnable && ! manualEffectsOverrideAutoEnable)
            {
                params.bypass.store(false);
                effectsToggle.setToggleState(true, juce::dontSendNotification);
                effectsToggle.setButtonText("Effects On");
                setEffectsHint({}, 0);
            }
        }
    }
    else if (button == &autoDownloadUpdatesToggle)
    {
        cachedSettings.autoInstallUpdates = autoDownloadUpdatesToggle.getToggleState();
        saveCachedSettings();
        if (cachedSettings.autoInstallUpdates)
        {
            setUpdateStatus("Auto-install is enabled.");
            if (! hasCheckedUpdatesThisSession)
                triggerUpdateCheck(false);
        }
        else
        {
            setUpdateStatus("Auto-install is off.");
        }
    }
    else if (button == &lightModeToggle)
    {
        cachedSettings.lightMode = lightModeToggle.getToggleState();
        saveCachedSettings();
        applyThemePalette();
    }
    else if (button == &startWithWindowsToggle)
    {
        cachedSettings.startWithWindows = startWithWindowsToggle.getToggleState();
        saveCachedSettings();
        applyWindowsStartupSetting();
    }
    else if (button == &startMinimizedToggle)
    {
        cachedSettings.startMinimizedToTray = startMinimizedToggle.getToggleState();
        saveCachedSettings();
        applyWindowsStartupSetting();
    }
    else if (button == &followAutoEnableWindowToggle)
    {
        cachedSettings.followAutoEnableWindowState = followAutoEnableWindowToggle.getToggleState();
        saveCachedSettings();
    }
    else if (button == &checkUpdatesButton)
    {
        triggerUpdateCheck(true);
    }
    else if (button == &updatesGithubButton)
    {
        juce::URL(kGithubRepoUrl).launchInDefaultBrowser();
    }
    else if (button == &updatesWebsiteButton)
    {
        juce::URL(kWebsiteUrl).launchInDefaultBrowser();
    }
}

void MainComponent::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &inputBox || comboBoxThatHasChanged == &outputBox || comboBoxThatHasChanged == &bufferBox)
    {
        if (suppressControlCallbacks)
            return;

        if (audioApplyQueued)
            return;

        audioApplyQueued = true;
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis == nullptr)
                return;
            safeThis->audioApplyQueued = false;
            safeThis->applySettingsFromControls();
        });
    }
    else if (comboBoxThatHasChanged == &vstAvailableBox)
    {
        if (suppressPluginAddFromSelection)
            return;
        const auto selected = vstAvailableBox.getSelectedId() - 1;
        auto known = engine.getVstHost().getKnownPluginDescriptions();
        if (selected >= 0 && selected < known.size())
        {
            juce::String error;
            const auto d = engine.getDiagnostics();
            const auto sampleRate = d.sampleRate > 0.0 ? d.sampleRate : 48000.0;
            const auto blockSize = d.bufferSize > 0 ? d.bufferSize : kDefaultBlockSize;
            engine.getVstHost().addPlugin(known.getReference(selected), sampleRate, blockSize, error);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin Load Failed", error);
            refreshPluginChainUi();
        }
    }
    else if (comboBoxThatHasChanged == &appearanceThemeBox)
    {
        cachedSettings.themeVariant = juce::jlimit(0, 1, appearanceThemeBox.getSelectedId() - 1);
        saveCachedSettings();
        applyThemePalette();
    }
    else if (comboBoxThatHasChanged == &appearanceBackgroundBox)
    {
        cachedSettings.transparentBackground = (appearanceBackgroundBox.getSelectedId() == 1);
        saveCachedSettings();
        applyThemePalette();
    }
    else if (comboBoxThatHasChanged == &appearanceSizeBox)
    {
        cachedSettings.uiDensity = juce::jlimit(0, 2, appearanceSizeBox.getSelectedId() - 1);
        saveCachedSettings();
        applyUiDensity();
    }
    else if (comboBoxThatHasChanged == &behaviorListenDeviceBox)
    {
        cachedSettings.listenMonitorDeviceName = behaviorListenDeviceBox.getText().trim();
        saveCachedSettings();
        if (engine.isListenEnabled() && cachedSettings.listenMonitorDeviceName.isNotEmpty())
        {
            engine.setMonitorOutputDevice(cachedSettings.listenMonitorDeviceName);
            engine.setListenEnabled(true);
        }
    }
    else if (comboBoxThatHasChanged == &presetBox)
    {
        const auto nextPreset = presetBox.getText().trim();
        if (nextPreset.isEmpty() || nextPreset == currentPresetName)
            return;

        pendingPresetName = nextPreset;
        if (! hasUnsavedPresetChanges())
        {
            loadPresetByName(pendingPresetName);
            refreshPresets();
            presetBox.setText(currentPresetName, juce::dontSendNotification);
            return;
        }

        juce::AlertWindow::showYesNoCancelBox(juce::AlertWindow::QuestionIcon,
                                              "Load Preset",
                                              "Save current preset before loading \"" + pendingPresetName + "\"?",
                                              "Save + Load",
                                              "Load Only",
                                              "Cancel",
                                              this,
                                              juce::ModalCallbackFunction::create([this](int result)
        {
            if (result == 0)
            {
                presetBox.setText(currentPresetName, juce::dontSendNotification);
                return;
            }

            if (result == 1)
            {
                auto* prompt = new juce::AlertWindow("Save Preset", "Preset name:", juce::AlertWindow::NoIcon);
                prompt->addTextEditor("name", currentPresetName, "Name:");
                prompt->addButton("Save", 1);
                prompt->addButton("Skip Save", 2);
                prompt->addButton("Cancel", 0);
                prompt->enterModalState(true, juce::ModalCallbackFunction::create([this, prompt](int saveResult)
                {
                    if (saveResult == 0)
                    {
                        presetBox.setText(currentPresetName, juce::dontSendNotification);
                        return;
                    }

                    if (saveResult == 1)
                    {
                        auto saveName = prompt->getTextEditorContents("name").trim();
                        if (saveName.isEmpty())
                            saveName = "Preset-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");

                        auto preset = buildCurrentPresetData(saveName);
                        presetStore.savePreset(preset);
                        currentPresetName = preset.name;
                        cachedSettings.lastPresetName = currentPresetName;
                        saveCachedSettings();
                        persistLastPresetName(currentPresetName);
                        markCurrentPresetSnapshot();
                    }

                    loadPresetByName(pendingPresetName);
                    refreshPresets();
                    presetBox.setText(currentPresetName, juce::dontSendNotification);
                }), true);
                return;
            }

            loadPresetByName(pendingPresetName);
            refreshPresets();
            presetBox.setText(currentPresetName, juce::dontSendNotification);
        }));
    }
}

void MainComponent::timerCallback()
{
    auto setTimerRate = [this](int desired)
    {
        if (desired != uiTimerHz)
        {
            uiTimerHz = desired;
            startTimerHz(uiTimerHz);
        }
    };

    ++uiTickCount;

    if (draggingWindow)
    {
        uiPulse += 0.010f;
        setTimerRate(60);
        repaint(headerBounds.expanded(28, 12));
        return;
    }

    auto d = engine.getDiagnostics();
    meterIn.setLevel(d.inputLevel);
    meterOut.setLevel(d.outputLevel);

    if (std::abs(d.sampleRate - lastDisplayedSampleRateHz) > 1.0)
    {
        lastDisplayedSampleRateHz = d.sampleRate;
        sampleRateValueLabel.setText(juce::String(d.sampleRate / 1000.0, 1) + " kHz", juce::dontSendNotification);
    }

    const auto out = getSelectedOutputDeviceName();
    const bool virt = isVirtualMicName(out);
    const juce::String currentVirtualStatus = virt ? "Fizzle Mic: routed" : "Fizzle Mic: not routed";
    if (currentVirtualStatus != lastVirtualMicStatus)
    {
        lastVirtualMicStatus = currentVirtualStatus;
        virtualMicStatusLabel.setText(currentVirtualStatus, juce::dontSendNotification);
    }

    if (cachedSettings.autoEnableByApp && (uiTickCount % 10 == 0))
    {
        bool hasCondition = false;
        const auto shouldEnable = computeAutoEnableShouldEnable(hasCondition);
        if (manualEffectsPinnedOn)
        {
            params.bypass.store(false);
            effectsToggle.setToggleState(true, juce::dontSendNotification);
            effectsToggle.setButtonText("Effects On");
            setEffectsHint({}, 0);
        }
        else
        {
            if (! hasCondition)
            {
                // No auto-enable condition configured: do not interfere with manual control.
                manualEffectsOverrideAutoEnable = false;
            }
            else if (shouldEnable)
            {
                if (manualEffectsOverrideAutoEnable && ! effectsToggle.getToggleState())
                {
                    setEffectsHint("Overriding auto-enable", 45);
                }
                else
                {
                    manualEffectsOverrideAutoEnable = false;
                    params.bypass.store(false);
                    effectsToggle.setToggleState(true, juce::dontSendNotification);
                    effectsToggle.setButtonText("Effects On");
                    setEffectsHint({}, 0);
                }
            }
            else if (! shouldEnable)
            {
                manualEffectsOverrideAutoEnable = false;
            }
        }

        if (cachedSettings.followAutoEnableWindowState && hasCondition)
        {
            if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            {
                if (shouldEnable && ! wasFollowAutoEnableMatched)
                {
                    window->setVisible(true);
                    window->setMinimised(false);
                    window->toFront(true);
                }
                else if (! shouldEnable && wasFollowAutoEnableMatched)
                {
                    window->setVisible(false);
                }
            }
            wasFollowAutoEnableMatched = shouldEnable;
        }
        else
        {
            wasFollowAutoEnableMatched = false;
        }
    }

    const auto listenEnabled = engine.isListenEnabled();
    const bool listenStateChanged = (listenEnabled != lastListenEnabledState);
    lastListenEnabledState = listenEnabled;

    const auto muted = params.mute.load();
    const bool muteStateChanged = (muted != lastMutedState);
    lastMutedState = muted;
    effectsHintLabel.setColour(juce::Label::textColourId, muted ? kUiSalmon.withAlpha(0.95f) : kUiAccentSoft);
    if (muted && ! effectsHintLabel.getText().containsIgnoreCase("Muted"))
        setEffectsHint("Muted from tray", -1);
    else if (! muted && effectsHintTicks < 0)
        setEffectsHint({}, 0);

    if (effectsHintTicks > 0)
    {
        --effectsHintTicks;
        if (effectsHintTicks == 0)
            effectsHintTargetAlpha = 0.0f;
    }

    const auto hintBefore = effectsHintAlpha;
    effectsHintAlpha += (effectsHintTargetAlpha - effectsHintAlpha) * 0.22f;
    if (std::abs(effectsHintAlpha - effectsHintTargetAlpha) < 0.01f)
        effectsHintAlpha = effectsHintTargetAlpha;
    effectsHintLabel.setAlpha(effectsHintAlpha);
    if (effectsHintAlpha <= 0.01f && effectsHintTargetAlpha <= 0.01f && effectsHintTicks == 0)
        effectsHintLabel.setText({}, juce::dontSendNotification);

    if (logoFizzActive)
    {
        for (auto& b : logoFizzBubbles)
        {
            b.pos.y -= b.speed;
            b.pos.x += std::sin((uiPulse * 1.6f) + (b.pos.y * 0.014f)) * b.drift;
            b.alpha -= 0.0035f;
        }
        logoFizzBubbles.erase(std::remove_if(logoFizzBubbles.begin(), logoFizzBubbles.end(),
                                             [this](const FizzBubble& b)
                                             {
                                                 return b.alpha <= 0.0f || b.pos.y < -24.0f;
                                             }),
                              logoFizzBubbles.end());

        if (logoFizzTicks > 0)
        {
            --logoFizzTicks;
            juce::Random random(static_cast<juce::int64>(juce::Time::getMillisecondCounterHiRes()));
            const auto bounds = getLocalBounds().toFloat().reduced(16.0f, 12.0f);
            for (int i = 0; i < 2; ++i)
            {
                FizzBubble b;
                b.pos = { bounds.getX() + random.nextFloat() * bounds.getWidth(),
                          bounds.getBottom() + random.nextFloat() * 12.0f };
                b.radius = (2.4f + random.nextFloat() * 8.5f) * kUiControlScale;
                b.speed = (0.75f + random.nextFloat() * 2.2f) * kUiControlScale;
                b.drift = (random.nextFloat() - 0.5f) * 0.8f;
                b.alpha = 0.24f + random.nextFloat() * 0.40f;
                logoFizzBubbles.push_back(b);
            }
        }
        else if (logoFizzBubbles.empty())
        {
            logoFizzActive = false;
        }
    }

    bool settingsAlphaChanged = false;
    if (settingsPanel != nullptr)
    {
        const auto target = settingsPanelTargetVisible ? 1.0f : 0.0f;
        const auto before = settingsPanelAlpha;
        settingsPanelAlpha += (target - settingsPanelAlpha) * 0.78f;
        if (std::abs(settingsPanelAlpha - target) < 0.01f)
            settingsPanelAlpha = target;
        settingsAlphaChanged = std::abs(settingsPanelAlpha - before) > 0.001f;

        settingsPanel->setAlpha(settingsPanelAlpha);
        if (! settingsPanelTargetVisible && settingsPanelAlpha <= 0.01f)
            settingsPanel->setVisible(false);
        else if (settingsPanelTargetVisible && ! settingsPanel->isVisible())
            settingsPanel->setVisible(true);
    }

    bool overlayAlphaChanged = false;
    if (restartOverlayBusy)
    {
        restartOverlayTargetAlpha = 1.0f;
    }
    else if (restartOverlayTicks > 0)
    {
        --restartOverlayTicks;
    }
    else
    {
        restartOverlayTargetAlpha = 0.0f;
    }

    const auto overlayBefore = restartOverlayAlpha;
    restartOverlayAlpha += (restartOverlayTargetAlpha - restartOverlayAlpha) * 0.24f;
    if (std::abs(restartOverlayAlpha - restartOverlayTargetAlpha) < 0.01f)
        restartOverlayAlpha = restartOverlayTargetAlpha;
    overlayAlphaChanged = std::abs(restartOverlayAlpha - overlayBefore) > 0.001f;

    if (! restartOverlayBusy && restartOverlayTicks == 0 && restartOverlayAlpha <= 0.01f)
    {
        restartOverlayAlpha = 0.0f;
        restartOverlayActive = false;
        restartOverlayText.clear();
    }
    else if (restartOverlayAlpha > 0.01f || restartOverlayBusy)
    {
        restartOverlayActive = true;
    }

    savePresetFlashAlpha *= 0.84f;
    aboutFlashAlpha *= 0.84f;
    styleTransitionAlpha *= 0.86f;
    if (savePresetFlashAlpha < 0.01f)
        savePresetFlashAlpha = 0.0f;
    if (aboutFlashAlpha < 0.01f)
        aboutFlashAlpha = 0.0f;
    if (styleTransitionAlpha < 0.01f)
        styleTransitionAlpha = 0.0f;

    juce::Rectangle<int> dirty;
    bool hasDirty = false;
    auto addDirty = [&dirty, &hasDirty](juce::Rectangle<int> r)
    {
        if (r.isEmpty())
            return;
        dirty = hasDirty ? dirty.getUnion(r) : r;
        hasDirty = true;
    };

    if (savePresetFlashAlpha > 0.0f)
        addDirty(savePresetButton.getBounds().expanded(6, 4));
    if (aboutFlashAlpha > 0.0f)
        addDirty(aboutButton.getBounds().expanded(6, 4));
    if (listenStateChanged)
        addDirty(routeSpeakersButton.getBounds().expanded(10, 6));
    if (muteStateChanged)
        addDirty(effectsToggle.getBounds().expanded(140, 18));
    if (overlayAlphaChanged || restartOverlayActive)
        addDirty(getLocalBounds());
    if (std::abs(effectsHintAlpha - hintBefore) > 0.0005f)
        addDirty(effectsHintLabel.getBounds().expanded(8, 4));
    if (styleTransitionAlpha > 0.0f)
        addDirty(getLocalBounds());
    if (logoFizzActive)
        addDirty(getLocalBounds());

    if (settingsAlphaChanged && settingsPanel != nullptr && settingsPanel->isVisible())
        settingsPanel->repaint();

    const bool highFpsNeeded = draggingResizeGrip
                            || logoFizzActive
                            || restartOverlayBusy
                            || restartOverlayAlpha > 0.02f;
    const bool mediumFpsNeeded = settingsPanelTargetVisible
                              || settingsPanelAlpha > 0.02f
                              || styleTransitionAlpha > 0.02f
                              || savePresetFlashAlpha > 0.02f
                              || aboutFlashAlpha > 0.02f
                              || effectsHintAlpha > 0.02f
                              || effectsHintTargetAlpha > 0.02f;
    const bool dividerAnimationNeeded = ! highFpsNeeded
                                     && ! mediumFpsNeeded
                                     && ! (settingsPanel != nullptr && settingsPanel->isVisible() && settingsPanelAlpha > 0.02f)
                                     && (uiTickCount % 4 == 0);

    if (highFpsNeeded || mediumFpsNeeded || dividerAnimationNeeded)
    {
        uiPulse += highFpsNeeded ? 0.020f : 0.010f;
        addDirty(headerBounds.expanded(18, 10));
    }

    setTimerRate(highFpsNeeded ? 60 : (mediumFpsNeeded ? 30 : 15));

    if (hasDirty)
        repaint(dirty);
}

int MainComponent::getNumRows()
{
    return engine.getVstHost().getChain().size();
}

void MainComponent::paintListBoxItem(int, juce::Graphics&, int, int, bool)
{
}

juce::Component* MainComponent::refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existing)
{
    auto* row = dynamic_cast<VstRowComponent*>(existing);
    if (row == nullptr)
    {
        row = new VstRowComponent(
            [this](int r) { startRowDrag(r); },
            [this](int r) { dragHoverRow(r); },
            [this](int r) { finishRowDrag(r); },
            [this](int r) { rowOpenEditor(r); },
            [this](int r, float m) { setRowMix(r, m); },
            [this](int r)
            {
                if (auto* p = engine.getVstHost().getPlugin(r))
                    setRowEnabled(r, ! p->enabled);
            },
            [this](int r, juce::Point<int> pt) { rowQuickActionMenu(r, pt); });
    }

    if (auto* plugin = engine.getVstHost().getPlugin(rowNumber))
        row->setRowData(rowNumber, plugin->description.name, plugin->enabled, plugin->mix, isRowSelected);

    return row;
}

void MainComponent::paint(juce::Graphics& g)
{
    const bool transparentMode = cachedSettings.transparentBackground;
    g.fillAll(juce::Colours::transparentBlack);
    auto shell = getLocalBounds().toFloat().reduced(6.0f, 2.0f);
    constexpr float shellRadius = 25.0f;
    auto makePanelPath = [](juce::Rectangle<float> r, float radius)
    {
        juce::Path p;
        p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                              radius, radius,
                              true, true, true, true);
        return p;
    };

    auto shellPath = makePanelPath(shell, shellRadius);
    g.reduceClipRegion(shellPath);

    const bool lowCostPass = draggingWindow || draggingResizeGrip;
    const bool lightMode = cachedSettings.lightMode;
    const float shellBaseAlpha = transparentMode ? (lightMode ? 0.86f : 0.82f) : 1.0f;
    const auto bgTop = transparentMode ? kUiBg.brighter(lightMode ? 0.05f : 0.02f) : kUiBg.brighter(0.05f);
    const auto bgBottom = transparentMode ? kUiBg.darker(lightMode ? 0.10f : 0.12f) : kUiBg.darker(0.14f);
    juce::ColourGradient bg(bgTop.withAlpha(shellBaseAlpha * kUiBackgroundAlphaScale),
                            shell.getX(), shell.getY(),
                            bgBottom.withAlpha((shellBaseAlpha * 0.96f) * kUiBackgroundAlphaScale),
                            shell.getX(), shell.getBottom(), false);
    g.setGradientFill(bg);
    g.fillPath(shellPath);
    if (transparentMode)
    {
        g.setColour(juce::Colours::black.withAlpha(lightMode ? 0.11f : 0.10f));
        g.fillPath(shellPath);
    }
    if (! lowCostPass)
    {
        g.setTiledImageFill(getDitherNoiseTile(), 0, 0, transparentMode ? 0.018f : 0.028f);
        g.fillPath(shellPath);
    }

    auto outer = shell.expanded(2.0f, 1.6f);
    g.setColour(kUiAccent.withAlpha(transparentMode ? 0.085f : 0.065f));
    g.strokePath(makePanelPath(outer, shellRadius + 2.0f), juce::PathStrokeType(transparentMode ? 1.7f : 1.4f));
    g.setColour(kUiSalmon.withAlpha(transparentMode ? 0.060f : 0.045f));
    g.strokePath(makePanelPath(shell.expanded(0.9f, 0.8f), shellRadius + 0.8f), juce::PathStrokeType(transparentMode ? 1.05f : 0.9f));

    auto card = shell.reduced(2.0f, 1.0f);
    const auto cardTopAlpha = transparentMode ? (lightMode ? 0.78f : 0.72f) : 0.90f;
    const auto cardBottomAlpha = transparentMode ? (lightMode ? 0.84f : 0.78f) : 0.96f;
    juce::ColourGradient cardFill(kUiPanelSoft.withAlpha(cardTopAlpha * kUiBackgroundAlphaScale), card.getX(), card.getY(),
                                  kUiPanel.withAlpha(cardBottomAlpha * kUiBackgroundAlphaScale), card.getX(), card.getBottom(), false);
    g.setGradientFill(cardFill);
    g.fillPath(makePanelPath(card, 21.0f));
    if (! lowCostPass)
    {
        g.setTiledImageFill(getDitherNoiseTile(), 0, 0, transparentMode ? 0.014f : 0.020f);
        g.fillPath(makePanelPath(card, 21.0f));

        juce::ColourGradient gloss(juce::Colours::white.withAlpha(transparentMode ? 0.12f : 0.08f),
                                   card.getX(), card.getY(),
                                   juce::Colours::white.withAlpha(0.0f),
                                   card.getX(), card.getY() + card.getHeight() * 0.38f, false);
        g.setGradientFill(gloss);
        g.fillRoundedRectangle(card.removeFromTop(card.getHeight() * 0.46f), 21.0f);
    }

    auto header = headerBounds.toFloat().expanded(2.0f, 1.0f);
    auto headerBand = header;
    headerBand.setBottom(headerBand.getBottom() + 16.0f);
    juce::Path headerPath;
    headerPath.addRoundedRectangle(headerBand.getX(), headerBand.getY(), headerBand.getWidth(), headerBand.getHeight(),
                                   15.0f, 15.0f,
                                   true, true, false, false);
    juce::ColourGradient headerFill(kUiPanelSoft.withAlpha(transparentMode ? 0.68f : 0.52f), headerBand.getX(), headerBand.getY(),
                                    kUiPanel.withAlpha(transparentMode ? 0.32f : 0.07f), headerBand.getX(), headerBand.getBottom(), false);
    g.setGradientFill(headerFill);
    g.fillPath(headerPath);
    g.setColour(kUiAccent.withAlpha(transparentMode ? 0.16f : 0.11f));
    g.strokePath(headerPath, juce::PathStrokeType(0.9f));

    auto chainPane = vstChainList.getBounds().toFloat().expanded(2.0f, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(transparentMode ? 0.018f : 0.02f));
    g.fillRoundedRectangle(chainPane, 10.0f);
    g.setColour(kUiAccent.withAlpha(0.16f));
    g.drawRoundedRectangle(chainPane, 10.0f, 1.1f);
    auto diagPane = diagnostics.getBounds().toFloat().expanded(2.0f, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(transparentMode ? 0.018f : 0.02f));
    g.fillRoundedRectangle(diagPane, 10.0f);
    g.setColour(kUiAccent.withAlpha(0.14f));
    g.drawRoundedRectangle(diagPane, 10.0f, 1.1f);

    g.setColour(kUiAccent.withAlpha(0.042f));
    g.strokePath(makePanelPath(shell.reduced(0.8f, 0.2f), shellRadius - 0.8f), juce::PathStrokeType(0.95f));
    g.setColour(kUiAccentSoft.withAlpha(0.022f));
    g.strokePath(makePanelPath(shell.expanded(1.2f, 0.4f), shellRadius + 1.1f), juce::PathStrokeType(0.85f));

    if (appLogo.isValid() && ! headerLogoBounds.isEmpty())
    {
        g.setColour(kUiAccent.withAlpha(0.36f));
        g.fillEllipse(headerLogoBounds.toFloat().expanded(2.4f));
        g.setColour(kUiAccentSoft.withAlpha(0.45f));
        g.drawEllipse(headerLogoBounds.toFloat().expanded(2.4f), 1.15f);
        g.drawImageWithin(appLogo,
                          headerLogoBounds.getX(),
                          headerLogoBounds.getY(),
                          headerLogoBounds.getWidth(),
                          headerLogoBounds.getHeight(),
                          juce::RectanglePlacement::centred);
    }

    const auto micIcon = inputIconBounds.toFloat().reduced(0.8f);
    const auto mcx = micIcon.getCentreX();
    const auto mTop = micIcon.getY() + micIcon.getHeight() * 0.10f;
    const auto mBodyW = micIcon.getWidth() * 0.44f;
    const auto mBodyH = micIcon.getHeight() * 0.56f;
    g.setColour(kUiText);
    g.drawRoundedRectangle(mcx - mBodyW * 0.5f, mTop, mBodyW, mBodyH, mBodyW * 0.45f, 1.35f);
    juce::Path micArc;
    micArc.addCentredArc(mcx, mTop + mBodyH * 0.96f, mBodyW * 0.78f, mBodyH * 0.44f, 0.0f,
                         juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
    g.strokePath(micArc, juce::PathStrokeType(1.2f));
    g.drawLine(mcx, mTop + mBodyH + mBodyH * 0.30f, mcx, micIcon.getBottom() - 2.2f, 1.2f);
    g.drawLine(mcx - mBodyW * 0.42f, micIcon.getBottom() - 2.2f, mcx + mBodyW * 0.42f, micIcon.getBottom() - 2.2f, 1.2f);

    auto fizzleMicIcon = outputIconBounds.toFloat().reduced(0.4f);
    g.setColour(kUiAccent.withAlpha(0.18f));
    g.fillEllipse(fizzleMicIcon.expanded(1.0f));
    g.setColour(kUiAccentSoft.withAlpha(0.4f));
    g.drawEllipse(fizzleMicIcon.expanded(1.0f), 1.0f);
    if (appLogo.isValid())
    {
        g.drawImageWithin(appLogo,
                          juce::roundToInt(fizzleMicIcon.getX()),
                          juce::roundToInt(fizzleMicIcon.getY()),
                          juce::roundToInt(fizzleMicIcon.getWidth()),
                          juce::roundToInt(fizzleMicIcon.getHeight()),
                          juce::RectanglePlacement::centred);
    }
    else
    {
        g.setColour(kUiText);
        g.drawLine(fizzleMicIcon.getX() + 2.0f, fizzleMicIcon.getCentreY(), fizzleMicIcon.getRight() - 6.0f, fizzleMicIcon.getCentreY(), 1.2f);
        g.drawRoundedRectangle(fizzleMicIcon.getRight() - 5.8f, fizzleMicIcon.getCentreY() - 3.0f, 4.6f, 6.0f, 1.2f, 1.2f);
        g.fillEllipse(fizzleMicIcon.getX() + 1.0f, fizzleMicIcon.getCentreY() - 1.3f, 2.5f, 2.5f);
    }

}

void MainComponent::paintOverChildren(juce::Graphics& g)
{
    const bool settingsVisible = settingsPanel != nullptr && settingsPanel->isVisible() && settingsPanelAlpha > 0.02f;
    const bool lowCostPass = draggingWindow || draggingResizeGrip;
    const auto header = headerBounds.toFloat();
    const auto dividerY = header.getBottom() + 8.0f;
    const auto dividerX = header.getX() + 14.0f;
    const auto dividerW = header.getWidth() - 28.0f;
    if (! settingsVisible)
    {
        g.setColour(kUiAccent.withAlpha(0.040f));
        g.drawLine(dividerX, dividerY, dividerX + dividerW, dividerY, 1.2f);

        if (! lowCostPass)
        {
            const auto loopWidth = dividerW + 220.0f;
            const auto phase = std::fmod(uiPulse * 72.0f, loopWidth);
            const auto x0 = dividerX - 60.0f + phase;
            juce::ColourGradient sweep(juce::Colour(0xffffffff).withAlpha(0.0f), x0, dividerY,
                                       kUiAccentSoft.withAlpha(0.26f), x0 + 86.0f, dividerY, false);
            sweep.addColour(1.0, juce::Colour(0xffffffff).withAlpha(0.0f));
            g.setGradientFill(sweep);
            g.fillRect(juce::Rectangle<float>(dividerX, dividerY - 1.2f, dividerW, 2.4f));
        }
    }

    if (! settingsVisible && engine.isListenEnabled())
    {
        auto lb = routeSpeakersButton.getBounds().toFloat();
        auto led = juce::Rectangle<float>(lb.getRight() - 16.0f, lb.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(kUiMint);
        g.fillEllipse(led);
        g.setColour(kUiMint.withAlpha(0.35f));
        g.drawEllipse(led.expanded(2.0f), 1.3f);
    }

    if (params.mute.load())
    {
        auto badge = effectsToggle.getBounds().toFloat().withWidth(118.0f).translated(0.0f, -22.0f);
        g.setColour(kUiSalmon.withAlpha(0.24f));
        g.fillRoundedRectangle(badge, 9.0f);
        g.setColour(kUiSalmon.withAlpha(0.7f));
        g.drawRoundedRectangle(badge.reduced(0.5f), 8.4f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        juce::Font muteFont(juce::FontOptions(11.5f, juce::Font::bold));
        muteFont.setExtraKerningFactor(0.06f);
        g.setFont(muteFont);
        g.drawFittedText("MUTED", badge.toNearestInt(), juce::Justification::centred, 1);
    }

    auto drawFlash = [&g](const juce::Button& button, float alpha)
    {
        if (alpha <= 0.0f)
            return;
        auto bounds = button.getBounds().toFloat().expanded(1.8f, 1.2f);
        g.setColour(kUiAccent.withAlpha(0.11f * alpha));
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(kUiAccentSoft.withAlpha(0.22f * alpha));
        g.drawRoundedRectangle(bounds, 12.0f, 1.2f);
    };
    drawFlash(savePresetButton, savePresetFlashAlpha);
    drawFlash(aboutButton, aboutFlashAlpha);

    if (styleTransitionAlpha > 0.0f)
    {
        auto shell = getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        juce::Path shellPath;
        shellPath.addRoundedRectangle(shell, 24.0f);
        g.setColour(kUiAccentSoft.withAlpha(0.08f * styleTransitionAlpha));
        g.strokePath(shellPath, juce::PathStrokeType(2.0f));
    }

    if (! windowMaximized && ! resizeGripBounds.isEmpty())
    {
        auto grip = resizeGripBounds.toFloat();
        g.setColour(kUiPanel.withAlpha(0.34f));
        g.fillRoundedRectangle(grip, 8.0f);
        g.setColour(kUiAccent.withAlpha(0.28f));
        g.drawRoundedRectangle(grip.reduced(0.4f), 7.6f, 1.0f);
        g.setColour(kUiAccentSoft.withAlpha(0.72f));
        const auto gx = grip.getRight() - 5.0f;
        const auto gy = grip.getBottom() - 5.0f;
        g.drawLine(gx - 10.0f, gy, gx, gy - 10.0f, 1.2f);
        g.drawLine(gx - 6.0f, gy, gx, gy - 6.0f, 1.2f);
        g.drawLine(gx - 2.0f, gy, gx, gy - 2.0f, 1.2f);
    }

    if (logoFizzActive && ! logoFizzBubbles.empty())
    {
        auto shell = getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        juce::Path shellPath;
        shellPath.addRoundedRectangle(shell, 24.0f);
        g.saveState();
        g.reduceClipRegion(shellPath);
        const int bubbleStep = lowCostPass ? 2 : 1;
        for (int i = 0; i < static_cast<int>(logoFizzBubbles.size()); i += bubbleStep)
        {
            const auto& b = logoFizzBubbles[static_cast<size_t>(i)];
            if (b.alpha <= 0.0f)
                continue;
            auto bubble = juce::Rectangle<float>(b.pos.x - b.radius, b.pos.y - b.radius, b.radius * 2.0f, b.radius * 2.0f);
            if (lowCostPass)
            {
                g.setColour(kUiAccentSoft.withAlpha(0.34f * b.alpha));
                g.fillEllipse(bubble);
            }
            else
            {
                juce::ColourGradient fill(kUiAccentSoft.withAlpha(0.50f * b.alpha), bubble.getCentreX(), bubble.getY(),
                                          kUiAccent.withAlpha(0.20f * b.alpha), bubble.getCentreX(), bubble.getBottom(), false);
                g.setGradientFill(fill);
                g.fillEllipse(bubble);
            }
            g.setColour(juce::Colours::white.withAlpha(0.45f * b.alpha));
            g.drawEllipse(bubble, 1.1f);
            if (! lowCostPass)
            {
                g.setColour(juce::Colours::white.withAlpha(0.24f * b.alpha));
                g.fillEllipse(bubble.getX() + b.radius * 0.45f, bubble.getY() + b.radius * 0.18f, b.radius * 0.42f, b.radius * 0.42f);
            }
        }
        g.restoreState();
    }

    if (restartOverlayAlpha > 0.01f || restartOverlayBusy)
    {
        auto shell = getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        juce::Path shellPath;
        shellPath.addRoundedRectangle(shell, 24.0f);
        g.saveState();
        g.reduceClipRegion(shellPath);
        g.setColour(juce::Colours::black.withAlpha(0.32f * restartOverlayAlpha));
        g.fillPath(shellPath);

        const auto spinnerCenter = juce::Point<float>(shell.getCentreX(), shell.getCentreY());
        g.setColour(kUiPanelSoft.withAlpha(0.55f * restartOverlayAlpha));
        g.fillEllipse(spinnerCenter.x - 34.0f, spinnerCenter.y - 34.0f, 68.0f, 68.0f);
        g.setColour(kUiAccent.withAlpha(0.45f * restartOverlayAlpha));
        g.drawEllipse(spinnerCenter.x - 34.0f, spinnerCenter.y - 34.0f, 68.0f, 68.0f, 1.6f);

        const auto startAngle = uiPulse * 3.6f;
        juce::Path arc;
        arc.addCentredArc(spinnerCenter.x, spinnerCenter.y, 18.0f, 18.0f, 0.0f, startAngle, startAngle + 4.9f, true);
        g.setColour(kUiAccentSoft.withAlpha(0.95f * restartOverlayAlpha));
        g.strokePath(arc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(juce::roundToInt(18.0f * uiScale), juce::roundToInt(12.0f * uiScale));
    const int gap = juce::jmax(6, juce::roundToInt(10.0f * uiScale));
    const int compactFactor = cachedSettings.uiDensity == 0 ? 1 : 0;

    headerBounds = area.removeFromTop(juce::roundToInt(38.0f * uiScale));
    auto headerInner = headerBounds.reduced(juce::roundToInt(8.0f * uiScale), juce::roundToInt(5.0f * uiScale));
    auto windowButtons = headerInner.removeFromRight(juce::roundToInt(96.0f * uiScale));
    const int winButtonW = juce::roundToInt(26.0f * uiScale);
    windowCloseButton.setBounds(windowButtons.removeFromRight(winButtonW));
    windowButtons.removeFromRight(juce::roundToInt(6.0f * uiScale));
    windowMaxButton.setBounds(windowButtons.removeFromRight(winButtonW));
    windowButtons.removeFromRight(juce::roundToInt(6.0f * uiScale));
    windowMinButton.setBounds(windowButtons.removeFromRight(winButtonW));

    const int logoSize = juce::roundToInt(20.0f * uiScale);
    headerLogoBounds = headerInner.removeFromLeft(juce::roundToInt(24.0f * uiScale)).withSizeKeepingCentre(logoSize, logoSize);
    headerInner.removeFromLeft(juce::roundToInt(7.0f * uiScale));
    title.setBounds(headerInner.removeFromLeft(juce::roundToInt(250.0f * uiScale)));

    area.removeFromTop(gap + juce::roundToInt(6.0f * uiScale));

    const int topWidth = area.getWidth();
    const int inputW = juce::jmax(160, static_cast<int>(topWidth * 0.24f));
    const int outputW = juce::jmax(190, static_cast<int>(topWidth * 0.25f));
    const int bufferW = juce::jmax(84, static_cast<int>(topWidth * 0.10f));
    const int sampleW = juce::jmax(84, static_cast<int>(topWidth * 0.10f));
    const int presetW = juce::jmax(140, topWidth - inputW - outputW - bufferW - sampleW - (gap * 4));

    auto labels = area.removeFromTop(juce::roundToInt(20.0f * uiScale));
    auto inputLabelArea = labels.removeFromLeft(inputW);
    inputLabel.setBounds(inputLabelArea.withTrimmedLeft(juce::roundToInt(40.0f * uiScale)));
    inputIconBounds = juce::Rectangle<int>(inputLabelArea.getX() + juce::roundToInt(8.0f * uiScale),
                                           inputLabelArea.getY() + juce::roundToInt(1.0f * uiScale),
                                           juce::roundToInt(18.0f * uiScale),
                                           juce::roundToInt(18.0f * uiScale));
    labels.removeFromLeft(gap);
    auto outputLabelArea = labels.removeFromLeft(outputW);
    outputLabel.setBounds(outputLabelArea.withTrimmedLeft(juce::roundToInt(40.0f * uiScale)));
    outputIconBounds = juce::Rectangle<int>(outputLabelArea.getX() + juce::roundToInt(8.0f * uiScale),
                                            outputLabelArea.getY() + juce::roundToInt(1.0f * uiScale),
                                            juce::roundToInt(18.0f * uiScale),
                                            juce::roundToInt(18.0f * uiScale));
    labels.removeFromLeft(gap);
    bufferLabel.setBounds(labels.removeFromLeft(bufferW)); labels.removeFromLeft(gap);
    sampleRateLabel.setBounds(labels.removeFromLeft(sampleW)); labels.removeFromLeft(gap);
    presetLabel.setBounds(labels.removeFromLeft(presetW));

    const int rowH = juce::roundToInt(34.0f * uiScale);
    auto devices = area.removeFromTop(rowH);
    inputBox.setBounds(devices.removeFromLeft(inputW)); devices.removeFromLeft(gap);
    outputBox.setBounds(devices.removeFromLeft(outputW)); devices.removeFromLeft(gap);
    bufferBox.setBounds(devices.removeFromLeft(bufferW)); devices.removeFromLeft(gap);
    sampleRateValueLabel.setBounds(devices.removeFromLeft(sampleW)); devices.removeFromLeft(gap);
    presetBox.setBounds(devices.removeFromLeft(presetW));

    area.removeFromTop(gap);
    const int actionRowH = juce::roundToInt(36.0f * uiScale);
    int saveW = juce::roundToInt(132.0f * uiScale);
    int deleteW = juce::roundToInt(108.0f * uiScale);
    int restartW = juce::roundToInt(126.0f * uiScale);
    int aboutW = juce::roundToInt(96.0f * uiScale);
    int toneW = juce::roundToInt(96.0f * uiScale);
    int effectsW = juce::roundToInt(110.0f * uiScale);
    int listenW = juce::roundToInt(106.0f * uiScale);
    int settingsW = juce::roundToInt(106.0f * uiScale);

    const int singleRowAvailable = area.getWidth() - (gap * 7);
    const int minRequestedW =
        juce::roundToInt(100.0f * uiScale) +
        juce::roundToInt(84.0f * uiScale) +
        juce::roundToInt(98.0f * uiScale) +
        juce::roundToInt(76.0f * uiScale) +
        juce::roundToInt(74.0f * uiScale) +
        juce::roundToInt(90.0f * uiScale) +
        juce::roundToInt(86.0f * uiScale) +
        juce::roundToInt(86.0f * uiScale);
    const bool splitActionRows = singleRowAvailable < minRequestedW;

    if (splitActionRows)
    {
        auto actionsBlock = area.removeFromTop(actionRowH * 2 + gap);
        auto firstRow = actionsBlock.removeFromTop(actionRowH);
        actionsBlock.removeFromTop(gap);
        auto secondRow = actionsBlock.removeFromTop(actionRowH);

        auto layoutFourButtons = [gap](juce::Rectangle<int> row, std::array<juce::Button*, 4> rowButtons)
        {
            const int cellW = juce::jmax(76, (row.getWidth() - gap * 3) / 4);
            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                auto* b = rowButtons[i];
                if (b == nullptr)
                    continue;
                const bool last = (i == rowButtons.size() - 1);
                const int w = last ? row.getWidth() : cellW;
                b->setBounds(row.removeFromLeft(w));
                if (! last)
                    row.removeFromLeft(gap);
            }
        };

        layoutFourButtons(firstRow, { &savePresetButton, &deletePresetButton, &restartButton, &aboutButton });
        layoutFourButtons(secondRow, { &toneButton, &effectsToggle, &routeSpeakersButton, &settingsButton });
    }
    else
    {
        auto actions = area.removeFromTop(actionRowH);
        const int availableW = actions.getWidth() - (gap * 7);
        int requestedW = saveW + deleteW + restartW + aboutW + toneW + effectsW + listenW + settingsW;
        if (requestedW > availableW && availableW > 0)
        {
            const float scale = static_cast<float>(availableW) / static_cast<float>(requestedW);
            auto scaleDown = [scale](int value, int minValue)
            {
                return juce::jmax(minValue, juce::roundToInt(static_cast<float>(value) * scale));
            };
            saveW = scaleDown(saveW, juce::roundToInt(100.0f * uiScale));
            deleteW = scaleDown(deleteW, juce::roundToInt(84.0f * uiScale));
            restartW = scaleDown(restartW, juce::roundToInt(98.0f * uiScale));
            aboutW = scaleDown(aboutW, juce::roundToInt(76.0f * uiScale));
            toneW = scaleDown(toneW, juce::roundToInt(74.0f * uiScale));
            effectsW = scaleDown(effectsW, juce::roundToInt(90.0f * uiScale));
            listenW = scaleDown(listenW, juce::roundToInt(86.0f * uiScale));
            settingsW = scaleDown(settingsW, juce::roundToInt(86.0f * uiScale));
        }

        savePresetButton.setBounds(actions.removeFromLeft(saveW)); actions.removeFromLeft(gap);
        deletePresetButton.setBounds(actions.removeFromLeft(deleteW)); actions.removeFromLeft(gap);
        restartButton.setBounds(actions.removeFromLeft(restartW)); actions.removeFromLeft(gap);
        aboutButton.setBounds(actions.removeFromLeft(aboutW)); actions.removeFromLeft(gap);
        toneButton.setBounds(actions.removeFromLeft(toneW)); actions.removeFromLeft(gap);
        effectsToggle.setBounds(actions.removeFromLeft(effectsW)); actions.removeFromLeft(gap);
        routeSpeakersButton.setBounds(actions.removeFromLeft(listenW)); actions.removeFromLeft(gap);
        settingsButton.setBounds(actions.removeFromLeft(settingsW));
    }

    area.removeFromTop(gap);
    auto gainRow = area.removeFromTop(juce::roundToInt(30.0f * uiScale));
    outputGainLabel.setBounds(gainRow.removeFromLeft(juce::roundToInt(120.0f * uiScale)));
    outputGain.setBounds(gainRow.removeFromLeft(juce::jmax(220, gainRow.getWidth() - juce::roundToInt(90.0f * uiScale))));
    gainRow.removeFromLeft(gap);
    outputGainResetButton.setBounds(gainRow.removeFromLeft(juce::roundToInt(84.0f * uiScale)));

    auto infoRow = area.removeFromTop(juce::roundToInt(22.0f * uiScale));
    currentPresetLabel.setBounds(infoRow.removeFromLeft(juce::jmax(220, infoRow.getWidth() / 2)));
    const int hintW = juce::jmax(180, infoRow.getWidth() / 3);
    effectsHintLabel.setBounds(infoRow.removeFromRight(hintW));
    virtualMicStatusLabel.setBounds(infoRow);

    area.removeFromTop(juce::roundToInt(4.0f * uiScale));
    if (compactFactor == 0)
    {
        auto meters = area.removeFromTop(juce::roundToInt(22.0f * uiScale));
        meterIn.setVisible(true);
        meterOut.setVisible(true);
        meterIn.setBounds(meters.removeFromLeft((meters.getWidth() - gap) / 2));
        meters.removeFromLeft(gap);
        meterOut.setBounds(meters);
    }
    else
    {
        meterIn.setVisible(false);
        meterOut.setVisible(false);
    }

    area.removeFromTop(gap);
    auto vstRow1 = area.removeFromTop(juce::roundToInt(32.0f * uiScale));
    auto leftVstW = juce::jmax(250, vstRow1.getWidth() - juce::roundToInt(300.0f * uiScale));
    vstAvailableBox.setBounds(vstRow1.removeFromLeft(leftVstW)); vstRow1.removeFromLeft(gap);
    autoScanVstButton.setBounds(vstRow1.removeFromLeft(juce::roundToInt(140.0f * uiScale))); vstRow1.removeFromLeft(gap);
    rescanVstButton.setBounds(vstRow1.removeFromLeft(juce::roundToInt(150.0f * uiScale)));

    area.removeFromTop(juce::roundToInt(4.0f * uiScale));
    auto vstRow2 = area.removeFromTop(juce::roundToInt(32.0f * uiScale));
    removeVstButton.setBounds(vstRow2.removeFromLeft(juce::roundToInt(120.0f * uiScale))); vstRow2.removeFromLeft(gap);
    dragHintLabel.setBounds(vstRow2.removeFromLeft(juce::roundToInt(220.0f * uiScale)));

    area.removeFromTop(gap);
    const int rowsVisible = juce::jlimit(1, 3, juce::jmax(1, getNumRows()));
    const int desiredListHeight = juce::roundToInt(rowsVisible * static_cast<float>(vstChainList.getRowHeight())
                                                   + (10.0f * uiScale));
    const int minDiagHeight = juce::roundToInt((compactFactor == 0 ? 190.0f : 160.0f) * uiScale);
    const int maxListByRemaining = juce::jmax(100, area.getHeight() - minDiagHeight - gap);
    const int listHeight = juce::jlimit(100, maxListByRemaining, desiredListHeight);
    vstChainList.setBounds(area.removeFromTop(listHeight));

    area.removeFromTop(gap);
    diagnostics.setBounds(area);
    const int gripSize = juce::roundToInt(30.0f * uiScale);
    const int gripInset = juce::roundToInt(7.0f * uiScale);
    auto versionStrip = diagnostics.getBounds().removeFromBottom(juce::roundToInt(18.0f * uiScale));
    if (! windowMaximized)
        versionStrip = versionStrip.withTrimmedRight(gripSize + gripInset + juce::roundToInt(8.0f * uiScale));
    auto versionBounds = versionStrip.removeFromRight(juce::roundToInt(92.0f * uiScale));
    versionLabel.setBounds(versionBounds.translated(-3, -2));
    versionLabel.toFront(false);
    resizeGripBounds = getLocalBounds()
                           .withTrimmedLeft(getWidth() - (gripSize + gripInset))
                           .withTrimmedTop(getHeight() - (gripSize + gripInset))
                           .removeFromRight(gripSize)
                           .removeFromBottom(gripSize);

    if (settingsPanel != nullptr)
    {
        settingsPanel->setBounds(getLocalBounds());
        auto panelBounds = getLocalBounds();
        const int marginX = juce::jmin(juce::roundToInt(72.0f * uiScale), juce::jmax(18, panelBounds.getWidth() / 6));
        const int marginY = juce::jmin(juce::roundToInt(52.0f * uiScale), juce::jmax(14, panelBounds.getHeight() / 7));
        auto panel = panelBounds.reduced(marginX, marginY);

        const int minPanelW = juce::jmin(panelBounds.getWidth() - 24, juce::roundToInt(640.0f * uiScale));
        const int minPanelH = juce::jmin(panelBounds.getHeight() - 24, juce::roundToInt(430.0f * uiScale));
        if (panel.getWidth() < minPanelW || panel.getHeight() < minPanelH)
            panel = panel.withSizeKeepingCentre(juce::jmax(panel.getWidth(), minPanelW),
                                                juce::jmax(panel.getHeight(), minPanelH));

        if (auto* overlay = dynamic_cast<SettingsOverlay*>(settingsPanel.get()))
            overlay->setPanelBounds(panel);

        const int sidebarW = juce::jlimit(154, juce::jmax(220, panel.getWidth() / 3), juce::roundToInt(204.0f * uiScale));
        auto sidebar = panel.removeFromLeft(sidebarW).reduced(juce::roundToInt(12.0f * uiScale));
        if (auto* overlay = dynamic_cast<SettingsOverlay*>(settingsPanel.get()))
            overlay->setSidebarWidth(static_cast<float>(sidebarW));
        int navRowH = juce::roundToInt(32.0f * uiScale);
        int navGap = juce::jmax(4, juce::roundToInt(6.0f * uiScale));
        settingsNavLabel.setBounds(sidebar.removeFromTop(juce::roundToInt(24.0f * uiScale)));
        sidebar.removeFromTop(juce::roundToInt(8.0f * uiScale));
        const int navItems = 4;
        const int availableNavH = juce::jmax(0, sidebar.getHeight());
        int requiredNavH = navItems * navRowH + (navItems - 1) * navGap;
        if (requiredNavH > availableNavH && availableNavH > 0)
        {
            navRowH = juce::jmax(juce::roundToInt(20.0f * uiScale),
                                 (availableNavH - (navItems - 1) * navGap) / navItems);
            requiredNavH = navItems * navRowH + (navItems - 1) * navGap;
            if (requiredNavH > availableNavH)
                navGap = juce::jmax(2, (availableNavH - navItems * navRowH) / (navItems - 1));
        }
        settingsNavAutoEnableButton.setBounds(sidebar.removeFromTop(navRowH));
        sidebar.removeFromTop(navGap);
        settingsNavAppearanceButton.setBounds(sidebar.removeFromTop(navRowH));
        sidebar.removeFromTop(navGap);
        settingsNavUpdatesButton.setBounds(sidebar.removeFromTop(navRowH));
        sidebar.removeFromTop(navGap);
        settingsNavStartupButton.setBounds(sidebar.removeFromTop(navRowH));

        auto content = panel.reduced(juce::roundToInt(14.0f * uiScale));
        auto contentNoFooter = content;
        auto footer = contentNoFooter.removeFromBottom(juce::roundToInt(34.0f * uiScale));
        closeSettingsButton.setBounds(footer.removeFromLeft(juce::roundToInt(92.0f * uiScale)));

        const auto h20 = juce::roundToInt(20.0f * uiScale);
        const auto h22 = juce::roundToInt(22.0f * uiScale);
        const auto h28 = juce::roundToInt(28.0f * uiScale);
        const auto h30 = juce::roundToInt(30.0f * uiScale);
        const auto h36 = juce::roundToInt(36.0f * uiScale);
        const auto h56 = juce::roundToInt(56.0f * uiScale);
        const auto h94 = juce::roundToInt(94.0f * uiScale);
        const auto h96 = juce::roundToInt(96.0f * uiScale);
        const auto h120 = juce::roundToInt(120.0f * uiScale);
        const auto tabRequiredHeight = [this, gap, h20, h22, h28, h30, h36, h56, h94, h96, h120]()
        {
            switch (activeSettingsTab)
            {
                case 0:
                    return h22 + h28 + gap + h20 + h28 + gap + h20 + h120 + gap
                         + h20 + h94 + gap + h28 + gap + h28 + gap + h30;
                case 1:
                    return h22 + gap + h20 + h28 + gap + h20 + h30 + gap
                         + h20 + h30 + gap + h20 + h30;
                case 2:
                    return h22 + h28 + gap + h30 + gap + h56 + gap + h20 + h30;
                case 3:
                    return h22 + gap + h20 + h30 + gap + h20 + h96 + gap + h30 + gap
                         + h28 + gap + h28 + gap + h28 + gap + h36;
                default:
                    break;
            }
            return 0;
        }();
        settingsScrollMax = juce::jmax(0, tabRequiredHeight - contentNoFooter.getHeight());
        settingsScrollY = juce::jlimit(0, settingsScrollMax, settingsScrollY);
        if (settingsScrollMax > 0)
            contentNoFooter = contentNoFooter.translated(0, -settingsScrollY);

        if (activeSettingsTab == 0)
        {
            appEnableLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(22.0f * uiScale)));
            autoEnableToggle.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appSearchLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            appSearchEditor.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appListLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            appListBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(120.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            enabledProgramsLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            enabledProgramsListBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(94.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            auto buttonsRow = contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale));
            refreshAppsButton.setBounds(buttonsRow.removeFromLeft(juce::roundToInt(120.0f * uiScale)));
            buttonsRow.removeFromLeft(gap);
            removeProgramButton.setBounds(buttonsRow.removeFromLeft(juce::roundToInt(138.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appPathEditor.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            auto pathRow = contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale));
            browseAppPathButton.setBounds(pathRow.removeFromLeft(juce::roundToInt(120.0f * uiScale)));
        }
        else if (activeSettingsTab == 1)
        {
            appearanceLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(22.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appearanceModeLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            lightModeToggle.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appearanceThemeLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            appearanceThemeBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appearanceBackgroundLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            appearanceBackgroundBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            appearanceSizeLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            appearanceSizeBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale)));
        }
        else if (activeSettingsTab == 2)
        {
            updatesLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(22.0f * uiScale)));
            autoDownloadUpdatesToggle.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            auto updateRow = contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale));
            checkUpdatesButton.setBounds(updateRow.removeFromLeft(juce::roundToInt(120.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            updateStatusLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(56.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            updatesLinksLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            auto linksRow = contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale));
            updatesGithubButton.setBounds(linksRow.removeFromLeft(juce::roundToInt(140.0f * uiScale)));
            linksRow.removeFromLeft(gap);
            updatesWebsiteButton.setBounds(linksRow.removeFromLeft(juce::roundToInt(110.0f * uiScale)));
        }
        else if (activeSettingsTab == 3)
        {
            startupLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(22.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            behaviorListenDeviceLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            behaviorListenDeviceBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            behaviorVstFoldersLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(20.0f * uiScale)));
            behaviorVstFoldersListBox.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(96.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            auto folderRow = contentNoFooter.removeFromTop(juce::roundToInt(30.0f * uiScale));
            behaviorAddVstFolderButton.setBounds(folderRow.removeFromLeft(juce::roundToInt(132.0f * uiScale)));
            folderRow.removeFromLeft(gap);
            behaviorRemoveVstFolderButton.setBounds(folderRow.removeFromLeft(juce::roundToInt(136.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            startWithWindowsToggle.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            startMinimizedToggle.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            followAutoEnableWindowToggle.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(28.0f * uiScale)));
            contentNoFooter.removeFromTop(gap);
            startupHintLabel.setBounds(contentNoFooter.removeFromTop(juce::roundToInt(36.0f * uiScale)));
        }
    }
}
}
