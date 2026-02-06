#include "VstHost.h"
#include "../core/Logger.h"

namespace fizzle
{
namespace
{
juce::AudioPluginFormat* getVst3Format(juce::AudioPluginFormatManager& fm)
{
    for (int i = 0; i < fm.getNumFormats(); ++i)
    {
        auto* format = fm.getFormat(i);
        if (format != nullptr && format->getName().containsIgnoreCase("VST3"))
            return format;
    }
    return nullptr;
}
}

VstHost::VstHost()
{
    formatManager.addDefaultFormats();
}

juce::StringArray VstHost::scanFolder(const juce::File& folder)
{
    juce::StringArray found;

    if (! folder.isDirectory())
        return found;

    juce::Array<juce::File> files;
    folder.findChildFiles(files, juce::File::findFiles, true, "*.vst3");

    for (const auto& f : files)
    {
        const auto path = f.getFullPathName();
        bool already = false;
        for (const auto& s : scanned)
        {
            if (s.path == path)
            {
                already = true;
                break;
            }
        }

        if (! already)
        {
            scanned.add({ f.getFileNameWithoutExtension(), path });
            found.add(path);
        }
    }

    return found;
}

juce::Array<juce::PluginDescription> VstHost::getKnownPluginDescriptions() const
{
    juce::Array<juce::PluginDescription> out;
    for (const auto& s : scanned)
    {
        juce::PluginDescription d;
        d.name = s.name;
        d.fileOrIdentifier = s.path;
        d.pluginFormatName = "VST3";
        out.add(d);
    }
    return out;
}

juce::StringArray VstHost::getScannedPaths() const
{
    juce::StringArray out;
    for (const auto& s : scanned)
        out.add(s.path);
    return out;
}

void VstHost::importScannedPaths(const juce::StringArray& paths)
{
    for (const auto& p : paths)
    {
        bool exists = false;
        for (const auto& s : scanned)
        {
            if (s.path == p)
            {
                exists = true;
                break;
            }
        }
        if (! exists)
            scanned.add({ juce::File(p).getFileNameWithoutExtension(), p });
    }
}

bool VstHost::addPlugin(const juce::PluginDescription& description, double sampleRate, int blockSize, juce::String& error)
{
    auto* vst3Format = getVst3Format(formatManager);
    if (vst3Format == nullptr)
    {
        error = "VST3 format unavailable";
        return false;
    }

    juce::OwnedArray<juce::PluginDescription> types;
    vst3Format->findAllTypesForFile(types, description.fileOrIdentifier);
    if (types.isEmpty())
    {
        error = "Could not load plugin description from: " + description.fileOrIdentifier;
        return false;
    }

    auto resolved = *types[0];
    auto result = formatManager.createPluginInstance(resolved, sampleRate, blockSize, error);
    if (result == nullptr)
        return false;

    std::unique_ptr<juce::AudioPluginInstance> instance(result.release());
    instance->prepareToPlay(sampleRate, blockSize);

    const juce::ScopedLock sl(chainLock);
    auto* hosted = new HostedPlugin();
    hosted->description = resolved;
    hosted->instance = std::move(instance);
    chain.add(hosted);
    return true;
}

bool VstHost::addPluginWithState(const juce::PluginDescription& description,
                                 double sampleRate,
                                 int blockSize,
                                 const juce::String& base64State,
                                 juce::String& error)
{
    if (! addPlugin(description, sampleRate, blockSize, error))
        return false;

    if (base64State.isNotEmpty())
    {
        juce::MemoryBlock state;
        if (state.fromBase64Encoding(base64State))
        {
            const juce::ScopedLock sl(chainLock);
            if (auto* plugin = chain.getLast())
                plugin->instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        }
    }

    return true;
}

bool VstHost::findDescriptionByIdentifier(const juce::String& identifier, juce::PluginDescription& out) const
{
    for (const auto& s : scanned)
    {
        if (s.path == identifier)
        {
            out.name = s.name;
            out.fileOrIdentifier = s.path;
            out.pluginFormatName = "VST3";
            return true;
        }
    }

    return false;
}

void VstHost::removePlugin(int index)
{
    const juce::ScopedLock sl(chainLock);
    if (juce::isPositiveAndBelow(index, chain.size()))
        chain.remove(index);
}

void VstHost::movePlugin(int from, int to)
{
    const juce::ScopedLock sl(chainLock);
    if (juce::isPositiveAndBelow(from, chain.size()) && juce::isPositiveAndBelow(to, chain.size()))
        chain.move(from, to);
}

void VstHost::swapPlugin(int first, int second)
{
    const juce::ScopedLock sl(chainLock);
    if (! juce::isPositiveAndBelow(first, chain.size())
        || ! juce::isPositiveAndBelow(second, chain.size())
        || first == second)
        return;

    chain.swap(first, second);
}

void VstHost::setEnabled(int index, bool enabled)
{
    const juce::ScopedLock sl(chainLock);
    if (juce::isPositiveAndBelow(index, chain.size()))
        if (auto* p = chain[index])
        p->enabled = enabled;
}

void VstHost::setMix(int index, float mix)
{
    const juce::ScopedLock sl(chainLock);
    if (juce::isPositiveAndBelow(index, chain.size()))
        if (auto* p = chain[index])
        p->mix = juce::jlimit(0.0f, 1.0f, mix);
}

float VstHost::getMix(int index) const
{
    const juce::ScopedLock sl(chainLock);
    if (juce::isPositiveAndBelow(index, chain.size()))
        if (auto* p = chain[index])
        return p->mix;
    return 1.0f;
}

void VstHost::clear()
{
    const juce::ScopedLock sl(chainLock);
    chain.clear();
}

void VstHost::processBlock(juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(chainLock);

    juce::MidiBuffer midi;
    wetBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

    for (auto* plugin : chain)
    {
        if (plugin == nullptr || ! plugin->enabled || plugin->instance == nullptr)
            continue;

        wetBuffer.makeCopyOf(buffer, true);
        plugin->instance->processBlock(wetBuffer, midi);

        const auto mix = juce::jlimit(0.0f, 1.0f, plugin->mix);
        const auto inv = 1.0f - mix;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* dry = buffer.getWritePointer(c);
            const auto* wet = wetBuffer.getReadPointer(c);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                dry[i] = dry[i] * inv + wet[i] * mix;
        }
    }
}

