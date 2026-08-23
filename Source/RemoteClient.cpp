#include "RemoteClient.h"

RemoteClient::RemoteClient(juce::String bearerTokenToUse) : bearerToken(std::move(bearerTokenToUse)) {}

juce::var RemoteClient::postJson(const juce::String& path, const juce::var& body, int& statusCode) const
{
    const auto bodyText = body.isVoid() ? juce::String() : juce::JSON::toString(body);
    auto url = juce::URL(apiBase() + path).withPOSTData(bodyText);

    const auto headers = "Authorization: Bearer " + bearerToken
                        + "\r\nContent-Type: application/json\r\nAccept: application/json\r\n";

    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(15000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(headers));
    if (stream == nullptr)
        return {};

    return juce::JSON::parse(stream->readEntireStreamAsString());
}

juce::var RemoteClient::getJson(const juce::String& path, int& statusCode) const
{
    auto url = juce::URL(apiBase() + path);
    const auto headers = "Authorization: Bearer " + bearerToken + "\r\nAccept: application/json\r\n";

    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                            .withHttpRequestCmd("GET")
                                            .withConnectionTimeoutMs(15000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(headers));
    if (stream == nullptr)
        return {};

    return juce::JSON::parse(stream->readEntireStreamAsString());
}

RemoteClient::CheckInResult RemoteClient::checkIn(const juce::String& deviceId, const juce::String& deviceName,
                                                   const juce::String& appVersion,
                                                   const juce::Array<ProjectSummary>& projects) const
{
    juce::Array<juce::var> projectsJson;
    for (const auto& project : projects)
    {
        auto projectObject = juce::DynamicObject::Ptr(new juce::DynamicObject);
        projectObject->setProperty("projectId", project.projectId);
        projectObject->setProperty("displayName", project.displayName);
        projectsJson.add(juce::var(projectObject.get()));
    }

    auto bodyObject = juce::DynamicObject::Ptr(new juce::DynamicObject);
    bodyObject->setProperty("productSlug", "creation-remote-receiver");
    bodyObject->setProperty("appId", "com.lagdaemon.creationremotereceiver");
    bodyObject->setProperty("appVersion", appVersion);
    bodyObject->setProperty("deviceId", deviceId);
    bodyObject->setProperty("deviceName", deviceName);
    bodyObject->setProperty("agentAvailable", true);
    bodyObject->setProperty("controlPanelAvailable", false);
    bodyObject->setProperty("capabilities", juce::Array<juce::var>());
    bodyObject->setProperty("projects", projectsJson);

    int statusCode = 0;
    const auto response = postJson("/api/remote/host-sessions/check-in", juce::var(bodyObject.get()), statusCode);

    CheckInResult result;
    if (statusCode < 200 || statusCode >= 300 || response.isVoid())
    {
        result.errorMessage = "Check-in failed (HTTP " + juce::String(statusCode) + ")";
        return result;
    }

    result.success = true;
    result.hostSessionId = response.getProperty("hostSessionId", {}).toString();
    result.presenceState = response.getProperty("presenceState", {}).toString();
    return result;
}

RemoteClient::PairingResult RemoteClient::createPairing(const juce::String& hostSessionId) const
{
    auto bodyObject = juce::DynamicObject::Ptr(new juce::DynamicObject);
    bodyObject->setProperty("hostSessionId", hostSessionId);

    int statusCode = 0;
    const auto response = postJson("/api/remote/pairings", juce::var(bodyObject.get()), statusCode);

    PairingResult result;
    if (statusCode < 200 || statusCode >= 300 || response.isVoid())
    {
        result.errorMessage = "Could not create a pairing code (HTTP " + juce::String(statusCode) + ")";
        return result;
    }

    result.success = true;
    result.pairingId = response.getProperty("pairingId", {}).toString();
    result.pairingCode = response.getProperty("pairingCode", {}).toString();
    return result;
}

RemoteClient::PairingStatusResult RemoteClient::getPairingStatus(const juce::String& pairingId) const
{
    int statusCode = 0;
    const auto response = getJson("/api/remote/pairings/" + pairingId + "/status", statusCode);

    PairingStatusResult result;
    if (statusCode < 200 || statusCode >= 300 || response.isVoid())
    {
        result.errorMessage = "Could not check pairing status (HTTP " + juce::String(statusCode) + ")";
        return result;
    }

    result.success = true;
    result.status = response.getProperty("status", {}).toString();
    return result;
}
