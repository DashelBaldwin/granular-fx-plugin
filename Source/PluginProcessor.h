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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)

    int currentSampleRate = 44100;

    CircularBuffer circularBuffer;
    int writePos = 0;
    int bufferSize = 1 << 18; // 6s at 44.1

    static constexpr int maxGrains = 32;
    Grain grainPool[maxGrains];

    int samplesUntilNextGrain;

    juce::LinearSmoothedValue<float> paramSpliceMs;
    juce::LinearSmoothedValue<float> paramDelayMs;
    juce::LinearSmoothedValue<float> paramDensity;
    juce::LinearSmoothedValue<float> paramPitch;
    juce::LinearSmoothedValue<float> paramSpread;
    juce::LinearSmoothedValue<float> paramFeedback;
    juce::LinearSmoothedValue<float> paramWidth;
    juce::LinearSmoothedValue<float> paramTone;
    bool paramReverse = true;
    juce::LinearSmoothedValue<float> paramMix;

    // DC blocker
    float hpfState;
    float lastFeedbackInput;

    // Tone knob
    float toneState;

    // Feedback
    float lastOutput; 

    void setupSmoother(juce::LinearSmoothedValue<float>& smoother, float initialValue) {
        smoother.reset(currentSampleRate, 0.05f);
        smoother.setCurrentAndTargetValue(initialValue);
    }

    float getParam(juce::String paramID) { return apvts.getRawParameterValue(paramID)->load(); }

    std::atomic<float>* splicePtr = nullptr;
    std::atomic<float>* delayPtr = nullptr;
    std::atomic<float>* densityPtr = nullptr;
    std::atomic<float>* pitchPtr = nullptr;
    std::atomic<float>* spreadPtr = nullptr;
    std::atomic<float>* feedbackPtr = nullptr;
    std::atomic<float>* widthPtr = nullptr;
    std::atomic<float>* tonePtr = nullptr;
    std::atomic<float>* reversePtr = nullptr;
    std::atomic<float>* mixPtr = nullptr;
};
