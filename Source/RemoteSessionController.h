#pragma once

#include <JuceHeader.h>
#include "RemoteClient.h"

// Drives check-in and the pairing-code lifecycle on a background thread,
// posting results back to the message thread -- same run()+callAsync shape
// as creation::ui::SuiteDesktopAuthSession, which this sits on top of for
// auth. One controller instance per receiver process lifetime.
class RemoteSessionController final : private juce::Thread
{
public:
    RemoteSessionController();
    ~RemoteSessionController() override;

    // Starts the background thread: check in, then wait for requestPairing().
    // projects is read once at construction of the check-in payload each
    // heartbeat -- MainComponent refreshes it by calling setProjects() before
    // the next heartbeat needs current data (project discovery doesn't
    // change often enough to need finer-grained invalidation than that).
    void start(juce::String bearerToken);
    void stop();
    void setProjects(juce::Array<RemoteClient::ProjectSummary> newProjects);

    // Requests a new pairing code; result arrives via onPairingReady/onPairingFailed.
    void requestPairing();

    juce::String getHostSessionId() const { return hostSessionId; }

    std::function<void(juce::String /*presenceState*/)> onCheckedIn;
    std::function<void(juce::String /*message*/)> onCheckInFailed;
    std::function<void(juce::String /*pairingCode*/)> onPairingReady;
    std::function<void(juce::String /*message*/)> onPairingFailed;
    std::function<void()> onPairingApproved;

private:
    void run() override;

    juce::String bearerToken;
    juce::String hostSessionId;
    juce::String pendingPairingId;
    std::atomic<bool> pairingRequested { false };
    juce::WaitableEvent wakeEvent;

    juce::CriticalSection projectsLock;
    juce::Array<RemoteClient::ProjectSummary> projects;
};
