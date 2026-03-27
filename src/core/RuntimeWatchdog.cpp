#include "RuntimeWatchdog.h"
#include "Logger.h"
#include <chrono>

#if JUCE_WINDOWS
#include <windows.h>
#include <psapi.h>
#endif

namespace fizzle
{
namespace
{
constexpr int64_t kWatchdogPollMs = 500;
constexpr int64_t kFreezeThresholdMs = 2500;

int64_t nowMs()
{
    return static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes());
}

juce::String sanitizeSummaryKey(juce::String value)
{
    value = value.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
    if (value.isEmpty())
        value = "unknown";
    return value;
}

juce::DynamicObject* ensureObject(juce::var& parent, const juce::Identifier& key)
{
    auto* parentObject = parent.getDynamicObject();
    if (parentObject == nullptr)
        return nullptr;

    auto child = parentObject->getProperty(key);
    if (auto* existing = child.getDynamicObject())
        return existing;

    auto* created = new juce::DynamicObject();
    parentObject->setProperty(key, juce::var(created));
    return created;
}

void incrementSummaryCounter(juce::DynamicObject& parent, const juce::String& rawKey)
{
    const auto key = juce::Identifier(sanitizeSummaryKey(rawKey));
    const auto current = static_cast<int>(parent.getProperty(key));
    parent.setProperty(key, current + 1);
}

juce::Array<juce::var> buildPluginArray(const std::vector<VstHost::HostedPluginHandle>& plugins)
{
    juce::Array<juce::var> out;
    for (const auto& plugin : plugins)
    {
        if (plugin == nullptr)
            continue;

        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", plugin->description.name);
        obj->setProperty("identifier", plugin->description.fileOrIdentifier);
        obj->setProperty("enabled", plugin->enabled.load());
        obj->setProperty("faulted", plugin->faulted.load());
        obj->setProperty("editorOpen", plugin->editorOpen.load());
        obj->setProperty("mix", plugin->mix.load());
        out.add(juce::var(obj));
    }
    return out;
}

juce::var buildProcessMemorySnapshot()
{
    auto* obj = new juce::DynamicObject();

#if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX counters {};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)))
    {
        obj->setProperty("workingSetMb", static_cast<double>(counters.WorkingSetSize) / (1024.0 * 1024.0));
        obj->setProperty("privateUsageMb", static_cast<double>(counters.PrivateUsage) / (1024.0 * 1024.0));
        obj->setProperty("pagefileMb", static_cast<double>(counters.PagefileUsage) / (1024.0 * 1024.0));
    }

    MEMORYSTATUSEX memoryStatus {};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus))
    {
        obj->setProperty("memoryLoadPercent", static_cast<int>(memoryStatus.dwMemoryLoad));
        obj->setProperty("availablePhysicalMb", static_cast<double>(memoryStatus.ullAvailPhys) / (1024.0 * 1024.0));
        obj->setProperty("totalPhysicalMb", static_cast<double>(memoryStatus.ullTotalPhys) / (1024.0 * 1024.0));
    }
#endif

    return juce::var(obj);
}

void updateSummaryFile(const juce::File& summaryFile,
                       const juce::String& activity,
                       const std::vector<VstHost::HostedPluginHandle>& plugins,
                       int totalIncidents)
{
    juce::var rootVar;
    if (summaryFile.existsAsFile())
        rootVar = juce::JSON::parse(summaryFile);

    if (rootVar.isVoid() || rootVar.getDynamicObject() == nullptr)
        rootVar = juce::var(new juce::DynamicObject());

    auto* root = rootVar.getDynamicObject();
    if (root == nullptr)
        return;

    root->setProperty("totalIncidents", totalIncidents);
    root->setProperty("lastActivity", activity);
    root->setProperty("lastReportedUtc", juce::Time::getCurrentTime().toISO8601(true));

    if (auto* activityCounts = ensureObject(rootVar, "activityCounts"))
        incrementSummaryCounter(*activityCounts, activity);

    if (auto* pluginCounts = ensureObject(rootVar, "pluginCounts"))
    {
        for (const auto& plugin : plugins)
        {
            if (plugin == nullptr)
                continue;
            incrementSummaryCounter(*pluginCounts, plugin->description.name);
        }
    }

    summaryFile.replaceWithText(juce::JSON::toString(rootVar, true));
}
}

RuntimeWatchdog::ScopedActivity::ScopedActivity(RuntimeWatchdog& ownerRef, juce::String label)
    : owner(&ownerRef)
{
    auto previous = owner->pushActivity(label);
    previousActivity = std::move(previous.first);
    previousStartedMs = previous.second;
    owner->heartbeat();
}

RuntimeWatchdog::ScopedActivity::~ScopedActivity()
{
    if (owner != nullptr)
    {
        owner->popActivity(previousActivity, previousStartedMs);
        owner->heartbeat();
    }
}

RuntimeWatchdog::ScopedActivity::ScopedActivity(ScopedActivity&& other) noexcept
    : owner(other.owner),
      previousActivity(std::move(other.previousActivity)),
      previousStartedMs(other.previousStartedMs)
{
    other.owner = nullptr;
    other.previousStartedMs = 0;
}

RuntimeWatchdog::ScopedActivity& RuntimeWatchdog::ScopedActivity::operator=(ScopedActivity&& other) noexcept
{
    if (this == &other)
        return *this;

    if (owner != nullptr)
    {
        owner->popActivity(previousActivity, previousStartedMs);
        owner->heartbeat();
    }

    owner = other.owner;
    previousActivity = std::move(other.previousActivity);
    previousStartedMs = other.previousStartedMs;
    other.owner = nullptr;
    other.previousStartedMs = 0;
    return *this;
}

