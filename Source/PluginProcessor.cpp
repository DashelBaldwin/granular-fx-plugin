#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ) {
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const {
    return "GranularFxPlugin";
}

bool AudioPluginAudioProcessor::acceptsMidi() const {
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms() {
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() {
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index) {
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index) {
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName) {
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    
    circularBuffer.respace(bufferSize);
    
    samplesUntilNextGrain = 0;
    writePos = 0;
}

void AudioPluginAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}


void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto numSamples = buffer.getNumSamples();

    // Calculate grain interval
    float spliceSamples = (paramSpliceMs / 1000.0f) * (float)currentSampleRate;
    int triggerInterval = static_cast<int>(spliceSamples / paramDensity);

    // Get write pointer for the first channel (mono output for now)
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = (totalNumInputChannels>1) ? buffer.getWritePointer(1) : nullptr;

    // Calculate tone filter coefficients
    float toneHz = juce::jmap(paramTone, 200.0f, 20000.0f); // Test if this linear scale is fine, otherwise change to logarithmic
    float alpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneHz / (float)currentSampleRate);

    for (int i = 0; i < numSamples; ++i) {
        float inputL = leftChannel[i];
        float inputR = rightChannel ? rightChannel[i] : inputL;
        float monoInput = (inputL + inputR) * 0.5f;

        // --- FEEDBACK ---
        // Add previous output back into buffer with DC blocker, tone filter, and tanh saturation
        float rawFeedback = monoInput + (lastOutput * paramFeedback);

        hpfState = 0.995f * (hpfState + rawFeedback - lastFeedbackInput);
        lastFeedbackInput = rawFeedback;

        toneState += alpha * (hpfState - toneState);

        float feedbackSample = std::tanh(toneState);
        circularBuffer.write(feedbackSample, writePos);

        // --- TRIGGER GRAINS ---
        // Trigger a new grain with delay + random spread and panning at regular intervals
        if (samplesUntilNextGrain <= 0) {
            for (auto& g : grainPool) {
                if (!g.isActive) {
                    float delaySamples = (paramDelayMs / 1000.0f) * (float)currentSampleRate;

                    // If our splice, delay, and pitch settings would cause this grain's read head to advance past 
                    // the write head, the grain should instead use the minimum delay value that avoids this problem
                    float safetyPadding = 512.0f;
                    float minSafeDelay = paramReverse ? safetyPadding : (spliceSamples * paramPitch) + safetyPadding;

                    float clampedBaseDelay = std::max(minSafeDelay, delaySamples);

                    float spread = juce::Random::getSystemRandom().nextFloat() * paramSpread * (float)currentSampleRate;

                    float totalInitialOffset = clampedBaseDelay + spread;

                    float randomSide = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; // -1.0 to 1.0
                    float grainPan = 0.5f + (randomSide * 0.5f * paramWidth);

                    g.trigger(writePos, totalInitialOffset, paramPitch, (int)spliceSamples, paramReverse, grainPan);
                    break;
                }
            }
            samplesUntilNextGrain = triggerInterval;
        }
        samplesUntilNextGrain--;

        // --- PROCESS ---
        // Sum active grains
        float grainSumL = 0.0f;
        float grainSumR = 0.0f;
        for (auto& g : grainPool) {
            if (g.isActive) {
                float grainSample = g.process(circularBuffer, bufferSize);

                // Constant power panning
                float panRads = g.pan * juce::MathConstants<float>::halfPi;
                grainSumL += grainSample * std::cos(panRads);
                grainSumR += grainSample * std::sin(panRads);
            }
        }

        // --- VOLUME ADJUSTMENT AND MIX ---
        // Scale grain volume by 1/sqrt(Density), calculate wet, mix
        float densityScale = 1.0f / std::sqrt(std::max(1.0f, paramDensity));
        float wetL = grainSumL * densityScale;
        float wetR = grainSumR * densityScale;
        
        float dryGain = std::cos(paramMix * juce::MathConstants<float>::halfPi);
        float wetGain = std::sin(paramMix * juce::MathConstants<float>::halfPi);
        
        leftChannel[i] = (inputL * dryGain) + (wetL * wetGain);
        if (rightChannel) rightChannel[i] = (inputR * dryGain) + (wetR * wetGain);

        lastOutput = (wetL + wetR) * 0.5f; // Send mono average of wet to feedback 

        writePos = (writePos + 1) % bufferSize; // Advance write ptr
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const {
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() {
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AudioPluginAudioProcessor();
}
