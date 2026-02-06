#include "SettingsStore.h"

namespace fizzle
{
namespace
{
EngineSettings fromVar(const juce::var& data)
{
    EngineSettings out;
    if (auto* obj = data.getDynamicObject())
    {
        out.inputDeviceName = obj->getProperty("inputDevice").toString();
        out.outputDeviceName = obj->getProperty("outputDevice").toString();
        out.outputDeviceType = obj->getProperty("outputDeviceType").toString();
        out.bufferSize = obj->hasProperty("bufferSize") ? static_cast<int>(obj->getProperty("bufferSize")) : kDefaultBlockSize;
        out.preferredSampleRate = obj->hasProperty("preferredSampleRate") ? static_cast<double>(obj->getProperty("preferredSampleRate")) : kInternalSampleRate;
        out.vstScanFolder = obj->getProperty("vstScanFolder").toString();
        out.hasCompletedInitialVstScan = obj->hasProperty("hasCompletedInitialVstScan") ? static_cast<bool>(obj->getProperty("hasCompletedInitialVstScan")) : false;
        out.autoEnableByApp = obj->hasProperty("autoEnableByApp") ? static_cast<bool>(obj->getProperty("autoEnableByApp")) : false;
        out.autoEnableProcessName = obj->getProperty("autoEnableProcessName").toString();
        if (auto* arr = obj->getProperty("autoEnableProcessNames").getArray())
        {
            for (const auto& v : *arr)
                out.autoEnableProcessNames.add(v.toString());
        }
        if (out.autoEnableProcessNames.isEmpty() && out.autoEnableProcessName.isNotEmpty())
            out.autoEnableProcessNames.add(out.autoEnableProcessName);
        out.autoEnableProcessPath = obj->getProperty("autoEnableProcessPath").toString();
        out.lastPresetName = obj->getProperty("lastPresetName").toString();
        out.hasSeenFirstLaunchGuide = obj->hasProperty("hasSeenFirstLaunchGuide")
                                          ? static_cast<bool>(obj->getProperty("hasSeenFirstLaunchGuide"))
                                          : false;
        if (obj->hasProperty("autoInstallUpdates"))
            out.autoInstallUpdates = static_cast<bool>(obj->getProperty("autoInstallUpdates"));
        else if (obj->hasProperty("autoDownloadUpdates"))
            out.autoInstallUpdates = static_cast<bool>(obj->getProperty("autoDownloadUpdates"));
        else
            out.autoInstallUpdates = false;
        out.startWithWindows = obj->hasProperty("startWithWindows")
                                   ? static_cast<bool>(obj->getProperty("startWithWindows"))
                                   : false;
        out.startMinimizedToTray = obj->hasProperty("startMinimizedToTray")
                                       ? static_cast<bool>(obj->getProperty("startMinimizedToTray"))
                                       : false;
        out.followAutoEnableWindowState = obj->hasProperty("followAutoEnableWindowState")
                                              ? static_cast<bool>(obj->getProperty("followAutoEnableWindowState"))
                                              : false;
        if (auto* arr = obj->getProperty("scannedVstPaths").getArray())
        {
            for (const auto& v : *arr)
                out.scannedVstPaths.add(v.toString());
        }
    }
    return out;
}

juce::var toVar(const EngineSettings& settings)
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("inputDevice", settings.inputDeviceName);
    obj->setProperty("outputDevice", settings.outputDeviceName);
    obj->setProperty("outputDeviceType", settings.outputDeviceType);
    obj->setProperty("bufferSize", settings.bufferSize);
    obj->setProperty("preferredSampleRate", settings.preferredSampleRate);
    obj->setProperty("vstScanFolder", settings.vstScanFolder);
    obj->setProperty("hasCompletedInitialVstScan", settings.hasCompletedInitialVstScan);
    obj->setProperty("autoEnableByApp", settings.autoEnableByApp);
    obj->setProperty("autoEnableProcessName", settings.autoEnableProcessName);
    juce::Array<juce::var> enabledNames;
    for (const auto& n : settings.autoEnableProcessNames)
        enabledNames.add(n);
    obj->setProperty("autoEnableProcessNames", enabledNames);
    obj->setProperty("autoEnableProcessPath", settings.autoEnableProcessPath);
    obj->setProperty("lastPresetName", settings.lastPresetName);
    obj->setProperty("hasSeenFirstLaunchGuide", settings.hasSeenFirstLaunchGuide);
    obj->setProperty("autoInstallUpdates", settings.autoInstallUpdates);
    // Keep legacy key for backward compatibility across older builds.
    obj->setProperty("autoDownloadUpdates", settings.autoInstallUpdates);
    obj->setProperty("startWithWindows", settings.startWithWindows);
    obj->setProperty("startMinimizedToTray", settings.startMinimizedToTray);
    obj->setProperty("followAutoEnableWindowState", settings.followAutoEnableWindowState);
    juce::Array<juce::var> paths;
    for (const auto& p : settings.scannedVstPaths)
        paths.add(p);
    obj->setProperty("scannedVstPaths", paths);
    return obj;
}
}

SettingsStore::SettingsStore()
    : appDir(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Fizzle")),
      settingsFile(appDir.getChildFile("settings.json"))
{
    appDir.createDirectory();
}

EngineSettings SettingsStore::loadEngineSettings() const
{
    if (! settingsFile.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse(settingsFile);
    if (parsed.isVoid())
        return {};

    return fromVar(parsed);
}

void SettingsStore::saveEngineSettings(const EngineSettings& settings) const
{
    settingsFile.replaceWithText(juce::JSON::toString(toVar(settings), true));
}

juce::File SettingsStore::getSettingsFile() const
{
    return settingsFile;
}

juce::File SettingsStore::getAppDirectory() const
{
    return appDir;
}
}
