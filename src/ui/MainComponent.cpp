#include "MainComponent.h"
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
const juce::Colour kUiBg(0xff0a111d);
const juce::Colour kUiPanel(0xff172338);
const juce::Colour kUiPanelSoft(0xff1e2d46);
const juce::Colour kUiText(0xffeaf1ff);
const juce::Colour kUiTextMuted(0xffa8bbdd);
const juce::Colour kUiAccent(0xff6dbbff);
const juce::Colour kUiAccentSoft(0xffa4ddff);
const juce::Colour kUiMint(0xff9af7d8);
const juce::Colour kUiSalmon(0xffff8d92);
const juce::String kGithubRepoUrl("https://github.com/softlynn/Fizzle");
const juce::String kWebsiteUrl("https://softlynn.github.io/Fizzle/");

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
        setColour(juce::ComboBox::backgroundColourId, kUiPanelSoft);
        setColour(juce::ComboBox::outlineColourId, kUiAccent.withAlpha(0.26f));
        setColour(juce::ComboBox::textColourId, kUiText);
        setColour(juce::TextButton::buttonColourId, kUiPanel);
        setColour(juce::TextButton::textColourOffId, kUiText);
        setColour(juce::Label::textColourId, kUiText);
        setColour(juce::ToggleButton::textColourId, kUiText);
        setColour(juce::Slider::trackColourId, kUiAccent.withAlpha(0.68f));
        setColour(juce::Slider::thumbColourId, kUiAccentSoft);
        setColour(juce::TextEditor::backgroundColourId, kUiPanel.withAlpha(0.7f));
        setColour(juce::TextEditor::outlineColourId, kUiAccent.withAlpha(0.22f));
        setColour(juce::TextEditor::textColourId, kUiText);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool isMouseOver,
                              bool isButtonDown) override
    {
        auto c = kUiPanel.withAlpha(0.94f);
        if (button.getToggleState())
            c = kUiAccent.withAlpha(0.32f);
        if (isMouseOver) c = c.brighter(0.08f);
        if (isButtonDown) c = c.brighter(0.14f);
        auto b = button.getLocalBounds().toFloat();
        juce::ColourGradient fill(c.brighter(0.03f), b.getX(), b.getY(),
                                  c.darker(0.1f), b.getX(), b.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(b, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(b.reduced(0.7f), 10.3f, 0.9f);
        g.setColour(kUiAccent.withAlpha(button.getToggleState() ? 0.30f : 0.14f));
        g.drawRoundedRectangle(b.reduced(0.3f), 10.8f, 1.1f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        auto bounds = button.getLocalBounds();
        auto r = bounds.reduced(12, 0);
        auto icon = juce::Rectangle<float>(static_cast<float>(r.getX()),
                                           static_cast<float>(bounds.getCentreY() - 6),
                                           12.0f,
                                           12.0f);
        r.removeFromLeft(18);

        constexpr float stroke = 1.45f;

        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        juce::Font f(juce::FontOptions(13.0f, juce::Font::bold));
        f.setExtraKerningFactor(0.018f);
        g.setFont(f);
        g.drawFittedText(button.getButtonText(), r, juce::Justification::centredLeft, 1);

        g.setColour(kUiAccentSoft);
        const auto id = button.getComponentID();
        const auto x = icon.getX();
        const auto y = icon.getY();
        const auto w = icon.getWidth();
        const auto h = icon.getHeight();
        const auto cx = icon.getCentreX();
        const auto cy = icon.getCentreY();

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
        juce::Font f(juce::FontOptions(14.0f, juce::Font::plain));
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
        auto track = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y + height / 2 - 3), static_cast<float>(width), 6.0f);
        g.setColour(kUiPanelSoft.withAlpha(0.9f));
        g.fillRoundedRectangle(track, 3.0f);

        auto active = track.withRight(sliderPos);
        juce::ColourGradient fill(kUiAccent, active.getX(), active.getY(),
                                  kUiMint, active.getRight(), active.getY(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(active, 3.0f);

        auto knob = juce::Rectangle<float>(sliderPos - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour(kUiAccentSoft);
        g.fillEllipse(knob);
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.drawEllipse(knob, 1.0f);
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
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.42f));
        auto panel = getLocalBounds().reduced(80, 56).toFloat();
        juce::ColourGradient fill(kUiPanelSoft.withAlpha(0.96f), panel.getX(), panel.getY(),
                                  kUiPanel.withAlpha(0.93f), panel.getX(), panel.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(panel, 18.0f);
        g.setColour(kUiAccent.withAlpha(0.58f));
        g.drawRoundedRectangle(panel, 18.0f, 1.4f);
        auto sidebar = panel.removeFromLeft(180.0f);
        g.setColour(kUiBg.withAlpha(0.78f));
        g.fillRoundedRectangle(sidebar, 16.0f);
        g.setColour(kUiAccent.withAlpha(0.22f));
        g.drawLine(sidebar.getRight(), panel.getY() + 8.0f, sidebar.getRight(), panel.getBottom() - 8.0f, 1.0f);
    }
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

    VstRowComponent(DragStart ds, DragHover dh, DragEnd de, OpenFn op, MixFn mx, ToggleFn tg)
        : dragStart(std::move(ds)), dragHover(std::move(dh)), dragEnd(std::move(de)), open(std::move(op)), setMix(std::move(mx)), toggle(std::move(tg))
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
        startTimerHz(60);
    }

    void setRowData(int rowIn, const juce::String& pluginName, bool isEnabled, float mixAmount, bool selectedIn)
    {
        row = rowIn;
        selected = selectedIn;
        name.setText(pluginName, juce::dontSendNotification);
        enabled.setToggleState(isEnabled, juce::dontSendNotification);
        mix.setValue(mixAmount, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6, 3);
        enabled.setBounds(r.removeFromLeft(48));
        auto knobArea = r.removeFromRight(128);
        auto knobBox = knobArea.removeFromRight(44);
        mix.setBounds(knobBox.withSizeKeepingCentre(40, 40));
        mixLabel.setBounds(knobArea.withTrimmedTop(10));
        openButton.setBounds(r.removeFromRight(64));
        name.setBounds(r);
    }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        auto fill = kUiPanelSoft.withAlpha(0.78f);
        auto border = kUiAccent.withAlpha(0.18f);
        if (selected)
        {
            fill = kUiPanel.withAlpha(0.94f);
            border = kUiAccent.withAlpha(0.85f);
        }
        if (dragging)
        {
            fill = fill.brighter(0.08f);
            border = kUiAccentSoft;
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRoundedRectangle(b.translated(0.0f, 2.0f), 8.0f);
        }
        g.setColour(fill);
        g.fillRoundedRectangle(b, 8.0f);
        g.setColour(border);
        g.drawRoundedRectangle(b, 8.0f, dragging ? 2.2f : (selected ? 2.0f : 1.0f));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (auto* lb = findParentComponentOfClass<juce::ListBox>())
            lb->selectRow(row);
        if (e.originalComponent == &mix || e.originalComponent == &enabled || e.originalComponent == &openButton)
            return;
        dragging = true;
        setInterceptsMouseClicks(true, true);
        toFront(true);
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
        if (! dragging)
            return;
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
        if (dragEnd)
            dragEnd(targetRow);
        repaint();
    }

private:
    int row { -1 };
    bool selected { false };
    bool dragging { false };
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
            setTransform(juce::AffineTransform::translation(0.0f, dragOffsetY)
                            .scaled(currentScale, currentScale, getWidth() * 0.5f, getHeight() * 0.5f));
            repaint();
        }
        else
        {
            setTransform(juce::AffineTransform());
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
    appLogo = juce::ImageFileFormat::loadFrom(BinaryData::app_png, BinaryData::app_pngSize);

    title.setText("Fizzle", juce::dontSendNotification);
    juce::Font titleFont(juce::FontOptions(26.0f, juce::Font::bold));
    titleFont.setExtraKerningFactor(0.018f);
    title.setFont(titleFont);
    addAndMakeVisible(title);
    title.setVisible(false);

    inputLabel.setText("Input Mic", juce::dontSendNotification);
    outputLabel.setText("Fizzle Mic Output", juce::dontSendNotification);
    bufferLabel.setText("Buffer Size", juce::dontSendNotification);
    sampleRateLabel.setText("Sample Rate", juce::dontSendNotification);
    sampleRateValueLabel.setText("48.0 kHz", juce::dontSendNotification);
    presetLabel.setText("Preset", juce::dontSendNotification);
    appEnableLabel.setText("Program Auto-Enable", juce::dontSendNotification);
    appSearchLabel.setText("Search Programs", juce::dontSendNotification);
    appListLabel.setText("Programs", juce::dontSendNotification);
    enabledProgramsLabel.setText("Enabled Programs", juce::dontSendNotification);
    updatesLabel.setText("Updates", juce::dontSendNotification);
    updatesLinksLabel.setText("Quick links", juce::dontSendNotification);
    updatesLinksLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    startupLabel.setText("Startup", juce::dontSendNotification);
    startupHintLabel.setText("Startup controls and app-follow behavior.", juce::dontSendNotification);
    startupHintLabel.setColour(juce::Label::textColourId, kUiTextMuted);
    startupHintLabel.setJustificationType(juce::Justification::topLeft);
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
    savePresetButton.setComponentID("save");
    deletePresetButton.setComponentID("remove");
    restartButton.setComponentID("restart");
    aboutButton.setComponentID("about");
    toneButton.setComponentID("tone");
    effectsToggle.setComponentID("effects");
    routeSpeakersButton.setComponentID("listen");
    removeVstButton.setComponentID("remove");
    settingsButton.setComponentID("settings");

    const std::array<juce::Button*, 11> buttons {
        &savePresetButton, &deletePresetButton, &rescanVstButton, &autoScanVstButton, &removeVstButton,
        &routeSpeakersButton, &restartButton, &aboutButton, &toneButton, &effectsToggle, &settingsButton
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
    settingsNavUpdatesButton.setClickingTogglesState(true);
    settingsNavStartupButton.setClickingTogglesState(true);
    settingsNavAutoEnableButton.setToggleState(true, juce::dontSendNotification);
    settingsNavUpdatesButton.setToggleState(false, juce::dontSendNotification);
    settingsNavStartupButton.setToggleState(false, juce::dontSendNotification);
    autoEnableToggle.setButtonText("Auto Enable by Program");
    autoDownloadUpdatesToggle.setButtonText("Auto-install new updates");
    startWithWindowsToggle.setButtonText("Start with Windows");
    startMinimizedToggle.setButtonText("Start minimized to tray");
    followAutoEnableWindowToggle.setButtonText("Open/close window with Program Auto-Enable");
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
    checkUpdatesButton.addListener(this);
    settingsNavAutoEnableButton.addListener(this);
    settingsNavUpdatesButton.addListener(this);
    settingsNavStartupButton.addListener(this);
    updatesGithubButton.addListener(this);
    updatesWebsiteButton.addListener(this);
    startWithWindowsToggle.addListener(this);
    startMinimizedToggle.addListener(this);
    followAutoEnableWindowToggle.addListener(this);

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

    settingsPanel = std::make_unique<SettingsOverlay>();
    settingsPanel->addAndMakeVisible(settingsNavLabel);
    settingsPanel->addAndMakeVisible(settingsNavAutoEnableButton);
    settingsPanel->addAndMakeVisible(settingsNavUpdatesButton);
    settingsPanel->addAndMakeVisible(settingsNavStartupButton);
    settingsPanel->addAndMakeVisible(appEnableLabel);
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
    outputGain.onValueChange = [this]
    {
        params.outputGainDb.store(static_cast<float>(outputGain.getValue()));
    };

    addAndMakeVisible(meterIn);
    addAndMakeVisible(meterOut);
    addAndMakeVisible(diagnostics);

    bufferBox.addItem("128", 1);
    bufferBox.addItem("256", 2);
    bufferBox.addItem("512", 3);
    bufferBox.setSelectedId(2, juce::dontSendNotification);

    vstAvailableBox.setTextWhenNothingSelected("Detected VST3 plugins");
    const auto fxEnabled = ! params.bypass.load();
    effectsToggle.setToggleState(fxEnabled, juce::dontSendNotification);
    effectsToggle.setButtonText(fxEnabled ? "Effects On" : "Effects Off");
    outputGain.setValue(params.outputGainDb.load(), juce::dontSendNotification);

    cachedSettings = settingsStore.loadEngineSettings();
    const auto persistedLastPreset = loadPersistedLastPresetName();
    if (persistedLastPreset.isNotEmpty())
        cachedSettings.lastPresetName = persistedLastPreset;
    engine.getVstHost().importScannedPaths(cachedSettings.scannedVstPaths);
    autoEnableToggle.setToggleState(cachedSettings.autoEnableByApp, juce::dontSendNotification);
    autoDownloadUpdatesToggle.setToggleState(cachedSettings.autoInstallUpdates, juce::dontSendNotification);
    startWithWindowsToggle.setToggleState(cachedSettings.startWithWindows, juce::dontSendNotification);
    startMinimizedToggle.setToggleState(cachedSettings.startMinimizedToTray, juce::dontSendNotification);
    followAutoEnableWindowToggle.setToggleState(cachedSettings.followAutoEnableWindowState, juce::dontSendNotification);
    setUpdateStatus(cachedSettings.autoInstallUpdates ? "Auto-install is enabled." : "Auto-install is off.");
    setActiveSettingsTab(0);
    applyWindowsStartupSetting();
    refreshRunningApps();
    refreshEnabledProgramsList();
    if (cachedSettings.autoEnableProcessPath.isNotEmpty())
        appPathEditor.setText(cachedSettings.autoEnableProcessPath, juce::dontSendNotification);

    loadDeviceLists();
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

            const juce::String guide =
                "Welcome to Fizzle!\n\n"
                "Quick setup:\n"
                "1) Select your microphone in Input Mic.\n"
                "2) Select Fizzle Mic (VB-Cable) in Fizzle Mic Output.\n"
                "3) Turn Effects On.\n"
                "4) Pick a VST3 from the detected list to add it.\n"
                "5) Use Listen if you want local monitoring.\n\n"
                "Tip: Save a preset after your first good setup.";

            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                   "First Launch Guide",
                                                   guide);
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

    startTimerHz(30);
}

