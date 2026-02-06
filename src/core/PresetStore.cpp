#include "PresetStore.h"

namespace fizzle
{
PresetStore::PresetStore(const juce::File& appDirectory)
    : presetDir(appDirectory.getChildFile("presets"))
{
    presetDir.createDirectory();
}

void PresetStore::savePreset(const PresetData& preset) const
{
    auto root = new juce::DynamicObject();
    root->setProperty("name", preset.name);
    root->setProperty("inputDevice", preset.engine.inputDeviceName);
    root->setProperty("outputDevice", preset.engine.outputDeviceName);
    root->setProperty("bufferSize", preset.engine.bufferSize);
    root->setProperty("sampleRate", preset.engine.preferredSampleRate);

    auto valuesObj = new juce::DynamicObject();
    for (const auto& [key, value] : preset.values)
        valuesObj->setProperty(key, value);
    root->setProperty("values", valuesObj);

    auto pluginsArray = juce::Array<juce::var>();
    for (const auto& plugin : preset.plugins)
    {
        auto pluginObj = new juce::DynamicObject();
        pluginObj->setProperty("identifier", plugin.identifier);
        pluginObj->setProperty("name", plugin.name);
        pluginObj->setProperty("enabled", plugin.enabled);
        pluginObj->setProperty("mix", plugin.mix);
        pluginObj->setProperty("state", plugin.base64State);
        pluginsArray.add(pluginObj);
    }
    root->setProperty("plugins", pluginsArray);

    const auto file = presetDir.getChildFile(preset.name + ".json");
    file.replaceWithText(juce::JSON::toString(juce::var(root), true));
}

std::optional<PresetData> PresetStore::loadPreset(const juce::String& name) const
{
    const auto file = presetDir.getChildFile(name + ".json");
    if (! file.existsAsFile())
        return std::nullopt;

    const auto parsed = juce::JSON::parse(file);
    if (parsed.isVoid())
        return std::nullopt;

    PresetData out;
    out.name = name;

    if (auto* obj = parsed.getDynamicObject())
    {
        out.engine.inputDeviceName = obj->getProperty("inputDevice").toString();
        out.engine.outputDeviceName = obj->getProperty("outputDevice").toString();
        out.engine.bufferSize = obj->hasProperty("bufferSize") ? static_cast<int>(obj->getProperty("bufferSize")) : kDefaultBlockSize;
        out.engine.preferredSampleRate = obj->hasProperty("sampleRate") ? static_cast<double>(obj->getProperty("sampleRate")) : kInternalSampleRate;

        if (auto* values = obj->getProperty("values").getDynamicObject())
        {
            for (const auto& p : values->getProperties())
                out.values[p.name.toString()] = static_cast<float>(p.value);
        }

        if (auto* plugins = obj->getProperty("plugins").getArray())
        {
            for (const auto& p : *plugins)
            {
                if (auto* po = p.getDynamicObject())
                {
                    PluginPresetState state;
                    state.identifier = po->getProperty("identifier").toString();
                    state.name = po->getProperty("name").toString();
                    state.enabled = po->hasProperty("enabled") ? static_cast<bool>(po->getProperty("enabled")) : true;
                    state.mix = po->hasProperty("mix") ? static_cast<float>(po->getProperty("mix")) : 1.0f;
                    state.base64State = po->getProperty("state").toString();
                    out.plugins.add(state);
                }
            }
        }
    }

    return out;
}

juce::StringArray PresetStore::listPresets() const
{
    juce::StringArray names;
    for (const auto& file : presetDir.findChildFiles(juce::File::findFiles, false, "*.json"))
        names.add(file.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

bool PresetStore::deletePreset(const juce::String& name) const
{
    if (name.isEmpty())
        return false;
    const auto file = presetDir.getChildFile(name + ".json");
    if (! file.existsAsFile())
        return false;
    return file.deleteFile();
}

juce::File PresetStore::getPresetDirectory() const
{
    return presetDir;
}
}
