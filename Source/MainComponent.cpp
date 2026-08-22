#include "MainComponent.h"
#include <creation/interop/ProjectRegistry.h>

MainComponent::MainComponent()
{
    titleLabel.setText("Creation Remote Receiver", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    statusLabel.setText("Not paired with any Creation Remote device yet.", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(15.0f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    projectCountLabel.setFont(juce::FontOptions(15.0f));
    projectCountLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(projectCountLabel);

    refreshProjectCount();
    setSize(640, 320);
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
        projectCountLabel.setText("Project discovery: " + registryError, juce::dontSendNotification);
    else
        projectCountLabel.setText(juce::String(projects.size()) + " project(s) available to receive into",
                                   juce::dontSendNotification);
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
}
