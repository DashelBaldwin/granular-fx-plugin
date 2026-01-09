#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("splice", "Splice (ms)", 0.01f, 2000.0f, 600.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delay", "Delay (ms)", 1.0f, 1000.0f, 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("density", "Density", 1.0f, 32.0f, 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", 0.5f, 4.0f, 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("spread", "Spread (s)", 0.001f, 1.0f, 0.15f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("width", "Width", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 1.0f, 0.75f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tone", "Tone", 0.0f, 1.0f, 0.9f));
    layout.add(std::make_unique<juce::AudioParameterBool>("reverse", "Reverse", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 0.5f));

    return layout;
}

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
    splicePtr = apvts.getRawParameterValue("splice");
    delayPtr = apvts.getRawParameterValue("delay");
    densityPtr = apvts.getRawParameterValue("density");
    pitchPtr = apvts.getRawParameterValue("pitch");
    spreadPtr = apvts.getRawParameterValue("spread");
    feedbackPtr = apvts.getRawParameterValue("feedback");
    widthPtr = apvts.getRawParameterValue("width");
    tonePtr = apvts.getRawParameterValue("tone");
    reversePtr = apvts.getRawParameterValue("reverse");
    mixPtr = apvts.getRawParameterValue("mix");

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
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = (int)sampleRate;

    setupSmoother(paramSpliceMs, 600.0f);
    setupSmoother(paramDelayMs, 20.0f);
    setupSmoother(paramPitch, 2.0f);
    setupSmoother(paramDensity, 2.0f);
    setupSmoother(paramFeedback, 0.75f);
    setupSmoother(paramSpread, 0.25f);
    setupSmoother(paramWidth, 1.0f);
    setupSmoother(paramTone, 0.90f);
    setupSmoother(paramMix, 0.80f);
    paramReverse = true;

    samplesUntilNextGrain = 0;
    writePos = 0;
    hpfState = 0.0f;
    lastFeedbackInput = 0.0f;
    toneState = 0.0f;
    lastOutput = 0.0f;
    
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

    paramSpliceMs.setTargetValue(splicePtr->load());
    paramDelayMs.setTargetValue(delayPtr->load());
    paramDensity.setTargetValue(densityPtr->load());
    paramPitch.setTargetValue(pitchPtr->load());
    paramSpread.setTargetValue(spreadPtr->load());
    paramFeedback.setTargetValue(feedbackPtr->load());
    paramWidth.setTargetValue(widthPtr->load());
    paramTone.setTargetValue(tonePtr->load());
    paramReverse = reversePtr->load() > 0.5f;
    paramMix.setTargetValue(mixPtr->load());

    // Get write ptr for each channel
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = (totalNumInputChannels>1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float currentMix = paramMix.getNextValue();
        float currentFeedback = paramFeedback.getNextValue();
        float currentTone = paramTone.getNextValue();
        float currentDensity = paramDensity.getNextValue();
        
        paramSpliceMs.getNextValue();
        paramPitch.getNextValue();
        paramDelayMs.getNextValue();
        paramSpread.getNextValue();
        paramWidth.getNextValue();

        // Calculate tone filter coefficients
        float toneHz = juce::jmap(currentTone, 200.0f, 20000.0f); // Test if this linear scale is fine, otherwise change to logarithmic
        float alpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneHz / (float)currentSampleRate);

        float inputL = leftChannel[i];
        float inputR = rightChannel ? rightChannel[i] : inputL;
        float monoInput = (inputL + inputR) * 0.5f;

        // --- FEEDBACK ---
        // Add previous output back into buffer with DC blocker, tone filter, and tanh saturation
        float rawFeedback = monoInput + (lastOutput * currentFeedback);

        hpfState = 0.995f * (hpfState + rawFeedback - lastFeedbackInput);
        lastFeedbackInput = rawFeedback;

        toneState += alpha * (hpfState - toneState);

        float feedbackSample = std::tanh(toneState);
        circularBuffer.write(feedbackSample, writePos);

        // --- TRIGGER GRAINS ---
        // Trigger a new grain with delay + random spread and panning at regular intervals
        if (samplesUntilNextGrain <= 0) {
            // Calculate grain interval
            float spliceMs = paramSpliceMs.getCurrentValue();
            float spliceSamples = (spliceMs / 1000.0f) * (float)currentSampleRate;
            float density = paramDensity.getCurrentValue();

            int triggerInterval = static_cast<int>(spliceSamples / std::max(1.0f, density));

            for (auto& g : grainPool) {
                if (!g.isActive) {
                    float pitch = paramPitch.getCurrentValue();
                    float delayMs = paramDelayMs.getCurrentValue();
                    float spread = paramSpread.getCurrentValue();
                    float width = paramWidth.getCurrentValue();

                    float delaySamples = (delayMs / 1000.0f) * (float)currentSampleRate;

                    // If our splice, delay, and pitch settings would cause this grain's read head to advance past 
                    // the write head, the grain should instead use the minimum delay value that avoids this problem
                    float safetyPadding = 512.0f;
                    float minSafeDelay = safetyPadding;

                    if (!paramReverse && pitch > 1.0f) {
                        minSafeDelay = (spliceSamples * (pitch - 1.0f)) + safetyPadding;
                    }

                    float clampedBaseDelay = std::max(minSafeDelay, delaySamples);
                    float spreadSamples = juce::Random::getSystemRandom().nextFloat() * spread * (float)currentSampleRate;
                    float totalInitialOffset = clampedBaseDelay + spreadSamples;

                    float randomSide = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; // -1.0 to 1.0
                    float grainPan = 0.5f + (randomSide * 0.5f * width);

                    g.trigger(writePos, totalInitialOffset, pitch, (int)spliceSamples, paramReverse, grainPan);
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

                grainSumL += grainSample * g.leftGain;
                grainSumR += grainSample * g.rightGain;
            }
        }

        // --- VOLUME ADJUSTMENT AND MIX ---
        // Scale grain volume by 1/sqrt(Density), calculate wet, mix
        float densityScale = 1.0f / std::sqrt(std::max(1.0f, currentDensity));
        float wetL = grainSumL * densityScale;
        float wetR = grainSumR * densityScale;
        
        float dryGain = std::cos(currentMix * juce::MathConstants<float>::halfPi);
        float wetGain = std::sin(currentMix * juce::MathConstants<float>::halfPi);
        
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
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AudioPluginAudioProcessor();
}
