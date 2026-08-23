#include "QrCodeComponent.h"
#include <qrcodegen.hpp>

void QrCodeComponent::setPayload(const juce::String& text)
{
    const auto qr = qrcodegen::QrCode::encodeText(text.toRawUTF8(), qrcodegen::QrCode::Ecc::MEDIUM);
    moduleCount = qr.getSize();

    modulesPath.clear();
    for (int y = 0; y < moduleCount; ++y)
        for (int x = 0; x < moduleCount; ++x)
            if (qr.getModule(x, y))
                modulesPath.addRectangle((float) x, (float) y, 1.0f, 1.0f);

    repaint();
}

void QrCodeComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::white);

    if (moduleCount <= 0)
        return;

    // Quiet zone: 4-module white border is part of the QR spec, not optional
    // -- scanners rely on it to find the code's edges.
    constexpr int quietZoneModules = 4;
    const auto totalModules = (float) (moduleCount + quietZoneModules * 2);
    const auto scale = juce::jmin((float) getWidth(), (float) getHeight()) / totalModules;

    juce::Graphics::ScopedSaveState saveState(g);
    g.addTransform(juce::AffineTransform::scale(scale)
                        .translated((float) quietZoneModules * scale, (float) quietZoneModules * scale));
    g.setColour(juce::Colours::black);
    g.fillPath(modulesPath);
}
