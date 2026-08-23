#pragma once

#include <JuceHeader.h>
#include <rtc/rtc.hpp>
#include <memory>

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

private:
    void handleSignalingMessage(const juce::String& message);
    void createPeerConnection();

    std::shared_ptr<rtc::WebSocket> signalingSocket;
    std::shared_ptr<rtc::PeerConnection> peerConnection;
    std::shared_ptr<rtc::DataChannel> dataChannel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebRTCClient)
};
