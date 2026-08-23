#pragma once

#include <JuceHeader.h>

// Renders a payload string as a scannable QR code. Uses the vendored
// Nayuki qrcodegen library (ThirdParty/qrcodegen, MIT license) -- no
// existing QR generator lived anywhere in the suite before this.
class QrCodeComponent final : public juce::Component
{
public:
    void setPayload(const juce::String& text);
    void paint(juce::Graphics& g) override;

private:
    juce::Path modulesPath;
    int moduleCount = 0;
};
