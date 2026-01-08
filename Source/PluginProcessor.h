#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Grain.h"
#include "CircularBuffer.h"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor {
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)

    int currentSampleRate = 44100;

    CircularBuffer circularBuffer;
    int writePos = 0;
    int bufferSize = 1 << 18; // 6s at 44.1

    static constexpr int maxGrains = 32;
    Grain grainPool[maxGrains];

    // These will later be linked to APVTS
    float paramSpliceMs = 600.0f; // 0.1 to 2000 (ms)
    float paramDelayMs = 10.0f;    // 0.1 to 1000 (ms)
    float paramPitch = 2.0f;      // 0.5 to 2.0 (pitch scale)
    float paramDensity = 16.0f;   // 1 to 32 (# grains)
    float paramFeedback = 0.75f;  // 0.0 to 1.0 (%)
    float paramSpread = 0.5f;     // 0.0 to 1.0 (in seconds)
    bool  paramReverse = true;
    float paramMix = 0.60f;

    // DC blocker
    float hpfState = 0.0f;
    float lastFeedbackInput = 0.0f;

    int samplesUntilNextGrain;
    float lastOutput = 0.0f; // For feedback
};