void MainComponent::onWindowVisible()
{
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
    for (const auto& folder : getDefaultVst3Folders())
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
    auto hwnd = GetForegroundWindow();
    if (hwnd == nullptr)
        return {};
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return {};

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return {};

    juce::String out;
    PROCESSENTRY32 pe {};
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == pid)
            {
                out = juce::String(pe.szExeFile);
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return out;
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
        refreshRunningApps();
        refreshProgramsList();
        updateSettingsTabVisibility();
        settingsPanel->setVisible(true);
        settingsPanel->toFront(true);
    }
    settingsPanel->setInterceptsMouseClicks(visible, visible);
}

void MainComponent::updateSettingsTabVisibility()
{
    const bool autoEnableTab = (activeSettingsTab == 0);
    const bool updatesTab = (activeSettingsTab == 1);
    const bool startupTab = (activeSettingsTab == 2);
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
    activeSettingsTab = juce::jlimit(0, 2, tab);
    settingsNavAutoEnableButton.setToggleState(activeSettingsTab == 0, juce::dontSendNotification);
    settingsNavUpdatesButton.setToggleState(activeSettingsTab == 1, juce::dontSendNotification);
    settingsNavStartupButton.setToggleState(activeSettingsTab == 2, juce::dontSendNotification);
    updateSettingsTabVisibility();
    resized();
}

