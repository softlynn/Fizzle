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

bool VstHost::createHostedPlugin(const juce::PluginDescription& description,
                                 double sampleRate,
                                 int blockSize,
                                 juce::String& error,
                                 HostedPluginPtr& outHosted)
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

    auto hosted = std::make_shared<HostedPlugin>();
    hosted->description = resolved;
    hosted->instance.reset(result.release());
    hosted->instance->prepareToPlay(sampleRate, blockSize);
    outHosted = std::move(hosted);
    return true;
}

std::vector<VstHost::HostedPluginPtr> VstHost::copyChainSnapshot() const
{
    const juce::ScopedLock sl(chainLock);
    return chain;
}

void VstHost::refreshLatencyCacheLocked()
{
    int total = 0;
    for (const auto& plugin : chain)
    {
        if (plugin == nullptr || plugin->instance == nullptr || ! plugin->enabled.load())
            continue;

        try
        {
            const juce::SpinLock::ScopedLockType lock(plugin->callbackLock);
            total += plugin->instance->getLatencySamples();
        }
        catch (...)
        {
            Logger::instance().log("VST latency query failed for " + plugin->description.name);
        }
    }

    cachedLatencySamples.store(total);
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
    HostedPluginPtr hosted;
    if (! createHostedPlugin(description, sampleRate, blockSize, error, hosted))
        return false;

    const juce::ScopedLock sl(chainLock);
    chain.push_back(std::move(hosted));
    refreshLatencyCacheLocked();
    return true;
}

