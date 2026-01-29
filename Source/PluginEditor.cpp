#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectralSubtractorAudioProcessorEditor::SpectralSubtractorAudioProcessorEditor(SpectralSubtractorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(900, 500);
    spectrogramImage = juce::Image(juce::Image::RGB, 900, 400, true);
    startTimerHz(30);

    // --- BUTTONS ---
    addAndMakeVisible(learnButton);
    learnButton.onClick = [this] {
        audioProcessor.isLearning = !audioProcessor.isLearning;
        learnButton.setButtonText(audioProcessor.isLearning ? "STOP LEARNING" : "LEARN NOISE");
        learnButton.setColour(juce::TextButton::buttonColourId,
            audioProcessor.isLearning ? juce::Colours::red : juce::Colours::darkgrey);
        };

    addAndMakeVisible(showProcButton);
    showProcButton.setToggleState(true, juce::dontSendNotification);
    showProcButton.onClick = [this] { audioProcessor.showProcessed = showProcButton.getToggleState(); };

    addAndMakeVisible(bypassButton);
    bypassButton.onClick = [this] { audioProcessor.bypass = bypassButton.getToggleState(); };

    // --- GAIN SLIDER ---
    addAndMakeVisible(gainSlider);
    gainSlider.setRange(1.0, 10.0, 0.1);
    gainSlider.setValue(1.0);
    gainSlider.onValueChange = [this] { audioProcessor.visualGain = (float)gainSlider.getValue(); };

    addAndMakeVisible(gainLabel);
    gainLabel.setText("Visual Boost", juce::dontSendNotification);
    gainLabel.attachToComponent(&gainSlider, false);

    // --- FREQ ZOOM ---
    addAndMakeVisible(freqRangeBox);
    freqRangeBox.addItem("Full Range (22kHz)", 1);
    freqRangeBox.addItem("Speech (8kHz)", 2);
    freqRangeBox.addItem("Mids (4kHz)", 3);
    freqRangeBox.addItem("Lows (1kHz)", 4);
    freqRangeBox.setSelectedId(1);
    freqRangeBox.onChange = [this] {
        switch (freqRangeBox.getSelectedId()) {
        case 1: audioProcessor.maxFreqIndex = 22000; break;
        case 2: audioProcessor.maxFreqIndex = 8000; break;
        case 3: audioProcessor.maxFreqIndex = 4000; break;
        case 4: audioProcessor.maxFreqIndex = 1000; break;
        }
        };

    // --- NEW: THEME SELECTOR ---
    addAndMakeVisible(themeBox);
    themeBox.addItem("Theme: Fire", 1);
    themeBox.addItem("Theme: Ocean", 2);
    themeBox.addItem("Theme: Matrix", 3);
    themeBox.addItem("Theme: Grayscale", 4);
    themeBox.setSelectedId(1);
    themeBox.onChange = [this] {
        // Map ID 1-4 to Index 0-3
        audioProcessor.colorTheme = themeBox.getSelectedId() - 1;
        };
}

SpectralSubtractorAudioProcessorEditor::~SpectralSubtractorAudioProcessorEditor()
{
    stopTimer();
}

