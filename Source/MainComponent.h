#pragma once

#include <JuceHeader.h>
#include <creation/suite/SuiteSettings.h>

// Status view for the Suite Remote Receiver. This app has no standing open
// project and no creative-editing UI -- see AGENTS.md. What's here today is
// a scaffold: it discovers what projects exist (real, via the same
// ProjectRegistry every other app uses) so there's something true on screen
// while it's idle, but pairing (CR-M2/M4) and the actual receive path are
// not wired up yet.
class MainComponent final : public juce::Component
{
public:
    MainComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshProjectCount();

    creation::suite::SuiteSettingsStore suiteSettingsStore;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label projectCountLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
