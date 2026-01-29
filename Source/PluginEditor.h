#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpectralSubtractorAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    SpectralSubtractorAudioProcessorEditor(SpectralSubtractorAudioProcessor&);
    ~SpectralSubtractorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SpectralSubtractorAudioProcessor& audioProcessor;

    // --- CONTROLS ---
    juce::TextButton learnButton{ "LEARN NOISE" };
    juce::ToggleButton showProcButton{ "View Filtered" };
    juce::ToggleButton bypassButton{ "Bypass Effect" };

    // Sliders & Labels
    juce::Slider gainSlider;
    juce::Label gainLabel;

    // Dropdowns
    juce::ComboBox freqRangeBox;
    juce::ComboBox themeBox; // NEW: Color Selector

    // --- SPECTROGRAM ---
    juce::Image spectrogramImage;
    juce::Colour getColourForLevel(float magnitude);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralSubtractorAudioProcessorEditor)
};