RuntimeWatchdog::RuntimeWatchdog(AudioEngine& engineRef)
    : engine(engineRef)
{
    lastHeartbeatMs.store(nowMs());
}

RuntimeWatchdog::~RuntimeWatchdog()
{
    stop();
}

void RuntimeWatchdog::start()
{
    if (running.exchange(true))
        return;

    incidentReported.store(false);
    heartbeat();
    watchdogThread = std::thread([this] { watchdogLoop(); });
}

void RuntimeWatchdog::stop()
{
    if (! running.exchange(false))
        return;

    if (watchdogThread.joinable())
        watchdogThread.join();
}

void RuntimeWatchdog::heartbeat()
{
    lastHeartbeatMs.store(nowMs());
}

void RuntimeWatchdog::setStateSnapshot(const juce::String& snapshot)
{
    const juce::ScopedLock sl(stateLock);
    stateSnapshot = snapshot;
}

void RuntimeWatchdog::setMonitoringSuspended(bool suspended)
{
    monitoringSuspended.store(suspended);
    if (suspended)
    {
        incidentReported.store(false);
        heartbeat();
    }
}

RuntimeWatchdog::ScopedActivity RuntimeWatchdog::scopedActivity(const juce::String& label)
{
    return ScopedActivity(*this, label);
}

juce::String RuntimeWatchdog::getLastIncidentSummary() const
{
    const juce::ScopedLock sl(stateLock);
    return lastIncidentSummary;
}

int RuntimeWatchdog::getIncidentCount() const
{
    return incidentCount.load();
}

std::pair<juce::String, int64_t> RuntimeWatchdog::pushActivity(const juce::String& activity)
{
    const juce::ScopedLock sl(stateLock);
    auto previous = std::make_pair(currentActivity, currentActivityStartedMs.load());
    currentActivity = activity.isNotEmpty() ? activity : "idle";
    currentActivityStartedMs.store(nowMs());
    return previous;
}

void RuntimeWatchdog::popActivity(const juce::String& previousActivity, int64_t previousStartedMs)
{
    const juce::ScopedLock sl(stateLock);
    currentActivity = previousActivity.isNotEmpty() ? previousActivity : "idle";
    currentActivityStartedMs.store(previousStartedMs);
}

void RuntimeWatchdog::watchdogLoop()
{
    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kWatchdogPollMs));

        if (! running.load())
            break;

        if (monitoringSuspended.load())
        {
            incidentReported.store(false);
            continue;
        }

        const auto ageMs = nowMs() - lastHeartbeatMs.load();
        if (ageMs >= kFreezeThresholdMs)
        {
            if (! incidentReported.exchange(true))
            {
                juce::String activity;
                juce::String snapshot;
                {
                    const juce::ScopedLock sl(stateLock);
                    activity = currentActivity;
                    snapshot = stateSnapshot;
                }

                const auto activityStartMs = currentActivityStartedMs.load();
                const auto activityAgeMs = activityStartMs > 0 ? nowMs() - activityStartMs : 0;
                reportFreeze(ageMs, activityAgeMs, activity, snapshot);
            }
        }
        else if (incidentReported.exchange(false))
        {
            Logger::instance().log("UI watchdog recovered after freeze.");
        }
    }
}

void RuntimeWatchdog::reportFreeze(int64_t heartbeatAgeMs,
                                   int64_t activityAgeMs,
                                   const juce::String& activity,
                                   const juce::String& snapshot)
{
    const auto diagnostics = engine.getDiagnostics();
    const auto plugins = engine.getVstHost().getChainHandles();
    const auto incidentIndex = incidentCount.fetch_add(1) + 1;
    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    const auto logDir = Logger::instance().getLogDirectory();
    logDir.createDirectory();
    const auto reportFile = logDir.getChildFile("freeze-" + timestamp + ".json");
    const auto summaryFile = logDir.getChildFile("freeze-summary.json");

    auto* root = new juce::DynamicObject();
    root->setProperty("capturedAtUtc", juce::Time::getCurrentTime().toISO8601(true));
    root->setProperty("incidentIndex", incidentIndex);
    root->setProperty("heartbeatAgeMs", static_cast<int64>(heartbeatAgeMs));
    root->setProperty("activityAgeMs", static_cast<int64>(activityAgeMs));
    root->setProperty("currentActivity", activity);
    root->setProperty("uiState", snapshot);
    root->setProperty("audioInputDevice", diagnostics.inputDevice);
    root->setProperty("audioOutputDevice", diagnostics.outputDevice);
    root->setProperty("sampleRate", diagnostics.sampleRate);
    root->setProperty("bufferSize", diagnostics.bufferSize);
    root->setProperty("cpuPercent", diagnostics.cpuPercent);
    root->setProperty("dryLatencyMs", diagnostics.dryLatencyMs);
    root->setProperty("postFxLatencyMs", diagnostics.postFxLatencyMs);
    root->setProperty("inputLevel", diagnostics.inputLevel);
    root->setProperty("outputLevel", diagnostics.outputLevel);
    root->setProperty("droppedBuffers", static_cast<int64>(diagnostics.droppedBuffers));
    root->setProperty("plugins", buildPluginArray(plugins));
    root->setProperty("processMemory", buildProcessMemorySnapshot());

    reportFile.replaceWithText(juce::JSON::toString(juce::var(root), true));
    updateSummaryFile(summaryFile, activity, plugins, incidentIndex);

    {
        const juce::ScopedLock sl(stateLock);
        lastIncidentSummary = "UI freeze " + juce::String(static_cast<int>(heartbeatAgeMs))
                            + "ms while " + activity
                            + " | report=" + reportFile.getFileName();
    }

    Logger::instance().log(getLastIncidentSummary());
}
}
