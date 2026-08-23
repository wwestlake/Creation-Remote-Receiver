#include "MainComponent.h"
#include <creation/interop/ProjectRegistry.h>

MainComponent::MainComponent()
{
    titleLabel.setText("Creation Remote Receiver", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    statusLabel.setFont(juce::FontOptions(15.0f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    projectCountLabel.setFont(juce::FontOptions(15.0f));
    projectCountLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(projectCountLabel);

    loginButton.onClick = [this] { authSession.beginLogin(); };
    addAndMakeVisible(loginButton);

    pairButton.onClick = [this] { beginPairing(); };
    pairButton.setVisible(false);
    addAndMakeVisible(pairButton);

    pairingCodeLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    pairingCodeLabel.setJustificationType(juce::Justification::centred);
    pairingCodeLabel.setVisible(false);
    addAndMakeVisible(pairingCodeLabel);

    qrCode.setVisible(false);
    addAndMakeVisible(qrCode);

    authSession.onStatusChanged = [this](const juce::String& text) { statusLabel.setText(text, juce::dontSendNotification); };
    authSession.onError = [this](const juce::String& text) { statusLabel.setText(text, juce::dontSendNotification); };
    authSession.onAuthenticated = [this](const creation::ui::SuiteDesktopAuthSession::SessionData&) { updateLoginUi(); };
    authSession.onSessionCleared = [this] { updateLoginUi(); };

    refreshProjectCount();
    updateLoginUi();
    setSize(640, 460);
}

MainComponent::~MainComponent()
{
    if (sessionController != nullptr)
        sessionController->stop();
}

void MainComponent::updateLoginUi()
{
    if (! authSession.hasValidSession())
    {
        statusLabel.setText("Not signed in.", juce::dontSendNotification);
        loginButton.setVisible(true);
        pairButton.setVisible(false);
        return;
    }

    loginButton.setVisible(false);
    statusLabel.setText("Signed in as " + authSession.getSession().user.email + " -- checking in...",
                         juce::dontSendNotification);

    sessionController = std::make_unique<RemoteSessionController>();

    sessionController->onCheckedIn = [this](const juce::String& presence)
    {
        statusLabel.setText("Checked in (" + presence + "). Not paired with any device yet.",
                             juce::dontSendNotification);
        pairButton.setVisible(true);
    };
    sessionController->onCheckInFailed = [this](const juce::String& message)
    {
        statusLabel.setText(message, juce::dontSendNotification);
    };
    sessionController->onPairingReady = [this](const juce::String& code)
    {
        pairingCodeLabel.setText(code, juce::dontSendNotification);
        pairingCodeLabel.setVisible(true);
        qrCode.setPayload("creationremote://pair?code=" + code);
        qrCode.setVisible(true);
        statusLabel.setText("Scan this in the Creation Remote app -- expires in 10 minutes.",
                             juce::dontSendNotification);
        resized();
    };
    sessionController->onPairingFailed = [this](const juce::String& message)
    {
        statusLabel.setText(message, juce::dontSendNotification);
    };
    sessionController->onPairingApproved = [this]
    {
        statusLabel.setText("Device paired -- negotiating a direct connection...", juce::dontSendNotification);
        qrCode.setVisible(false);
        pairingCodeLabel.setVisible(false);
        pairButton.setVisible(true);
        resized();

        webRtcClient.connectSignaling(sessionController->getHostSessionId(), authSession.getSession().token);
    };
    webRtcClient.onPeerConnected = [this]
    {
        statusLabel.setText("Direct connection established.", juce::dontSendNotification);
    };
    webRtcClient.onPeerDisconnected = [this]
    {
        statusLabel.setText("Device paired.", juce::dontSendNotification);
    };
    webRtcClient.onError = [this](const juce::String& message)
    {
        statusLabel.setText("Signaling error: " + message, juce::dontSendNotification);
    };
    webRtcClient.onAssetReceived = [this](const juce::String& displayName)
    {
        statusLabel.setText("Received: " + displayName, juce::dontSendNotification);
        refreshProjectCount();
    };

    sessionController->start(authSession.getSession().token);
    refreshProjectCount(); // seeds sessionController's project list before the first heartbeat
}

void MainComponent::beginPairing()
{
    if (sessionController == nullptr)
        return;

    pairButton.setVisible(false);
    statusLabel.setText("Generating pairing code...", juce::dontSendNotification);
    sessionController->requestPairing();
}

void MainComponent::refreshProjectCount()
{
    juce::String settingsError;
    const auto settings = suiteSettingsStore.load(settingsError);

    if (settingsError.isNotEmpty())
    {
        projectCountLabel.setText("Suite settings: " + settingsError, juce::dontSendNotification);
        return;
    }

    juce::String registryError;
    const auto projects = creation::interop::ProjectRegistry::discoverProjects(settings, registryError);

    if (registryError.isNotEmpty())
    {
        projectCountLabel.setText("Project discovery: " + registryError, juce::dontSendNotification);
        return;
    }

    projectCountLabel.setText(juce::String(projects.size()) + " project(s) available to receive into",
                               juce::dontSendNotification);

    if (sessionController != nullptr)
    {
        juce::Array<RemoteClient::ProjectSummary> summaries;
        for (const auto& project : projects)
            summaries.add({ project.projectId, project.manifest.projectName });
        sessionController->setProjects(std::move(summaries));
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10161f));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(24);
    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(12);
    statusLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    projectCountLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(16);
    loginButton.setBounds(area.removeFromTop(32).withWidth(140));
    pairButton.setBounds(area.getX(), area.getY(), 180, 32);
    area.removeFromTop(40);

    if (qrCode.isVisible())
    {
        constexpr int qrSize = 220;
        qrCode.setBounds(area.getCentreX() - qrSize / 2, area.getY(), qrSize, qrSize);
        area.removeFromTop(qrSize + 12);
    }
    if (pairingCodeLabel.isVisible())
        pairingCodeLabel.setBounds(area.removeFromTop(28));
}