// --- COLOR ENGINE ---
juce::Colour SpectralSubtractorAudioProcessorEditor::getColourForLevel(float magnitude)
{
    float boosted = magnitude * audioProcessor.visualGain;
    float db = juce::Decibels::gainToDecibels(boosted);
    float level = juce::jmap(db, -90.0f, 0.0f, 0.0f, 1.0f);
    level = juce::jlimit(0.0f, 1.0f, level);

    int theme = audioProcessor.colorTheme;

    // 0: FIRE (Default)
    if (theme == 0) {
        if (level < 0.2f) return juce::Colours::black;
        if (level < 0.4f) return juce::Colours::purple.interpolatedWith(juce::Colours::black, 1.0f - (level - 0.2f) * 5.0f);
        if (level < 0.6f) return juce::Colours::darkred.interpolatedWith(juce::Colours::purple, 1.0f - (level - 0.4f) * 5.0f);
        if (level < 0.85f) return juce::Colours::orange.interpolatedWith(juce::Colours::darkred, 1.0f - (level - 0.6f) * 4.0f);
        return juce::Colours::white.interpolatedWith(juce::Colours::orange, 1.0f - (level - 0.85f) * 6.6f);
    }

    // 1: OCEAN (Blue/Cyan)
    if (theme == 1) {
        if (level < 0.1f) return juce::Colours::black;
        if (level < 0.4f) return juce::Colours::darkblue.interpolatedWith(juce::Colours::black, 1.0f - (level - 0.1f) * 3.3f);
        if (level < 0.7f) return juce::Colours::cyan.interpolatedWith(juce::Colours::darkblue, 1.0f - (level - 0.4f) * 3.3f);
        return juce::Colours::white.interpolatedWith(juce::Colours::cyan, 1.0f - (level - 0.7f) * 3.3f);
    }

    // 2: MATRIX (Green)
    if (theme == 2) {
        if (level < 0.1f) return juce::Colours::black;
        if (level < 0.5f) return juce::Colours::darkgreen.interpolatedWith(juce::Colours::black, 1.0f - (level - 0.1f) * 2.5f);
        if (level < 0.8f) return juce::Colours::lime.interpolatedWith(juce::Colours::darkgreen, 1.0f - (level - 0.5f) * 3.3f);
        return juce::Colours::white.interpolatedWith(juce::Colours::lime, 1.0f - (level - 0.8f) * 5.0f);
    }

    // 3: GRAYSCALE (Scientific)
    if (theme == 3) {
        // Simple luminance ramp
        uint8_t lum = (uint8_t)(level * 255.0f);
        return juce::Colour(lum, lum, lum);
    }

    return juce::Colours::black;
}

void SpectralSubtractorAudioProcessorEditor::timerCallback()
{
    if (spectrogramImage.isNull()) return;

    int scrollSpeed = 2;
    int w = spectrogramImage.getWidth();
    int h = spectrogramImage.getHeight();
    spectrogramImage.moveImageSection(0, 0, scrollSpeed, 0, w - scrollSpeed, h);

    float* dataToDraw = audioProcessor.showProcessed ? audioProcessor.scopeDataProc : audioProcessor.scopeDataRaw;

    double nyquist = audioProcessor.getSampleRate() / 2.0;
    int maxBin = (int)((audioProcessor.maxFreqIndex / nyquist) * (fftSize / 2));
    maxBin = juce::jlimit(10, fftSize / 2, maxBin);

    juce::Image::BitmapData pixelData(spectrogramImage, juce::Image::BitmapData::readWrite);

    for (int xOffset = 0; xOffset < scrollSpeed; ++xOffset)
    {
        int x = w - scrollSpeed + xOffset;

        for (int y = 0; y < h; ++y)
        {
            int binIndex = juce::jmap(h - y, 0, h, 0, maxBin);

            if (binIndex >= 0 && binIndex < fftSize / 2)
            {
                juce::Colour col = getColourForLevel(dataToDraw[binIndex]);
                spectrogramImage.setPixelAt(x, y, col);
            }
        }
    }
    repaint();
}

void SpectralSubtractorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.drawImageAt(spectrogramImage, 0, 0);
    g.setColour(juce::Colours::white);
    g.drawRect(0, 0, getWidth(), 400, 1);

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText(juce::String(audioProcessor.maxFreqIndex) + " Hz", getWidth() - 80, 5, 70, 20, juce::Justification::right);
    g.drawText("0 Hz", getWidth() - 80, 380, 70, 20, juce::Justification::right);
}

void SpectralSubtractorAudioProcessorEditor::resized()
{
    if (spectrogramImage.getWidth() != getWidth()) {
        spectrogramImage = juce::Image(juce::Image::RGB, getWidth(), 400, true);
        spectrogramImage.clear(spectrogramImage.getBounds(), juce::Colours::black);
    }

    int y = 420;
    // Row 1
    learnButton.setBounds(10, y, 110, 30);
    showProcButton.setBounds(130, y, 110, 30);
    bypassButton.setBounds(250, y, 110, 30);

    // Row 2 (Right side)
    gainSlider.setBounds(380, y, 180, 30);
    freqRangeBox.setBounds(570, y, 150, 30);
    themeBox.setBounds(730, y, 150, 30); // Theme box at the far right
}