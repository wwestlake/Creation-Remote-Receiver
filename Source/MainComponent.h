#pragma once

#include <JuceHeader.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/ui/SuiteDesktopAuthSession.h>
#include "RemoteSessionController.h"
#include "QrCodeComponent.h"
#include "WebRTCClient.h"

// Status view for the Suite Remote Receiver. No standing open project, no
// creative-editing UI -- see AGENTS.md. Login reuses the suite-wide
// SuiteDesktopAuthSession (shared across every app via the VFS service), so
// this app doesn't need its own separate account UI if the user is already
// signed in anywhere else in the suite.
class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshProjectCount();
    void updateLoginUi();
    void beginPairing();

    creation::suite::SuiteSettingsStore suiteSettingsStore;
    creation::ui::SuiteDesktopAuthSession authSession { "creation-remote-receiver" };
    std::unique_ptr<RemoteSessionController> sessionController;
    WebRTCClient webRtcClient;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label projectCountLabel;
    juce::TextButton loginButton { "Sign In" };
    juce::TextButton pairButton { "Pair New Device" };
    QrCodeComponent qrCode;
    juce::Label pairingCodeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
