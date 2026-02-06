#include <JuceHeader.h>

#include "AppConfig.h"
#include "audio/AudioEngine.h"
#include "core/Logger.h"
#include "core/SettingsStore.h"
#include "core/PresetStore.h"
#include "ui/MainWindow.h"
#include "ui/MainComponent.h"
#include "ui/TrayController.h"

namespace fizzle
{
class FizzleApplication final : public juce::JUCEApplication,
                                public TrayController::Listener
{
public:
    const juce::String getApplicationName() override { return "Fizzle"; }
    const juce::String getApplicationVersion() override { return FIZZLE_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override
    {
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory()
            .setAsCurrentWorkingDirectory();

        Logger::instance().initialise();

        settings = std::make_unique<SettingsStore>();
        presets = std::make_unique<PresetStore>(settings->getAppDirectory());

        engine.setEffectParameters(&effectParams);

        auto loaded = settings->loadEngineSettings();
        if (loaded.bufferSize == 0)
            loaded.bufferSize = kDefaultBlockSize;

        juce::String error;
        if (! engine.start(loaded, error))
            Logger::instance().log("Start error: " + error);

        auto mainComponent = std::make_unique<MainComponent>(engine, *settings, *presets, effectParams);
        window = std::make_unique<MainWindow>(std::move(mainComponent));
        tray = std::make_unique<TrayController>(*this);

        bool forceShow = commandLine.containsIgnoreCase("--show");
        bool forceTray = commandLine.containsIgnoreCase("--tray");
        const bool startupTray = loaded.startMinimizedToTray;
        for (const auto& arg : juce::JUCEApplication::getCommandLineParameterArray())
        {
            if (arg.equalsIgnoreCase("--show"))
            {
                forceShow = true;
            }
            else if (arg.equalsIgnoreCase("--tray"))
            {
                forceTray = true;
            }
        }

        Logger::instance().log("Command line: " + commandLine);
        Logger::instance().log("Force show: " + juce::String(forceShow ? 1 : 0));
        Logger::instance().log("Force tray: " + juce::String(forceTray ? 1 : 0));
        Logger::instance().log("Startup tray: " + juce::String(startupTray ? 1 : 0));

        if ((forceTray || startupTray) && ! forceShow)
            window->setVisible(false);
        else
        {
            trayOpenRequested();
            juce::Timer::callAfterDelay(250, [this] { trayOpenRequested(); });
            juce::Timer::callAfterDelay(900, [this] { trayOpenRequested(); });
        }
    }

    void shutdown() override
    {
        tray.reset();
        window.reset();
        engine.stop();
        presets.reset();
        settings.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override
    {
        trayOpenRequested();
    }

private:
    AudioEngine engine;
    EffectParameters effectParams;
    std::unique_ptr<SettingsStore> settings;
    std::unique_ptr<PresetStore> presets;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<TrayController> tray;

    void trayOpenRequested() override
    {
        if (window != nullptr)
        {
            window->setMinimised(false);
            window->setVisible(true);
            window->toFront(true);
            if (auto* main = window->getMainComponent())
                main->onWindowVisible();
        }
    }

    void trayToggleBypass() override
    {
        if (window != nullptr)
        {
            if (auto* main = window->getMainComponent())
            {
                main->trayToggleEffectsBypass();
                return;
            }
        }

        const auto next = ! effectParams.bypass.load();
        effectParams.bypass.store(next);
    }

    void trayToggleMute() override
    {
        if (window != nullptr)
        {
            if (auto* main = window->getMainComponent())
            {
                main->trayToggleMute();
                return;
            }
        }

        const auto next = ! effectParams.mute.load();
        effectParams.mute.store(next);
    }

    void trayRestartAudio() override
    {
        if (window != nullptr)
        {
            if (auto* main = window->getMainComponent())
            {
                main->trayRestartAudio();
                return;
            }
        }

        juce::String error;
        engine.restartAudio(error);
        if (error.isNotEmpty())
            Logger::instance().log("Restart error: " + error);
    }

    void trayExit() override
    {
        quit();
    }

    void trayPresetSelected(const juce::String& name) override
    {
        if (window != nullptr)
        {
            if (auto* main = window->getMainComponent())
            {
                main->trayLoadPreset(name);
                return;
            }
        }

        if (presets == nullptr)
            return;

        auto preset = presets->loadPreset(name);
        if (! preset.has_value())
            return;

        if (const auto it = preset->values.find("outputGainDb"); it != preset->values.end())
            effectParams.outputGainDb.store(it->second);
    }

    juce::StringArray trayPresets() const override
    {
        return presets != nullptr ? presets->listPresets() : juce::StringArray {};
    }

    bool trayEffectsEnabled() const override
    {
        if (window != nullptr)
            if (auto* main = window->getMainComponent())
                return main->areEffectsEnabled();
        return ! effectParams.bypass.load();
    }

    bool trayMuted() const override
    {
        if (window != nullptr)
            if (auto* main = window->getMainComponent())
                return main->isMuted();
        return effectParams.mute.load();
    }

    TrayController::Appearance trayAppearance() const override
    {
        TrayController::Appearance appearance;

        if (window != nullptr)
        {
            if (auto* main = window->getMainComponent())
            {
                appearance.lightMode = main->isLightModeEnabled();
                appearance.themeVariant = main->getThemeVariant();
                appearance.uiDensity = main->getUiDensity();
                return appearance;
            }
        }

        if (settings != nullptr)
        {
            const auto s = settings->loadEngineSettings();
            appearance.lightMode = s.lightMode;
            appearance.themeVariant = juce::jlimit(0, 1, s.themeVariant);
            appearance.uiDensity = juce::jlimit(0, 2, s.uiDensity);
        }

        return appearance;
    }
};
}

START_JUCE_APPLICATION(fizzle::FizzleApplication)
