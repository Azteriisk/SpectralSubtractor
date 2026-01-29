#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SpectralSubtractorAudioProcessor::SpectralSubtractorAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    ),
    fft(fftOrder),
    window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    // SAFETY INIT: Resize immediately so we don't crash if prepareToPlay is skipped
    inputBuffer.resize(fftSize * 2, 0.0f);
    outputBuffer.resize(fftSize * 2, 0.0f);
}

SpectralSubtractorAudioProcessor::~SpectralSubtractorAudioProcessor() {}

//==============================================================================
void SpectralSubtractorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    inputBuffer.resize(fftSize * 2, 0.0f);
    outputBuffer.resize(fftSize * 2, 0.0f);
    juce::zeromem(noiseProfile, sizeof(noiseProfile));
    juce::zeromem(overlapBuffer, sizeof(overlapBuffer));

    // Clear Visualizer buffers too to prevent startup garbage
    juce::zeromem(scopeDataRaw, sizeof(scopeDataRaw));
    juce::zeromem(scopeDataProc, sizeof(scopeDataProc));

    inputBufferWritePos = 0;
    noiseLearnCounter = 0;
    hasNoiseProfile = false;
    currentSampleRate = sampleRate;
}

void SpectralSubtractorAudioProcessor::releaseResources() {}

bool SpectralSubtractorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

//==============================================================================
void SpectralSubtractorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto* channelData = buffer.getWritePointer(0);
    int numSamples = buffer.getNumSamples();

    // CRASH FIX 1: If buffer isn't ready, do nothing.
    if (inputBuffer.size() < fftSize * 2) return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        inputBuffer[inputBufferWritePos] = channelData[sample];
        inputBufferWritePos++;

        if (inputBufferWritePos >= hopSize)
        {
            // --- COPY & SHIFT ---
            for (int i = 0; i < fftSize; ++i) fftWorkspace[i] = inputBuffer[i];

            // CRASH FIX 2: Safer unsigned integer math for buffer shifting
            if (inputBuffer.size() > hopSize) {
                int remaining = (int)inputBuffer.size() - hopSize;
                for (int i = 0; i < remaining; ++i) inputBuffer[i] = inputBuffer[i + hopSize];
            }
            inputBufferWritePos -= hopSize;

            // --- FFT ---
            window.multiplyWithWindowingTable(fftWorkspace, fftSize);
            fft.performRealOnlyForwardTransform(fftWorkspace);

            // --- PROCESS ---
            // CRASH FIX 3: Changed "<=" to "<". 
            // We strictly iterate UP TO fftSize/2. Index fftSize/2 is out of bounds for some arrays.
            for (int k = 0; k < fftSize / 2; ++k)
            {
                float real = fftWorkspace[2 * k];
                float imag = fftWorkspace[2 * k + 1];
                float magnitude = std::sqrt(real * real + imag * imag);
                float phase = std::atan2(imag, real);

                // VISUALIZATION INPUT
                scopeDataRaw[k] = (scopeDataRaw[k] * 0.8f) + (magnitude * 0.2f);

                float newMag = magnitude;

                if (isLearning)
                {
                    noiseProfile[k] += magnitude;
                }
                else if (hasNoiseProfile && !bypass)
                {
                    float floorVal = noiseProfile[k] * spectralFloor;
                    float subtracted = magnitude - (noiseProfile[k] * subtractionStrength);
                    newMag = std::max(subtracted, floorVal);
                }

                // VISUALIZATION OUTPUT
                scopeDataProc[k] = (scopeDataProc[k] * 0.8f) + (newMag * 0.2f);

                // RECONSTRUCT
                fftWorkspace[2 * k] = newMag * std::cos(phase);
                fftWorkspace[2 * k + 1] = newMag * std::sin(phase);
            }

            if (isLearning) noiseLearnCounter++;

            // --- IFFT ---
            fft.performRealOnlyInverseTransform(fftWorkspace);
            window.multiplyWithWindowingTable(fftWorkspace, fftSize);

            for (int j = 0; j < fftSize; ++j)
            {
                float scaling = 1.0f / (float)fftSize;
                scaling *= 1.5f;
                overlapBuffer[j] += fftWorkspace[j] * scaling;
            }
        }

        float outSample = overlapBuffer[0];
        for (int i = 0; i < fftSize - 1; ++i) overlapBuffer[i] = overlapBuffer[i + 1];
        overlapBuffer[fftSize - 1] = 0.0f;

        channelData[sample] = outSample;
    }

    if (!isLearning && noiseLearnCounter > 0)
    {
        // Safety: Use strictly "<" here too
        for (int k = 0; k < fftSize / 2; ++k)
            noiseProfile[k] /= (float)noiseLearnCounter;
        noiseLearnCounter = 0;
        hasNoiseProfile = true;
    }

    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* SpectralSubtractorAudioProcessor::createEditor()
{
    return new SpectralSubtractorAudioProcessorEditor(*this);
}

void SpectralSubtractorAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {}
void SpectralSubtractorAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralSubtractorAudioProcessor();
}