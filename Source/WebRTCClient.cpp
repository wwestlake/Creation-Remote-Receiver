#include "WebRTCClient.h"

namespace
{
juce::String signalingUrl(const juce::String& hostSessionId, const juce::String& token)
{
    return "wss://lagdaemon.com/djehuti/ws/remote/signaling?hostSessionId="
         + juce::URL::addEscapeChars(hostSessionId, true)
         + "&role=host&token=" + juce::URL::addEscapeChars(token, true);
}
}

WebRTCClient::WebRTCClient() = default;

WebRTCClient::~WebRTCClient()
{
    disconnect();
}

void WebRTCClient::createPeerConnection()
{
    rtc::Configuration config;
    // No TURN, on purpose -- if a direct connection can't be negotiated via
    // STUN, that's a signaling failure the caller treats the same as
    // "receiver unreachable" (phone queues locally). See the umbrella
    // AGENTS.md-linked protocol doc §1/§4: no relay of any kind, ever.
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");

    peerConnection = std::make_shared<rtc::PeerConnection>(config);

    peerConnection->onLocalDescription([this](rtc::Description description)
    {
        auto bodyObject = juce::DynamicObject::Ptr(new juce::DynamicObject);
        bodyObject->setProperty("type", juce::String(description.typeString()));
        bodyObject->setProperty("sdp", juce::String(std::string(description)));
        const auto json = juce::JSON::toString(juce::var(bodyObject.get()));

        if (signalingSocket != nullptr && signalingSocket->isOpen())
            signalingSocket->send(json.toStdString());
    });

    peerConnection->onLocalCandidate([this](rtc::Candidate candidate)
    {
        auto bodyObject = juce::DynamicObject::Ptr(new juce::DynamicObject);
        bodyObject->setProperty("type", "ice");
        bodyObject->setProperty("candidate", juce::String(std::string(candidate)));
        bodyObject->setProperty("sdpMid", juce::String(candidate.mid()));
        const auto json = juce::JSON::toString(juce::var(bodyObject.get()));

        if (signalingSocket != nullptr && signalingSocket->isOpen())
            signalingSocket->send(json.toStdString());
    });

    peerConnection->onStateChange([this](rtc::PeerConnection::State state)
    {
        if (state == rtc::PeerConnection::State::Connected)
        {
            if (onPeerConnected != nullptr)
                juce::MessageManager::callAsync([callback = onPeerConnected] { callback(); });
        }
        else if (state == rtc::PeerConnection::State::Disconnected
               || state == rtc::PeerConnection::State::Failed
               || state == rtc::PeerConnection::State::Closed)
        {
            if (onPeerDisconnected != nullptr)
                juce::MessageManager::callAsync([callback = onPeerDisconnected] { callback(); });
        }
    });

    // The phone is expected to be the one creating the DataChannel (it
    // drives the send-a-file flow); the receiver just needs to catch it.
    peerConnection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel)
    {
        dataChannel = std::move(channel);
        // Message handling wired up here in a later pass, once the asset
        // payload framing (protocol doc §5) is implemented -- this
        // milestone is P2P connectivity, not the transfer itself yet.
    });
}

void WebRTCClient::connectSignaling(const juce::String& hostSessionId, const juce::String& bearerToken)
{
    createPeerConnection();

    signalingSocket = std::make_shared<rtc::WebSocket>();

    signalingSocket->onMessage([this](rtc::message_variant data)
    {
        if (! std::holds_alternative<std::string>(data))
            return;

        const auto message = juce::String(std::get<std::string>(data));
        juce::MessageManager::callAsync([this, message] { handleSignalingMessage(message); });
    });

    signalingSocket->onError([this](std::string error)
    {
        const auto message = juce::String(error);
        if (onError != nullptr)
            juce::MessageManager::callAsync([callback = onError, message] { callback(message); });
    });

    signalingSocket->open(signalingUrl(hostSessionId, bearerToken).toStdString());
}

void WebRTCClient::handleSignalingMessage(const juce::String& message)
{
    const auto parsed = juce::JSON::parse(message);
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return;

    const auto type = object->getProperty("type").toString();

    if (type == "offer")
    {
        const auto sdp = object->getProperty("sdp").toString();
        peerConnection->setRemoteDescription(rtc::Description(sdp.toStdString(), "offer"));
        // libdatachannel auto-generates and fires onLocalDescription with
        // the answer once setRemoteDescription sees an offer -- no
        // separate "create answer" call needed.
    }
    else if (type == "ice")
    {
        const auto candidate = object->getProperty("candidate").toString();
        const auto sdpMid = object->getProperty("sdpMid").toString();
        peerConnection->addRemoteCandidate(rtc::Candidate(candidate.toStdString(), sdpMid.toStdString()));
    }
}

void WebRTCClient::disconnect()
{
    if (signalingSocket != nullptr)
    {
        signalingSocket->close();
        signalingSocket.reset();
    }
    dataChannel.reset();
    peerConnection.reset();
}