void MainComponent::triggerButtonFlash(juce::Button* button)
{
    if (button == &savePresetButton)
        savePresetFlashAlpha = 1.0f;
    else if (button == &aboutButton)
        aboutFlashAlpha = 1.0f;
}

void MainComponent::applyWindowsStartupSetting()
{
#if JUCE_WINDOWS
    if (! setRunAtStartupEnabled(cachedSettings.startWithWindows, cachedSettings.startMinimizedToTray))
        Logger::instance().log("Failed to update Windows startup registration.");
#endif
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
    if (current.inputDeviceName.isNotEmpty())
        inputBox.setSelectedId(inputNames.indexOf(current.inputDeviceName) + 1, juce::dontSendNotification);
    if (current.outputDeviceName.isNotEmpty())
    {
        const auto outputIndex = outputDeviceRealNames.indexOf(current.outputDeviceName);
        if (outputIndex >= 0)
            outputBox.setSelectedId(outputIndex + 1, juce::dontSendNotification);
    }
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
    s.bufferSize = requestedBuffer > 0 ? requestedBuffer : 256;

    if (s.inputDeviceName.isEmpty() || s.outputDeviceName.isEmpty())
        return;

    if (s.inputDeviceName == previous.inputDeviceName
        && s.outputDeviceName == previous.outputDeviceName
        && s.bufferSize == previous.bufferSize)
        return;

    juce::String error;
    closePluginEditorWindow();
    engine.stop();
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
    effectsHintTicks = juce::jmax(0, ticks);
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
        const auto params = juce::String("/VERYSILENT /SUPPRESSMSGBOXES /NOCANCEL /NORESTART /CLOSEAPPLICATIONS /FORCECLOSEAPPLICATIONS");
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
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis == nullptr)
                return;
            if (auto* app = juce::JUCEApplicationBase::getInstance())
                app->systemRequestedQuit();
        });
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

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, from, to]
    {
        if (safeThis == nullptr)
            return;
        safeThis->closePluginEditorWindow();
        safeThis->engine.getVstHost().movePlugin(from, to);
        safeThis->refreshPluginChainUi();
        safeThis->vstChainList.selectRow(to);
    });
}

