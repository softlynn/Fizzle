#include "TrayController.h"
#include <BinaryData.h>

namespace fizzle
{
TrayController::TrayController(Listener& l) : listener(l)
{
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

    m.showMenuAsync(juce::PopupMenu::Options());
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
