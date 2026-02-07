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
    void paintOverChildren(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void refreshPresets();
    void onWindowVisible();
    void trayToggleEffectsBypass();
    void trayToggleMute();
    void trayRestartAudio();
    void trayLoadPreset(const juce::String& name);
    bool areEffectsEnabled() const;
    bool isMuted() const;
    bool isLightModeEnabled() const;
    int getThemeVariant() const;
    int getUiDensity() const;

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
    juce::TextButton windowMinButton;
    juce::TextButton windowMaxButton;
    juce::TextButton windowCloseButton;
    juce::ToggleButton toneButton;
    juce::ToggleButton autoEnableToggle { "Auto Enable by App" };
    juce::TextButton refreshAppsButton { "Refresh" };
    juce::Label appEnableLabel;
    juce::Label appSearchLabel;
    juce::Label appListLabel;
    juce::Label enabledProgramsLabel;
    juce::Label appearanceLabel;
    juce::Label updatesLabel;
    juce::Label updateStatusLabel;
    juce::Label settingsNavLabel;
    juce::TextButton settingsNavAutoEnableButton { "App Auto-Enable" };
    juce::TextButton settingsNavAppearanceButton { "Appearance" };
    juce::TextButton settingsNavUpdatesButton { "Updates" };
    juce::TextButton settingsNavStartupButton { "Startup" };
    juce::Label appearanceModeLabel;
    juce::ToggleButton lightModeToggle { "Light mode" };
    juce::Label appearanceThemeLabel;
    juce::ComboBox appearanceThemeBox;
    juce::Label appearanceBackgroundLabel;
    juce::ComboBox appearanceBackgroundBox;
    juce::Label appearanceSizeLabel;
    juce::ComboBox appearanceSizeBox;
    juce::TextEditor appSearchEditor;
    juce::TextEditor appPathEditor;
    juce::TextButton browseAppPathButton { "Browse..." };
    juce::TextButton removeProgramButton { "Remove Selected" };
    juce::TextButton closeSettingsButton { "Close" };
    juce::ToggleButton autoDownloadUpdatesToggle { "Auto-install new updates" };
    juce::TextButton checkUpdatesButton { "Check Now" };
    juce::TextButton updatesGithubButton { "Open GitHub Repo" };
    juce::TextButton updatesWebsiteButton { "Open Website" };
    juce::Label updatesLinksLabel;
    juce::Label startupLabel;
    juce::Label behaviorListenDeviceLabel;
    juce::ComboBox behaviorListenDeviceBox;
    juce::Label behaviorVstFoldersLabel;
    juce::ListBox behaviorVstFoldersListBox { "VST Search Folders", nullptr };
    juce::TextButton behaviorAddVstFolderButton { "Add Folder..." };
    juce::TextButton behaviorRemoveVstFolderButton { "Remove Folder" };
    juce::ToggleButton startWithWindowsToggle { "Start with Windows" };
    juce::ToggleButton startMinimizedToggle { "Start minimized to tray" };
    juce::ToggleButton followAutoEnableWindowToggle { "Open/close window with Program Auto-Enable" };
    juce::Label startupHintLabel;
    juce::ListBox appListBox { "Programs", nullptr };
    juce::ListBox enabledProgramsListBox { "Enabled Programs", nullptr };
    std::unique_ptr<juce::Component> settingsPanel;
    juce::Slider outputGain;
    juce::Label outputGainLabel;
    juce::TextButton outputGainResetButton { "Reset" };
    juce::Label versionLabel;

    MeterComponent meterIn;
    MeterComponent meterOut;
    DiagnosticsPanel diagnostics;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    float uiPulse { 0.0f };
    float settingsPanelAlpha { 0.0f };
    float savePresetFlashAlpha { 0.0f };
    float aboutFlashAlpha { 0.0f };
    bool scannedOnVisible { false };
    bool settingsPanelTargetVisible { false };
    juce::String currentPresetName { "Default" };
    juce::String pendingPresetName;
    int dragFromRow { -1 };
    int dragToRow { -1 };
    int uiTickCount { 0 };
    int effectsHintTicks { 0 };
    float effectsHintAlpha { 0.0f };
    float effectsHintTargetAlpha { 0.0f };
    float styleTransitionAlpha { 0.0f };
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
    int activeSettingsTab { 0 };
    bool wasFollowAutoEnableMatched { false };
    EngineSettings cachedSettings;
    std::unique_ptr<ProgramListModel> programListModel;
    std::unique_ptr<ProgramListModel> enabledProgramListModel;
    std::unique_ptr<ProgramListModel> vstSearchPathListModel;
    std::vector<int> filteredProgramIndices;
    int selectedProgramRow { -1 };
    int selectedEnabledProgramRow { -1 };
    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> headerLogoBounds;
    juce::Rectangle<int> inputIconBounds;
    juce::Rectangle<int> outputIconBounds;
    juce::ComponentDragger windowDragger;
    bool draggingWindow { false };
    bool draggingResizeGrip { false };
    juce::Point<int> resizeDragStartScreen;
    juce::Rectangle<int> resizeStartBounds;
    juce::Rectangle<int> resizeGripBounds;
    bool windowMaximized { false };
    juce::Rectangle<int> windowRestoreBounds;
    struct RunningAppEntry
    {
        juce::String name;
        juce::String path;
        juce::Image icon;
    };
    std::vector<RunningAppEntry> runningApps;
    juce::StringArray outputDeviceRealNames;
    juce::Image appLogo;
    juce::String lastVirtualMicStatus;
    double lastDisplayedSampleRateHz { 0.0 };
    bool lastListenEnabledState { false };
    bool lastMutedState { false };
    bool restartOverlayActive { false };
    bool restartOverlayBusy { false };
    float restartOverlayAlpha { 0.0f };
    float restartOverlayTargetAlpha { 0.0f };
    int restartOverlayTicks { 0 };
    juce::String restartOverlayText;
    int settingsScrollY { 0 };
    int settingsScrollMax { 0 };
    int uiTimerHz { 30 };
    float uiScale { 1.0f };
    struct FizzBubble
    {
        juce::Point<float> pos;
        float radius { 0.0f };
        float speed { 0.0f };
        float drift { 0.0f };
        float alpha { 0.0f };
    };
    std::vector<FizzBubble> logoFizzBubbles;
    bool logoFizzActive { false };
    int logoFizzTicks { 0 };
    struct RemovedPluginSnapshot
    {
        juce::PluginDescription description;
        juce::String base64State;
        bool enabled { true };
        float mix { 1.0f };
        int index { -1 };
        bool valid { false };
    };
    RemovedPluginSnapshot lastRemovedPlugin;

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
    void refreshListenOutputDevices();
    void refreshVstSearchPathList();
    void addVstSearchPath(const juce::String& path);
    void removeSelectedVstSearchPath();
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
    juce::String getSelectedOutputDeviceName() const;
    juce::String getDisplayOutputName(const juce::String& realOutputName) const;
    bool isVirtualMicName(const juce::String& outputName) const;
    void startRowDrag(int row);
    void dragHoverRow(int row);
    void finishRowDrag(int row);
    void setRowMix(int row, float mix);
    void setRowEnabled(int row, bool enabled);
    void rowQuickActionMenu(int row, juce::Point<int> screenPosition);
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
    void updateSettingsTabVisibility();
    void setActiveSettingsTab(int tab);
    void triggerButtonFlash(juce::Button* button);
    void showFirstLaunchGuide();
    void triggerLogoFizzAnimation();
    void capturePluginSnapshotForUndo(int index);
    void restoreLastRemovedPlugin();
    bool removePluginAtIndex(int index, bool captureUndo = true);
    void applyWindowsStartupSetting();
    void toggleWindowMaximize();
    void applyEffectsEnabledState(bool enabled, bool fromUserToggle);
    void runAudioRestartWithOverlay(bool fromTray);
    void applyThemePalette();
    void applyUiDensity();
    void refreshAppearanceControls();
    void scrollSettingsContent(int deltaPixels);
};
}
