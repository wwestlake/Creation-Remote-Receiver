#include "WebRTCClient.h"
#include <creation/assets/ProjectSession.h>
#include <creation/assets/ProjectAssetService.h>

namespace
{
juce::String signalingUrl(const juce::String& hostSessionId, const juce::String& token)
{
    return "wss://lagdaemon.com/djehuti/ws/remote/signaling?hostSessionId="
         + juce::URL::addEscapeChars(hostSessionId, true)
         + "&role=host&token=" + juce::URL::addEscapeChars(token, true);
}

// Protocol doc §5's "mediaType" is the MIME type the phone captured; the
// receiver only needs it to pick a sane file extension for the deposited
// asset -- everything else about how it's stored is up to the project.
juce::String extensionForMediaType(const juce::String& mediaType)
{
    if (mediaType == "image/jpeg") return ".jpg";
    if (mediaType == "image/png") return ".png";
    if (mediaType == "video/mp4") return ".mp4";
    if (mediaType == "audio/mp4" || mediaType == "audio/m4a") return ".m4a";
    if (mediaType == "audio/wav" || mediaType == "audio/wave") return ".wav";
    if (mediaType == "audio/3gpp") return ".3gp";
    return ".bin";
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
        dataChannel->onMessage([this](rtc::message_variant data) { onDataChannelMessage(std::move(data)); });
    });
}

void WebRTCClient::onDataChannelMessage(rtc::message_variant data)
{
    if (std::holds_alternative<std::string>(data))
    {
        const auto text = juce::String(std::get<std::string>(data));
        juce::MessageManager::callAsync([this, text]
        {
            const auto parsed = juce::JSON::parse(text);
            if (parsed.getDynamicObject() != nullptr)
                handleIncomingHeader(parsed);
        });
    }
    else
    {
        // rtc::binary (std::vector<std::byte>) belongs to this callback's
        // stack frame on libdatachannel's own thread -- copy it before
        // marshaling to the message thread, same convention as everywhere
        // else this class crosses threads.
        const auto& bytes = std::get<rtc::binary>(data);
        auto copy = std::make_shared<juce::MemoryBlock>(bytes.data(), bytes.size());
        juce::MessageManager::callAsync([this, copy] { handleIncomingChunk(copy->getData(), copy->getSize()); });
    }
}

void WebRTCClient::handleIncomingHeader(const juce::var& header)
{
    const auto* object = header.getDynamicObject();
    // No totalChunks field, or chunkIndex 0, means this header starts a
    // fresh transfer -- reset any leftover state from a prior asset.
    const bool startsNewTransfer = object == nullptr
                                 || ! object->hasProperty("totalChunks")
                                 || (int) object->getProperty("chunkIndex") == 0;
    if (startsNewTransfer)
        receiveBuffer.reset();

    pendingHeader = header;
}

void WebRTCClient::handleIncomingChunk(const void* data, size_t size)
{
    auto* header = pendingHeader.getDynamicObject();
    if (header == nullptr)
        return; // binary arrived with no preceding header -- nothing to attach it to

    const bool isChunked = header->hasProperty("totalChunks");

    if (isChunked)
    {
        const auto expectedChunkSha = header->getProperty("chunkSha256").toString();
        const juce::SHA256 chunkHash(data, size);
        if (expectedChunkSha.isNotEmpty() && chunkHash.toHexString() != expectedChunkSha)
        {
            if (onError != nullptr)
                onError("Asset chunk failed checksum verification -- transfer dropped.");
            pendingHeader = juce::var();
            receiveBuffer.reset();
            return;
        }
    }

    receiveBuffer.append(data, size);

    const bool isFinal = ! isChunked || (bool) header->getProperty("isFinalChunk");
    if (! isFinal)
        return; // wait for the rest of the chunks before verifying/depositing

    const auto expectedSha = header->getProperty("sha256").toString();
    const juce::SHA256 fullHash(receiveBuffer.getData(), receiveBuffer.getSize());
    if (expectedSha.isNotEmpty() && fullHash.toHexString() != expectedSha)
    {
        if (onError != nullptr)
            onError("Received asset failed checksum verification -- discarded.");
        pendingHeader = juce::var();
        receiveBuffer.reset();
        return;
    }

    depositAsset();
    pendingHeader = juce::var();
    receiveBuffer.reset();
}

void WebRTCClient::depositAsset()
{
    auto* header = pendingHeader.getDynamicObject();
    if (header == nullptr)
        return;

    const auto projectId = header->getProperty("projectId").toString();
    const auto sessionText = header->getProperty("sessionText").toString();
    const auto kind = header->getProperty("kind").toString();
    const auto mediaType = header->getProperty("mediaType").toString();
    const auto capturedAtUtc = header->getProperty("capturedAtUtc").toString();

    if (projectId.isEmpty())
    {
        if (onError != nullptr)
            onError("Received asset had no projectId -- discarded.");
        return;
    }

    juce::String settingsError;
    const auto settings = suiteSettingsStore.load(settingsError);
    if (settingsError.isNotEmpty())
    {
        if (onError != nullptr)
            onError("Could not load suite settings: " + settingsError);
        return;
    }

    creation::assets::ProjectSession session;
    juce::String sessionError;
    if (! creation::assets::ProjectSession::open(settings, projectId, session, sessionError))
    {
        if (onError != nullptr)
            onError("Could not open project " + projectId + ": " + sessionError);
        return;
    }

    const auto rawTimestamp = capturedAtUtc.isNotEmpty() ? capturedAtUtc : juce::Time::getCurrentTime().toISO8601(true);
    const auto safeTimestamp = rawTimestamp.replaceCharacter(':', '-');
    const auto shortId = juce::Uuid().toString().substring(0, 8);
    const auto folder = kind.isNotEmpty() ? kind : juce::String("asset");
    const auto logicalPath = "remote/" + folder + "/" + safeTimestamp + "_" + shortId + extensionForMediaType(mediaType);

    creation::assets::ProjectAssetService::ImportOptions options;
    options.kind = kind == "audio" ? creation::assets::AssetKind::audio : creation::assets::AssetKind::binary;
    options.displayName = sessionText.isNotEmpty() ? sessionText : logicalPath.fromLastOccurrenceOf("/", false, false);
    options.logicalPath = logicalPath;
    options.category = "Creation Remote";
    options.description = sessionText;
    options.mediaType = mediaType;
    options.sourceApp = "Creation Remote";
    options.sourceTool = "Creation Remote Receiver";

    creation::assets::AssetDescriptor descriptor;
    juce::String depositError;
    if (! creation::assets::ProjectAssetService::saveGeneratedAsset(session, receiveBuffer, options, descriptor, depositError))
    {
        if (onError != nullptr)
            onError("Could not save received asset: " + depositError);
        return;
    }

    juce::String commitError;
    if (! session.commit(commitError))
    {
        if (onError != nullptr)
            onError("Received asset saved but project commit failed: " + commitError);
        return;
    }

    if (onAssetReceived != nullptr)
        onAssetReceived(options.displayName);
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