bool VstHost::addPluginWithState(const juce::PluginDescription& description,
                                 double sampleRate,
                                 int blockSize,
                                 const juce::String& base64State,
                                 juce::String& error)
{
    HostedPluginPtr hosted;
    if (! createHostedPlugin(description, sampleRate, blockSize, error, hosted))
        return false;

    if (base64State.isNotEmpty())
    {
        juce::MemoryBlock state;
        if (state.fromBase64Encoding(base64State))
        {
            try
            {
                if (hosted->instance != nullptr)
                    hosted->instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            }
            catch (...)
            {
                Logger::instance().log("VST state restore failed for " + hosted->description.name);
            }
        }
    }

    {
        const juce::ScopedLock sl(chainLock);
        chain.push_back(std::move(hosted));
        refreshLatencyCacheLocked();
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
    if (! juce::isPositiveAndBelow(index, static_cast<int>(chain.size())))
        return;

    chain.erase(chain.begin() + index);
    refreshLatencyCacheLocked();
}

void VstHost::movePlugin(int from, int to)
{
    const juce::ScopedLock sl(chainLock);
    if (! juce::isPositiveAndBelow(from, static_cast<int>(chain.size()))
        || ! juce::isPositiveAndBelow(to, static_cast<int>(chain.size()))
        || from == to)
        return;

    auto item = std::move(chain[static_cast<size_t>(from)]);
    chain.erase(chain.begin() + from);
    chain.insert(chain.begin() + to, std::move(item));
}

void VstHost::swapPlugin(int first, int second)
{
    const juce::ScopedLock sl(chainLock);
    if (! juce::isPositiveAndBelow(first, static_cast<int>(chain.size()))
        || ! juce::isPositiveAndBelow(second, static_cast<int>(chain.size()))
        || first == second)
        return;

    std::swap(chain[static_cast<size_t>(first)], chain[static_cast<size_t>(second)]);
}

void VstHost::setEnabled(int index, bool enabled)
{
    HostedPluginPtr target;
    const juce::ScopedLock sl(chainLock);
    if (! juce::isPositiveAndBelow(index, static_cast<int>(chain.size())))
        return;

    target = chain[static_cast<size_t>(index)];
    if (target != nullptr)
        target->enabled.store(enabled);
    refreshLatencyCacheLocked();
}

void VstHost::setMix(int index, float mix)
{
    const juce::ScopedLock sl(chainLock);
    if (! juce::isPositiveAndBelow(index, static_cast<int>(chain.size())))
        return;

    if (auto& p = chain[static_cast<size_t>(index)])
        p->mix.store(juce::jlimit(0.0f, 1.0f, mix));
}

float VstHost::getMix(int index) const
{
    const juce::ScopedLock sl(chainLock);
    if (juce::isPositiveAndBelow(index, static_cast<int>(chain.size())))
        if (auto& p = chain[static_cast<size_t>(index)])
            return p->mix.load();
    return 1.0f;
}

void VstHost::clear()
{
    const juce::ScopedLock sl(chainLock);
    chain.clear();
    cachedLatencySamples.store(0);
}

void VstHost::processBlock(juce::AudioBuffer<float>& buffer)
{
    const auto snapshot = copyChainSnapshot();
    if (snapshot.empty())
        return;

    juce::MidiBuffer midi;
    if (wetBuffer.getNumChannels() != buffer.getNumChannels()
        || wetBuffer.getNumSamples() != buffer.getNumSamples())
    {
        wetBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    }

    for (const auto& plugin : snapshot)
    {
        if (plugin == nullptr || plugin->instance == nullptr || ! plugin->enabled.load())
            continue;

        const juce::SpinLock::ScopedTryLockType lock(plugin->callbackLock);
        if (! lock.isLocked())
            continue;

        wetBuffer.makeCopyOf(buffer, true);
        try
        {
            plugin->instance->processBlock(wetBuffer, midi);
        }
        catch (...)
        {
            plugin->enabled.store(false);
            Logger::instance().log("VST process failed for " + plugin->description.name + " (disabled)");
            continue;
        }

        const auto mix = juce::jlimit(0.0f, 1.0f, plugin->mix.load());
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
    return cachedLatencySamples.load();
}

juce::Array<HostedPlugin*> VstHost::getChain()
{
    const juce::ScopedLock sl(chainLock);
    juce::Array<HostedPlugin*> out;
    out.ensureStorageAllocated(static_cast<int>(chain.size()));
    for (const auto& p : chain)
        out.add(p.get());
    return out;
}

HostedPlugin* VstHost::getPlugin(int index)
{
    const juce::ScopedLock sl(chainLock);
    return juce::isPositiveAndBelow(index, static_cast<int>(chain.size()))
         ? chain[static_cast<size_t>(index)].get()
         : nullptr;
}

VstHost::HostedPluginHandle VstHost::getPluginHandle(int index)
{
    const juce::ScopedLock sl(chainLock);
    return juce::isPositiveAndBelow(index, static_cast<int>(chain.size()))
         ? chain[static_cast<size_t>(index)]
         : HostedPluginHandle{};
}

std::vector<VstHost::HostedPluginHandle> VstHost::getChainHandles() const
{
    return copyChainSnapshot();
}

void VstHost::prepare(double sampleRate, int blockSize)
{
    const auto snapshot = copyChainSnapshot();
    for (const auto& plugin : snapshot)
    {
        if (plugin == nullptr || plugin->instance == nullptr)
            continue;
        try
        {
            const juce::SpinLock::ScopedLockType lock(plugin->callbackLock);
            plugin->instance->releaseResources();
            plugin->instance->prepareToPlay(sampleRate, blockSize);
        }
        catch (...)
        {
            Logger::instance().log("VST prepare failed for " + plugin->description.name);
        }
    }

    const juce::ScopedLock sl(chainLock);
    refreshLatencyCacheLocked();
}

void VstHost::release()
{
    const auto snapshot = copyChainSnapshot();
    for (const auto& plugin : snapshot)
    {
        if (plugin == nullptr || plugin->instance == nullptr)
            continue;
        try
        {
            const juce::SpinLock::ScopedLockType lock(plugin->callbackLock);
            plugin->instance->releaseResources();
        }
        catch (...)
        {
            Logger::instance().log("VST release failed for " + plugin->description.name);
        }
    }
}
}
