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
    void start(juce::String bearerToken);
    void stop();

    // Requests a new pairing code; result arrives via onPairingReady/onPairingFailed.
    void requestPairing();

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
};
