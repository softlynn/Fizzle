#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <thread>
#include <utility>
#include "../audio/AudioEngine.h"

namespace fizzle
{
class RuntimeWatchdog
{
public:
    class ScopedActivity
    {
    public:
        ScopedActivity(RuntimeWatchdog& ownerRef, juce::String label);
        ~ScopedActivity();

        ScopedActivity(ScopedActivity&& other) noexcept;
        ScopedActivity& operator=(ScopedActivity&& other) noexcept;

        ScopedActivity(const ScopedActivity&) = delete;
        ScopedActivity& operator=(const ScopedActivity&) = delete;

    private:
        RuntimeWatchdog* owner { nullptr };
        juce::String previousActivity;
        int64_t previousStartedMs { 0 };
    };

    explicit RuntimeWatchdog(AudioEngine& engineRef);
    ~RuntimeWatchdog();

    void start();
    void stop();

    void heartbeat();
    void setStateSnapshot(const juce::String& snapshot);
    void setMonitoringSuspended(bool suspended);

    [[nodiscard]] ScopedActivity scopedActivity(const juce::String& label);
    [[nodiscard]] juce::String getLastIncidentSummary() const;
    [[nodiscard]] int getIncidentCount() const;

private:
    AudioEngine& engine;
    std::atomic<bool> running { false };
    std::atomic<bool> monitoringSuspended { false };
    std::atomic<bool> incidentReported { false };
    std::atomic<int64_t> lastHeartbeatMs { 0 };
    std::atomic<int64_t> currentActivityStartedMs { 0 };
    std::atomic<int> incidentCount { 0 };
    mutable juce::CriticalSection stateLock;
    juce::String currentActivity { "idle" };
    juce::String stateSnapshot;
    juce::String lastIncidentSummary;
    std::thread watchdogThread;

    std::pair<juce::String, int64_t> pushActivity(const juce::String& activity);
    void popActivity(const juce::String& previousActivity, int64_t previousStartedMs);
    void watchdogLoop();
    void reportFreeze(int64_t heartbeatAgeMs,
                      int64_t activityAgeMs,
                      const juce::String& activity,
                      const juce::String& snapshot);

    friend class ScopedActivity;
};
}
