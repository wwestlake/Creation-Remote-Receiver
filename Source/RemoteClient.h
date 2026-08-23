#pragma once

#include <JuceHeader.h>

// Thin client for the Creation Remote pairing API (djehuti /api/remote/*).
// Every call is synchronous and blocking -- callers run these off the
// message thread (see MainComponent, which drives this from its own
// juce::Thread). See docs/architecture/Creation-Remote-Protocol.md in the
// Creation-Suite umbrella repo for the wire contract.
class RemoteClient
{
public:
    struct CheckInResult
    {
        bool success = false;
        juce::String hostSessionId;
        juce::String presenceState;
        juce::String errorMessage;
    };

    struct PairingResult
    {
        bool success = false;
        juce::String pairingId;
        juce::String pairingCode;
        juce::String errorMessage;
    };

    struct PairingStatusResult
    {
        bool success = false;
        juce::String status; // "pending" | "approved"
        juce::String errorMessage;
    };

    struct ProjectSummary
    {
        juce::String projectId;
        juce::String displayName;
    };

    explicit RemoteClient(juce::String bearerToken);

    CheckInResult checkIn(const juce::String& deviceId, const juce::String& deviceName,
                          const juce::String& appVersion, const juce::Array<ProjectSummary>& projects) const;
    PairingResult createPairing(const juce::String& hostSessionId) const;
    PairingStatusResult getPairingStatus(const juce::String& pairingId) const;

private:
    juce::var postJson(const juce::String& path, const juce::var& body, int& statusCode) const;
    juce::var getJson(const juce::String& path, int& statusCode) const;

    juce::String bearerToken;
    static juce::String apiBase() { return "https://lagdaemon.com/djehuti"; }
};
