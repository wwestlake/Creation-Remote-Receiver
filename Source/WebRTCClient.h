#pragma once

#include <JuceHeader.h>
#include <rtc/rtc.hpp>
#include <memory>
#include <creation/suite/SuiteSettings.h>

// Connects to djehuti's /ws/remote/signaling relay as the "host" role,
// negotiates a direct P2P connection to whichever phone is signaling for
// this host session, and exposes the resulting DataChannel. See
// docs/architecture/Creation-Remote-Protocol.md (Creation-Suite repo) §4.
//
// The signaling relay (rtc::WebSocket here) and the actual P2P connection
// (rtc::PeerConnection) are two separate libdatachannel objects -- the
// relay carries only small JSON offer/answer/ICE messages; once connected,
// the DataChannel talks directly to the phone, never through the server.
//
// All libdatachannel callbacks fire on its own internal thread(s); this
// class marshals everything back to the JUCE message thread via
// MessageManager::callAsync before touching any JUCE UI state, same
// convention as RemoteSessionController.
class WebRTCClient
{
public:
    WebRTCClient();
    ~WebRTCClient();

    // hostSessionId/bearerToken identify and authenticate this receiver to
    // the signaling relay (role=host).
    void connectSignaling(const juce::String& hostSessionId, const juce::String& bearerToken);
    void disconnect();

    std::function<void()> onPeerConnected;
    std::function<void()> onPeerDisconnected;
    std::function<void(const juce::String& /*message*/)> onError;
    std::function<void(const juce::String& /*displayName*/)> onAssetReceived;

private:
    void handleSignalingMessage(const juce::String& message);
    void createPeerConnection();

    // Protocol doc §5: a JSON text message (the asset header) is always
    // immediately followed by one binary message (the payload, or one chunk
    // of it). onDataChannelMessage demuxes by variant type and routes to
    // whichever of these two is expecting it next.
    void onDataChannelMessage(rtc::message_variant data);
    void handleIncomingHeader(const juce::var& header);
    void handleIncomingChunk(const void* data, size_t size);
    void depositAsset();

    std::shared_ptr<rtc::WebSocket> signalingSocket;
    std::shared_ptr<rtc::PeerConnection> peerConnection;
    std::shared_ptr<rtc::DataChannel> dataChannel;

    creation::suite::SuiteSettingsStore suiteSettingsStore;

    // Single in-flight receive state -- the protocol is discrete
    // capture-then-send, never concurrent overlapping transfers, so there is
    // only ever one asset (or one chunk of one asset) being reassembled.
    juce::var pendingHeader;
    juce::MemoryBlock receiveBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebRTCClient)
};