int VstHost::getLatencySamples() const
{
    const juce::ScopedLock sl(chainLock);
    int total = 0;
    for (auto* plugin : chain)
    {
        if (plugin != nullptr && plugin->enabled && plugin->instance != nullptr)
            total += plugin->instance->getLatencySamples();
    }
    return total;
}

juce::Array<HostedPlugin*> VstHost::getChain()
{
    const juce::ScopedLock sl(chainLock);
    juce::Array<HostedPlugin*> out;
    for (auto* p : chain)
        out.add(p);
    return out;
}

HostedPlugin* VstHost::getPlugin(int index)
{
    const juce::ScopedLock sl(chainLock);
    return juce::isPositiveAndBelow(index, chain.size()) ? chain[index] : nullptr;
}

void VstHost::prepare(double sampleRate, int blockSize)
{
    const juce::ScopedLock sl(chainLock);
    for (auto* plugin : chain)
    {
        if (plugin == nullptr || plugin->instance == nullptr)
            continue;
        try
        {
            plugin->instance->releaseResources();
            plugin->instance->prepareToPlay(sampleRate, blockSize);
        }
        catch (...)
        {
            Logger::instance().log("VST prepare failed for " + plugin->description.name);
        }
    }
}

void VstHost::release()
{
    const juce::ScopedLock sl(chainLock);
    for (auto* plugin : chain)
    {
        if (plugin == nullptr || plugin->instance == nullptr)
            continue;
        try
        {
            plugin->instance->releaseResources();
        }
        catch (...)
        {
            Logger::instance().log("VST release failed for " + plugin->description.name);
        }
    }
}
}