void MainComponent::setRowMix(int row, float mix)
{
    engine.getVstHost().setMix(row, mix);
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
        if (engineSettings.bufferSize == 128) bufferBox.setSelectedId(1, juce::dontSendNotification);
        else if (engineSettings.bufferSize == 512) bufferBox.setSelectedId(3, juce::dontSendNotification);
        else bufferBox.setSelectedId(2, juce::dontSendNotification);

        juce::String startError;
        engine.stop();
        engine.start(engineSettings, startError);
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
        const auto blockSize = d.bufferSize > 0 ? d.bufferSize : 256;
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
    if (button == &restartButton)
    {
        juce::String error;
        engine.restartAudio(error);
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Restart failed", error);
    }
    else if (button == &aboutButton)
    {
        triggerButtonFlash(button);
        auto* aboutPrompt = new juce::AlertWindow("About Fizzle",
                                                  "Fizzle " + juce::String(FIZZLE_VERSION)
                                                    + "\n\nGitHub: " + kGithubRepoUrl
                                                    + "\nWebsite: " + kWebsiteUrl,
                                                  juce::AlertWindow::InfoIcon);
        aboutPrompt->addButton("Open GitHub", 1);
        aboutPrompt->addButton("Open Website", 2);
        aboutPrompt->addButton("Close", 0);
        aboutPrompt->enterModalState(true, juce::ModalCallbackFunction::create([](int result)
        {
            if (result == 1)
                juce::URL(kGithubRepoUrl).launchInDefaultBrowser();
            else if (result == 2)
                juce::URL(kWebsiteUrl).launchInDefaultBrowser();
        }), true);
    }
    else if (button == &routeSpeakersButton)
    {
        if (! engine.isListenEnabled())
        {
            const auto monitor = findOutputByMatch({ "speaker", "headphones", "airpods", "realtek", "output" });
            if (monitor.isEmpty())
                return;
            if (monitor == getSelectedOutputDeviceName())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                       "Listen Output",
                                                       "Listen output must be different from the Virtual Mic output.");
                return;
            }
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
            closePluginEditorWindow();
            engine.getVstHost().removePlugin(selected);
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
        const auto enabled = effectsToggle.getToggleState();
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
    else if (button == &settingsButton)
    {
        setSettingsPanelVisible(! settingsPanelTargetVisible);
    }
    else if (button == &settingsNavAutoEnableButton)
    {
        setActiveSettingsTab(0);
    }
    else if (button == &settingsNavUpdatesButton)
    {
        setActiveSettingsTab(1);
    }
    else if (button == &settingsNavStartupButton)
    {
        setActiveSettingsTab(2);
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
            const auto blockSize = d.bufferSize > 0 ? d.bufferSize : 256;
            engine.getVstHost().addPlugin(known.getReference(selected), sampleRate, blockSize, error);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin Load Failed", error);
            refreshPluginChainUi();
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
    uiPulse += 0.03f;
    ++uiTickCount;
    auto d = engine.getDiagnostics();
    meterIn.setLevel(d.inputLevel);
    meterOut.setLevel(d.outputLevel);
    sampleRateValueLabel.setText(juce::String(d.sampleRate / 1000.0, 1) + " kHz", juce::dontSendNotification);
    const auto out = getSelectedOutputDeviceName();
    const bool virt = isVirtualMicName(out);
    virtualMicStatusLabel.setText(virt ? "Fizzle Mic: routed" : "Fizzle Mic: not routed", juce::dontSendNotification);

    if (cachedSettings.autoEnableByApp && (uiTickCount % 30 == 0))
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

    if (effectsHintTicks > 0)
    {
        --effectsHintTicks;
        if (effectsHintTicks == 0)
            effectsHintLabel.setText({}, juce::dontSendNotification);
    }

    if (settingsPanel != nullptr)
    {
        const auto target = settingsPanelTargetVisible ? 1.0f : 0.0f;
        settingsPanelAlpha += (target - settingsPanelAlpha) * 0.55f;
        if (std::abs(settingsPanelAlpha - target) < 0.01f)
            settingsPanelAlpha = target;

        settingsPanel->setAlpha(settingsPanelAlpha);
        if (! settingsPanelTargetVisible && settingsPanelAlpha <= 0.01f)
            settingsPanel->setVisible(false);
        else if (settingsPanelTargetVisible && ! settingsPanel->isVisible())
            settingsPanel->setVisible(true);
    }

    savePresetFlashAlpha *= 0.84f;
    aboutFlashAlpha *= 0.84f;
    if (savePresetFlashAlpha < 0.01f)
        savePresetFlashAlpha = 0.0f;
    if (aboutFlashAlpha < 0.01f)
        aboutFlashAlpha = 0.0f;

    repaint();
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
                    engine.getVstHost().setEnabled(r, ! p->enabled);
                refreshPluginChainUi();
            });
    }

    if (auto* plugin = engine.getVstHost().getPlugin(rowNumber))
        row->setRowData(rowNumber, plugin->description.name, plugin->enabled, plugin->mix, isRowSelected);

    return row;
}

