#include "RemoteSessionController.h"

namespace
{
juce::String deviceName()
{
    return juce::SystemStats::getComputerName();
}
}

RemoteSessionController::RemoteSessionController() : juce::Thread("Djehuti Remote Session") {}

RemoteSessionController::~RemoteSessionController()
{
    stop();
}

void RemoteSessionController::start(juce::String bearerTokenToUse)
{
    bearerToken = std::move(bearerTokenToUse);
    startThread();
}

void RemoteSessionController::stop()
{
    signalThreadShouldExit();
    wakeEvent.signal();
    stopThread(5000);
}

void RemoteSessionController::requestPairing()
{
    pairingRequested = true;
    wakeEvent.signal();
}

void RemoteSessionController::setProjects(juce::Array<RemoteClient::ProjectSummary> newProjects)
{
    juce::ScopedLock lock(projectsLock);
    projects = std::move(newProjects);
}

void RemoteSessionController::run()
{
    const RemoteClient client(bearerToken);
    const auto deviceId = juce::SystemStats::getUniqueDeviceID();

    auto checkIn = [&]() -> bool
    {
        juce::Array<RemoteClient::ProjectSummary> currentProjects;
        {
            juce::ScopedLock lock(projectsLock);
            currentProjects = projects;
        }
        const auto result = client.checkIn(deviceId, deviceName(), "0.0.1", currentProjects);
        if (threadShouldExit())
            return false;

        if (! result.success)
        {
            if (onCheckInFailed != nullptr)
            {
                const auto message = result.errorMessage;
                juce::MessageManager::callAsync([callback = onCheckInFailed, message] { callback(message); });
            }
            return false;
        }

        hostSessionId = result.hostSessionId;
        if (onCheckedIn != nullptr)
        {
            const auto presence = result.presenceState;
            juce::MessageManager::callAsync([callback = onCheckedIn, presence] { callback(presence); });
        }
        return true;
    };

    if (! checkIn())
        return;

    // Re-check-in periodically as a heartbeat (matches remote_host_sessions'
    // last_heartbeat_at, which the API/UI can use to judge presence), and
    // wake immediately on requestPairing() rather than waiting out the full
    // interval.
    constexpr int heartbeatIntervalMs = 30000;

    while (! threadShouldExit())
    {
        wakeEvent.wait(heartbeatIntervalMs);
        if (threadShouldExit())
            return;

        if (pairingRequested.exchange(false))
        {
            const auto pairing = client.createPairing(hostSessionId);
            if (threadShouldExit())
                return;

            if (! pairing.success)
            {
                if (onPairingFailed != nullptr)
                {
                    const auto message = pairing.errorMessage;
                    juce::MessageManager::callAsync([callback = onPairingFailed, message] { callback(message); });
                }
                continue;
            }

            pendingPairingId = pairing.pairingId;
            if (onPairingReady != nullptr)
            {
                const auto code = pairing.pairingCode;
                juce::MessageManager::callAsync([callback = onPairingReady, code] { callback(code); });
            }

            // Poll for approval every 2s, up to the pairing's own 10-minute
            // server-side expiry -- no WebSocket push relay yet (CR-M2/M4
            // follow-on work), so this is deliberately a poll loop.
            constexpr int pollIntervalMs = 2000;
            constexpr int maxPolls = 10 * 60 * 1000 / pollIntervalMs;
            for (int i = 0; i < maxPolls && ! threadShouldExit(); ++i)
            {
                wait(pollIntervalMs);
                if (threadShouldExit())
                    return;

                const auto status = client.getPairingStatus(pendingPairingId);
                if (status.success && status.status == "approved")
                {
                    if (onPairingApproved != nullptr)
                        juce::MessageManager::callAsync([callback = onPairingApproved] { callback(); });
                    break;
                }
            }
        }
        else
        {
            checkIn();
        }
    }
}
