#pragma once

#include <JuceHeader.h>
#include "../AppConfig.h"
#include "../audio/AudioEngine.h"
#include "../core/SettingsStore.h"
#include "../core/PresetStore.h"
#include "MeterComponent.h"
#include "DiagnosticsPanel.h"
#include <vector>
#include <atomic>

namespace fizzle
{
class MainComponent : public juce::Component,
                      private juce::Button::Listener,
                      private juce::ComboBox::Listener,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    MainComponent(AudioEngine& engineRef, SettingsStore& settingsRef, PresetStore& presetsRef, EffectParameters& paramsRef);

    void resized() override;
    void paint(juce::Graphics& g) override;

    void refreshPresets();
    void onWindowVisible();

private:
    class ProgramListModel;

    AudioEngine& engine;
    SettingsStore& settingsStore;
    PresetStore& presetStore;
    EffectParameters& params;

    juce::Label title;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label bufferLabel;
    juce::Label sampleRateLabel;
    juce::Label sampleRateValueLabel;
    juce::Label presetLabel;
    juce::Label currentPresetLabel;
    juce::Label virtualMicStatusLabel;
    juce::Label effectsHintLabel;
    juce::Label dragHintLabel;
    juce::ComboBox inputBox;
    juce::ComboBox outputBox;
    juce::ComboBox bufferBox;
    juce::ComboBox presetBox;
    juce::ComboBox vstAvailableBox;
    juce::ListBox vstChainList { "VST Chain", this };

    juce::TextButton savePresetButton;
    juce::TextButton deletePresetButton { "Delete Preset" };
    juce::TextButton rescanVstButton { "Scan VST3 Folder" };
    juce::TextButton autoScanVstButton { "Auto Detect VST3" };
    juce::TextButton removeVstButton;
    juce::TextButton toggleVstButton { "Enable/Disable" };
    juce::TextButton routeSpeakersButton;
    juce::ToggleButton effectsToggle;
    juce::TextButton restartButton;
    juce::TextButton aboutButton;
    juce::TextButton settingsButton { "Settings" };
    juce::ToggleButton toneButton;
    juce::ToggleButton autoEnableToggle { "Auto Enable by App" };
    juce::TextButton refreshAppsButton { "Refresh" };
    juce::Label appEnableLabel;
    juce::Label appSearchLabel;
    juce::Label appListLabel;
    juce::Label enabledProgramsLabel;
    juce::Label updatesLabel;
    juce::Label updateStatusLabel;
    juce::Label settingsNavLabel;
    juce::TextButton settingsNavAutoEnableButton { "App Auto-Enable" };
    juce::TextEditor appSearchEditor;
    juce::TextEditor appPathEditor;
    juce::TextButton browseAppPathButton { "Browse..." };
    juce::TextButton removeProgramButton { "Remove Selected" };
    juce::TextButton closeSettingsButton { "Close" };
    juce::ToggleButton autoDownloadUpdatesToggle { "Auto-download new updates" };
    juce::TextButton checkUpdatesButton { "Check Now" };
    juce::ListBox appListBox { "Programs", nullptr };
    juce::ListBox enabledProgramsListBox { "Enabled Programs", nullptr };
    std::unique_ptr<juce::Component> settingsPanel;
    juce::Slider outputGain;
    juce::Label outputGainLabel;
    juce::TextButton outputGainResetButton { "Reset" };

    MeterComponent meterIn;
    MeterComponent meterOut;
    DiagnosticsPanel diagnostics;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    float uiPulse { 0.0f };
    float settingsPanelAlpha { 0.0f };
    bool scannedOnVisible { false };
    bool settingsPanelTargetVisible { false };
    juce::String currentPresetName { "Default" };
    juce::String pendingPresetName;
    int dragFromRow { -1 };
    int dragToRow { -1 };
    int uiTickCount { 0 };
    int effectsHintTicks { 0 };
    bool manualEffectsOverrideAutoEnable { false };
    bool manualEffectsPinnedOn { false };
    juce::String lastPresetSnapshot;
    bool suppressPluginAddFromSelection { false };
    bool suppressControlCallbacks { false };
    bool audioApplyQueued { false };
    bool applyingAudioSettings { false };
    std::atomic<bool> updateCheckInFlight { false };
    bool hasCheckedUpdatesThisSession { false };
    juce::String latestAvailableVersion;
    EngineSettings cachedSettings;
    std::unique_ptr<ProgramListModel> programListModel;
    std::unique_ptr<ProgramListModel> enabledProgramListModel;
    std::vector<int> filteredProgramIndices;
    int selectedProgramRow { -1 };
    int selectedEnabledProgramRow { -1 };
    juce::Rectangle<int> inputIconBounds;
    juce::Rectangle<int> outputIconBounds;
    struct RunningAppEntry
    {
        juce::String name;
        juce::String path;
        juce::Image icon;
    };
    std::vector<RunningAppEntry> runningApps;

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void timerCallback() override;
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void loadDeviceLists();
    void applySettingsFromControls();
    void refreshKnownPlugins();
    void refreshPluginChainUi();
    void autoScanVstFolders();
    void refreshRunningApps();
    void refreshProgramsList();
    void refreshEnabledProgramsList();
    void selectProgramByRow(int row);
    bool addAutoEnableProgramFromPath(const juce::String& pathText);
    void removeSelectedProgram();
    juce::String getForegroundProcessName() const;
    juce::String getForegroundProcessPath() const;
    juce::Image extractAppIcon(const juce::String& exePath) const;
    void saveCachedSettings();
    std::vector<int> filterRunningAppIndices(const juce::String& query) const;
    void setSettingsPanelVisible(bool visible);
    juce::Array<juce::File> getDefaultVst3Folders() const;
    juce::String findOutputByMatch(const juce::StringArray& candidates) const;
    void startRowDrag(int row);
    void dragHoverRow(int row);
    void finishRowDrag(int row);
    void setRowMix(int row, float mix);
    void rowOpenEditor(int row);
    void closePluginEditorWindow();
    bool computeAutoEnableShouldEnable(bool& hasCondition) const;
    void setEffectsHint(const juce::String& text, int ticks = 120);
    PresetData buildCurrentPresetData(const juce::String& name);
    juce::String buildCurrentPresetSnapshot();
    bool hasUnsavedPresetChanges();
    void markCurrentPresetSnapshot();
    juce::String loadPersistedLastPresetName() const;
    void persistLastPresetName(const juce::String& presetName) const;
    void loadPresetByName(const juce::String& name);
    void setUpdateStatus(const juce::String& text, bool warning = false);
    void triggerUpdateCheck(bool manualTrigger);
};
}
