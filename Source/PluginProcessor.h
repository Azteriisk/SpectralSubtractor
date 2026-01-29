#pragma once
#include <JuceHeader.h>

enum { fftOrder = 10, fftSize = 1 << fftOrder, hopSize = fftSize / 4 };

class SpectralSubtractorAudioProcessor : public juce::AudioProcessor
{
public:
    SpectralSubtractorAudioProcessor();
    ~SpectralSubtractorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Spectral Subtractor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    // --- SHARED UI VARIABLES ---
    std::atomic<bool> isLearning{ false };
    std::atomic<bool> showProcessed{ true };
    std::atomic<bool> bypass{ false };
    std::atomic<float> visualGain{ 1.0f };
    std::atomic<int> maxFreqIndex{ 20000 };
    std::atomic<int> colorTheme{ 0 };

    // VISUALIZATION ARRAYS
    // CRITICAL FIX: Added +1 to safety buffer to prevent off-by-one crashes
    float scopeDataRaw[fftSize / 2 + 1];
    float scopeDataProc[fftSize / 2 + 1];

    double getSampleRate() { return currentSampleRate; }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    int inputBufferWritePos = 0;
    double currentSampleRate = 44100.0;

    float fftWorkspace[fftSize * 2];
    float overlapBuffer[fftSize];

    float noiseProfile[fftSize / 2 + 1];
    int noiseLearnCounter = 0;
    bool hasNoiseProfile = false;

    float subtractionStrength = 2.0f;
    float spectralFloor = 0.05f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralSubtractorAudioProcessor)
};