void MainComponent::paint(juce::Graphics& g)
{
    auto shell = getLocalBounds().toFloat().reduced(4.0f, 0.0f);
    constexpr float shellRadius = 24.0f;
    auto makePanelPath = [](juce::Rectangle<float> r, float radius)
    {
        juce::Path p;
        p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                              radius, radius,
                              false, false, true, true);
        return p;
    };

    auto shellPath = makePanelPath(shell, shellRadius);
    g.reduceClipRegion(shellPath);

    const auto pulse = 0.5f + 0.5f * std::sin(uiPulse * 0.6f);
    juce::ColourGradient bg(kUiBg.brighter(0.08f), shell.getX(), shell.getY(),
                            kUiBg.darker(0.22f), shell.getX(), shell.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRect(getLocalBounds());

    auto outer = shell.expanded(1.5f, 1.0f);
    g.setColour(kUiAccent.withAlpha(0.035f + pulse * 0.015f));
    g.strokePath(makePanelPath(outer, shellRadius + 1.5f), juce::PathStrokeType(1.2f));

    auto card = shell.reduced(2.4f, 0.7f);
    juce::ColourGradient cardFill(kUiPanelSoft.withAlpha(0.9f), card.getX(), card.getY(),
                                  kUiPanel.withAlpha(0.95f), card.getX(), card.getBottom(), false);
    g.setGradientFill(cardFill);
    g.fillPath(makePanelPath(card, 21.0f));

    auto topBlend = card.withHeight(60.0f);
    juce::Path blendPath;
    blendPath.addRoundedRectangle(topBlend.getX(), topBlend.getY(), topBlend.getWidth(), topBlend.getHeight() + 16.0f,
                                  18.0f, 18.0f, true, true, false, false);
    juce::ColourGradient blendFill(kUiPanelSoft.withAlpha(0.28f), topBlend.getX(), topBlend.getY(),
                                   juce::Colours::transparentBlack, topBlend.getX(), topBlend.getBottom() + 16.0f, false);
    g.setGradientFill(blendFill);
    g.fillPath(blendPath);

    auto seamShade = juce::Rectangle<float>(card.getX() + 10.0f, card.getY() + 1.0f, card.getWidth() - 20.0f, 22.0f);
    juce::ColourGradient seam(juce::Colours::white.withAlpha(0.03f), seamShade.getX(), seamShade.getY(),
                              juce::Colours::transparentBlack, seamShade.getX(), seamShade.getBottom(), false);
    g.setGradientFill(seam);
    g.fillRoundedRectangle(seamShade, 8.0f);

    auto chainPane = vstChainList.getBounds().toFloat().expanded(2.0f, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.02f));
    g.fillRoundedRectangle(chainPane, 10.0f);
    g.setColour(kUiAccent.withAlpha(0.16f));
    g.drawRoundedRectangle(chainPane, 10.0f, 1.1f);
    auto diagPane = diagnostics.getBounds().toFloat().expanded(2.0f, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.02f));
    g.fillRoundedRectangle(diagPane, 10.0f);
    g.setColour(kUiAccent.withAlpha(0.14f));
    g.drawRoundedRectangle(diagPane, 10.0f, 1.1f);

    g.setColour(kUiAccent.withAlpha(0.055f + pulse * 0.016f));
    g.strokePath(makePanelPath(shell.reduced(0.9f, 0.0f), shellRadius - 0.8f), juce::PathStrokeType(0.95f));
    g.setColour(kUiAccentSoft.withAlpha(0.016f + pulse * 0.008f));
    g.strokePath(makePanelPath(shell.expanded(1.4f, 0.4f), shellRadius + 1.2f), juce::PathStrokeType(0.9f));

    const auto dividerY = card.getY() + 9.0f;
    const auto dividerX = card.getX() + 14.0f;
    const auto dividerW = card.getWidth() - 28.0f;
    g.setColour(kUiAccent.withAlpha(0.028f));
    g.drawLine(dividerX, dividerY, dividerX + dividerW, dividerY, 1.0f);
    const auto loopWidth = dividerW + 180.0f;
    const auto phase = std::fmod(uiPulse * 96.0f, loopWidth);
    const auto x0 = dividerX - 60.0f + phase;
    juce::ColourGradient sweep(juce::Colour(0xffffffff).withAlpha(0.0f), x0, dividerY,
                               kUiAccentSoft.withAlpha(0.25f), x0 + 72.0f, dividerY, false);
    sweep.addColour(1.0, juce::Colour(0xffffffff).withAlpha(0.0f));
    g.setGradientFill(sweep);
    g.fillRect(juce::Rectangle<float>(dividerX, dividerY - 1.0f, dividerW, 2.0f));

    const auto micIcon = inputIconBounds.toFloat();
    g.setColour(kUiText);
    g.drawRoundedRectangle(micIcon.getX() + 3.5f, micIcon.getY() + 2.0f, 7.0f, 8.0f, 2.8f, 1.2f);
    g.drawLine(micIcon.getCentreX(), micIcon.getY() + 10.2f, micIcon.getCentreX(), micIcon.getBottom() - 1.8f, 1.2f);
    g.drawLine(micIcon.getCentreX() - 2.4f, micIcon.getBottom() - 1.8f, micIcon.getCentreX() + 2.4f, micIcon.getBottom() - 1.8f, 1.2f);

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

    if (engine.isListenEnabled())
    {
        auto lb = routeSpeakersButton.getBounds().toFloat();
        auto led = juce::Rectangle<float>(lb.getRight() - 16.0f, lb.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(kUiMint);
        g.fillEllipse(led);
        g.setColour(kUiMint.withAlpha(0.35f));
        g.drawEllipse(led.expanded(2.0f), 1.3f);
    }

    auto drawFlash = [&g](const juce::Button& button, float alpha)
    {
        if (alpha <= 0.0f)
            return;
        auto bounds = button.getBounds().toFloat().expanded(1.8f, 1.2f);
        g.setColour(kUiSalmon.withAlpha(0.10f * alpha));
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(kUiSalmon.withAlpha(0.24f * alpha));
        g.drawRoundedRectangle(bounds, 12.0f, 1.2f);
    };
    drawFlash(savePresetButton, savePresetFlashAlpha);
    drawFlash(aboutButton, aboutFlashAlpha);

    if (settingsPanel != nullptr && settingsPanel->isVisible())
    {
        return;
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    const int gap = 8;

    area.removeFromTop(1);

    const int topWidth = area.getWidth();
    const int inputW = juce::jmax(180, static_cast<int>(topWidth * 0.24f));
    const int outputW = juce::jmax(200, static_cast<int>(topWidth * 0.25f));
    const int bufferW = juce::jmax(90, static_cast<int>(topWidth * 0.10f));
    const int sampleW = juce::jmax(90, static_cast<int>(topWidth * 0.10f));
    const int presetW = juce::jmax(150, topWidth - inputW - outputW - bufferW - sampleW - (gap * 4));

    auto labels = area.removeFromTop(20);
    auto inputLabelArea = labels.removeFromLeft(inputW);
    inputLabel.setBounds(inputLabelArea.withTrimmedLeft(34));
    inputIconBounds = juce::Rectangle<int>(inputLabelArea.getX() + 8, inputLabelArea.getY() + 2, 16, 16);
    labels.removeFromLeft(gap);
    auto outputLabelArea = labels.removeFromLeft(outputW);
    outputLabel.setBounds(outputLabelArea.withTrimmedLeft(34));
    outputIconBounds = juce::Rectangle<int>(outputLabelArea.getX() + 8, outputLabelArea.getY() + 2, 16, 16);
    labels.removeFromLeft(gap);
    bufferLabel.setBounds(labels.removeFromLeft(bufferW)); labels.removeFromLeft(gap);
    sampleRateLabel.setBounds(labels.removeFromLeft(sampleW)); labels.removeFromLeft(gap);
    presetLabel.setBounds(labels.removeFromLeft(presetW));

    auto devices = area.removeFromTop(34);
    inputBox.setBounds(devices.removeFromLeft(inputW)); devices.removeFromLeft(gap);
    outputBox.setBounds(devices.removeFromLeft(outputW)); devices.removeFromLeft(gap);
    bufferBox.setBounds(devices.removeFromLeft(bufferW)); devices.removeFromLeft(gap);
    sampleRateValueLabel.setBounds(devices.removeFromLeft(sampleW)); devices.removeFromLeft(gap);
    presetBox.setBounds(devices.removeFromLeft(presetW));

    area.removeFromTop(gap);
    auto actions = area.removeFromTop(36);
    const int saveW = 134;
    const int deleteW = 114;
    const int buttonW = juce::jmax(92, (actions.getWidth() - saveW - deleteW - (gap * 7)) / 6);
    savePresetButton.setBounds(actions.removeFromLeft(saveW)); actions.removeFromLeft(gap);
    deletePresetButton.setBounds(actions.removeFromLeft(deleteW)); actions.removeFromLeft(gap);
    restartButton.setBounds(actions.removeFromLeft(buttonW)); actions.removeFromLeft(gap);
    aboutButton.setBounds(actions.removeFromLeft(buttonW)); actions.removeFromLeft(gap);
    toneButton.setBounds(actions.removeFromLeft(buttonW)); actions.removeFromLeft(gap);
    effectsToggle.setBounds(actions.removeFromLeft(buttonW)); actions.removeFromLeft(gap);
    routeSpeakersButton.setBounds(actions.removeFromLeft(buttonW)); actions.removeFromLeft(gap);
    settingsButton.setBounds(actions.removeFromLeft(buttonW));

    area.removeFromTop(gap);
    auto gainRow = area.removeFromTop(30);
    outputGainLabel.setBounds(gainRow.removeFromLeft(120));
    outputGain.setBounds(gainRow.removeFromLeft(juce::jmax(220, gainRow.getWidth() - 90)));
    gainRow.removeFromLeft(gap);
    outputGainResetButton.setBounds(gainRow.removeFromLeft(84));

    auto infoRow = area.removeFromTop(22);
    currentPresetLabel.setBounds(infoRow.removeFromLeft(juce::jmax(220, infoRow.getWidth() / 2)));
    const int hintW = juce::jmax(180, infoRow.getWidth() / 3);
    effectsHintLabel.setBounds(infoRow.removeFromRight(hintW));
    virtualMicStatusLabel.setBounds(infoRow);

    area.removeFromTop(4);
    auto meters = area.removeFromTop(22);
    meterIn.setBounds(meters.removeFromLeft((meters.getWidth() - gap) / 2));
    meters.removeFromLeft(gap);
    meterOut.setBounds(meters);

    area.removeFromTop(gap);
    auto vstRow1 = area.removeFromTop(32);
    auto leftVstW = juce::jmax(250, vstRow1.getWidth() - 300);
    vstAvailableBox.setBounds(vstRow1.removeFromLeft(leftVstW)); vstRow1.removeFromLeft(gap);
    autoScanVstButton.setBounds(vstRow1.removeFromLeft(140)); vstRow1.removeFromLeft(gap);
    rescanVstButton.setBounds(vstRow1.removeFromLeft(150));

    area.removeFromTop(4);
    auto vstRow2 = area.removeFromTop(32);
    removeVstButton.setBounds(vstRow2.removeFromLeft(120)); vstRow2.removeFromLeft(gap);
    // Top-level Enable/Disable removed; per-row checkbox controls plugin enable state.
    dragHintLabel.setBounds(vstRow2.removeFromLeft(180));

    area.removeFromTop(gap);
    const int listHeight = juce::jmax(140, static_cast<int>(getHeight() * 0.26f));
    vstChainList.setBounds(area.removeFromTop(listHeight));

    area.removeFromTop(gap);
    diagnostics.setBounds(area);
    auto versionBounds = diagnostics.getBounds().removeFromBottom(18).removeFromRight(86);
    versionLabel.setBounds(versionBounds.translated(-4, -2));
    versionLabel.toFront(false);

    if (settingsPanel != nullptr)
    {
        settingsPanel->setBounds(getLocalBounds());
        auto panel = getLocalBounds().reduced(80, 56);
        auto sidebar = panel.removeFromLeft(180).reduced(12);
        settingsNavLabel.setBounds(sidebar.removeFromTop(24));
        sidebar.removeFromTop(8);
        settingsNavAutoEnableButton.setBounds(sidebar.removeFromTop(32));
        sidebar.removeFromTop(6);
        settingsNavUpdatesButton.setBounds(sidebar.removeFromTop(32));
        sidebar.removeFromTop(6);
        settingsNavStartupButton.setBounds(sidebar.removeFromTop(32));

        auto content = panel.reduced(14);
        auto contentNoFooter = content;
        auto footer = contentNoFooter.removeFromBottom(34);
        closeSettingsButton.setBounds(footer.removeFromLeft(92));

        if (activeSettingsTab == 0)
        {
            appEnableLabel.setBounds(contentNoFooter.removeFromTop(22));
            autoEnableToggle.setBounds(contentNoFooter.removeFromTop(28));
            contentNoFooter.removeFromTop(8);
            appSearchLabel.setBounds(contentNoFooter.removeFromTop(20));
            appSearchEditor.setBounds(contentNoFooter.removeFromTop(28));
            contentNoFooter.removeFromTop(8);
            appListLabel.setBounds(contentNoFooter.removeFromTop(20));
            appListBox.setBounds(contentNoFooter.removeFromTop(120));
            contentNoFooter.removeFromTop(8);
            enabledProgramsLabel.setBounds(contentNoFooter.removeFromTop(20));
            enabledProgramsListBox.setBounds(contentNoFooter.removeFromTop(94));
            contentNoFooter.removeFromTop(8);
            auto buttonsRow = contentNoFooter.removeFromTop(28);
            refreshAppsButton.setBounds(buttonsRow.removeFromLeft(120));
            buttonsRow.removeFromLeft(gap);
            removeProgramButton.setBounds(buttonsRow.removeFromLeft(138));
            contentNoFooter.removeFromTop(8);
            appPathEditor.setBounds(contentNoFooter.removeFromTop(28));
            contentNoFooter.removeFromTop(8);
            auto pathRow = contentNoFooter.removeFromTop(30);
            browseAppPathButton.setBounds(pathRow.removeFromLeft(120));
        }
        else
        {
            if (activeSettingsTab == 1)
            {
                updatesLabel.setBounds(contentNoFooter.removeFromTop(22));
                autoDownloadUpdatesToggle.setBounds(contentNoFooter.removeFromTop(28));
                contentNoFooter.removeFromTop(8);
                auto updateRow = contentNoFooter.removeFromTop(30);
                checkUpdatesButton.setBounds(updateRow.removeFromLeft(120));
                contentNoFooter.removeFromTop(8);
                updateStatusLabel.setBounds(contentNoFooter.removeFromTop(56));
                contentNoFooter.removeFromTop(10);
                updatesLinksLabel.setBounds(contentNoFooter.removeFromTop(20));
                auto linksRow = contentNoFooter.removeFromTop(30);
                updatesGithubButton.setBounds(linksRow.removeFromLeft(140));
                linksRow.removeFromLeft(gap);
                updatesWebsiteButton.setBounds(linksRow.removeFromLeft(110));
            }
            else if (activeSettingsTab == 2)
            {
                startupLabel.setBounds(contentNoFooter.removeFromTop(22));
                startWithWindowsToggle.setBounds(contentNoFooter.removeFromTop(28));
                contentNoFooter.removeFromTop(8);
                startMinimizedToggle.setBounds(contentNoFooter.removeFromTop(28));
                contentNoFooter.removeFromTop(8);
                followAutoEnableWindowToggle.setBounds(contentNoFooter.removeFromTop(28));
                contentNoFooter.removeFromTop(10);
                startupHintLabel.setBounds(contentNoFooter.removeFromTop(36));
            }
        }
    }
}
}
