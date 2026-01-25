#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&](
        const juce::String& id, 
        const juce::String& name, 
        float min, 
        float max, 
        float step,
        float def, 
        float skew = 1.0f, 
        bool symmetric = false
    ) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            id, 
            name, 
            juce::NormalisableRange<float>(min, max, step, skew, symmetric), 
            def
        ));
    };

    addFloat("splice", "Splice (ms)", 0.10f, 2000.0f, 0.1f, 600.0f, 0.3f);
    addFloat("delay", "Delay (ms)", 0.0f, 1000.0f, 0.1f, 150.0f, 0.3f);
    addFloat("density", "Density", 1.0f, 32.0f, 0.1f, 2.0f);
    addFloat("pitch", "Pitch", 0.25f, 4.0f, 0.0f, 2.0f, 1.0f, true);
    addFloat("spread", "Spread (ms)", 0.0f, 500.0f, 0.1f, 150.0f, 0.3f);

    addFloat("width", "Width", 0.0f, 1.0f, 0.01f, 1.0f);
    addFloat("feedback", "Feedback", 0.0f, 1.0f, 0.01f, 0.75f);
    addFloat("tone", "Tone", 0.0f, 1.0f, 0.01f, 0.9f);
    addFloat("mix", "Mix", 0.0f, 1.0f, 0.01f, 0.5f);

    layout.add(std::make_unique<juce::AudioParameterBool>("reverse", "Reverse", true));

    addFloat("pitchOffset", "Pitch Offset (Cents)", 0.0f, 4800.0f, 1.0f, 0.0f);
    addFloat("spliceOffset", "Splice Offset (%)", 0.0f, 99.0f, 1.0f, 0.0f);
    addFloat("delayOffset", "Delay Offset (%)", 0.0f, 99.0f, 1.0f, 0.0f);

    return layout;
}

// void AudioPluginAudioProcessor::logGrainStats(const Grain& g) {
//     juce::ScopedLock lock(logMutex);
    
//     bool errorL = g.actualSamplesReadL > g.expectedSamplesL;
//     bool errorR = g.actualSamplesReadR > g.expectedSamplesR;
//     bool hasError = errorL || errorR;
    
//     juce::String logEntry;
    
//     if (hasError) {
//         logEntry << "[ERROR] ";
//     }

//     logEntry << "Grain started at buffer sample: " << g.startBufferSample << "\n";
//     logEntry << "Initial finalBaseDelay: " << juce::String(g.initFinalBaseDelay, 4);
//     logEntry << "Initial readPosR: " << juce::String(g.initReadPosR, 4);
//     logEntry << "Initial writePos: " << g.initWritePos;
//     logEntry << "writePos at collision: " << g.writePosAtCollision;

//     logEntry << "  L channel - Expected: " << juce::String(g.expectedSamplesL, 2) 
//              << " samples, Actual: " << juce::String(g.actualSamplesReadL, 2) << " samples";
//     if (errorL) {
//         logEntry << " [OVERREAD: " << juce::String(g.actualSamplesReadL - g.expectedSamplesL, 2) << "]";
//     }
//     logEntry << "\n";
    
//     logEntry << "  R channel - Expected: " << juce::String(g.expectedSamplesR, 2) 
//              << " samples, Actual: " << juce::String(g.actualSamplesReadR, 2) << " samples";
//     if (errorR) {
//         logEntry << " [OVERREAD: " << juce::String(g.actualSamplesReadR - g.expectedSamplesR, 2) << "]";
//     }
//     if (g.collision) {
//         logEntry << "[COLLISION DETECTED: " << juce::String(g.cs, 2) << "]";
//     }
//     logEntry << "\n\n";
    
//     logFile.appendText(logEntry);
// }

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
    pitchOffsetPtr  = apvts.getRawParameterValue("pitchOffset");
    spliceOffsetPtr = apvts.getRawParameterValue("spliceOffset");
    delayOffsetPtr  = apvts.getRawParameterValue("delayOffset");

    // logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    //             .getChildFile("GranularFxDebug.log");
    // 
    // logFile.deleteFile();
    // logFile.create();
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

    setupSmoother(paramSpliceMs, splicePtr->load());
    setupSmoother(paramDelayMs, delayPtr->load());
    setupSmoother(paramDensity, densityPtr->load());
    setupSmoother(paramPitch, pitchPtr->load());
    setupSmoother(paramSpread, spreadPtr->load());

    setupSmoother(paramWidth, widthPtr->load());
    setupSmoother(paramFeedback, feedbackPtr->load());
    setupSmoother(paramTone, tonePtr->load());
    setupSmoother(paramMix, mixPtr->load());

    paramReverse = true;

    setupSmoother(paramPitchOffset, pitchOffsetPtr->load());
    setupSmoother(paramSpliceOffset, spliceOffsetPtr->load());
    setupSmoother(paramDelayOffset, delayOffsetPtr->load());

    samplesUntilNextGrain = 0;
    writePos = 0;

    hpfStateL = 0.0f;
    hpfStateR = 0.0f;
    lastFeedL = 0.0f;
    lastFeedR = 0.0f;

    toneStateL = 0.0f;
    toneStateR = 0.0f;

    lastOutputL = 0.0f;
    lastOutputR = 0.0f;
    
    circularBuffer.respace(bufferSize);
    
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

    paramPitchOffset.setTargetValue(pitchOffsetPtr->load());
    paramSpliceOffset.setTargetValue(spliceOffsetPtr->load());
    paramDelayOffset.setTargetValue(delayOffsetPtr->load());

    // Get write ptr for each channel
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = (totalNumInputChannels>1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float curMix      = paramMix.getNextValue();
        float curFeedback = paramFeedback.getNextValue();
        float curTone     = paramTone.getNextValue();
        float curDensity  = paramDensity.getNextValue();
        float curSplice   = paramSpliceMs.getNextValue();
        float curPitch    = paramPitch.getNextValue();
        float curDelay    = paramDelayMs.getNextValue();
        float curSpread   = paramSpread.getNextValue();
        float curWidth    = paramWidth.getNextValue();
        
        float curPitchOff  = paramPitchOffset.getNextValue();
        float curSpliceOff = paramSpliceOffset.getNextValue();
        float curDelayOff  = paramDelayOffset.getNextValue();

        // Calculate tone filter coefficients
        float toneHz = juce::jmap(curTone, 200.0f, 20000.0f);
        float alpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneHz / (float)currentSampleRate);

        float inputL = leftChannel[i];
        float inputR = rightChannel ? rightChannel[i] : inputL;
        
        // --- FEEDBACK ---
        // Add previous output back into buffer with DC blocker, tone filter, and tanh saturation
        float rawFeedL = inputL + (lastOutputL * curFeedback);
        float rawFeedR = inputR + (lastOutputR * curFeedback);

        hpfStateL = 0.997f * (hpfStateL + rawFeedL - lastFeedL);
        hpfStateR = 0.997f * (hpfStateR + rawFeedR - lastFeedR);
        lastFeedL = rawFeedL;
        lastFeedR = rawFeedR;

        toneStateL += alpha * (hpfStateL - toneStateL);
        toneStateR += alpha * (hpfStateR - toneStateR);

        float feedL = std::tanh(toneStateL);
        float feedR = std::tanh(toneStateR);

        circularBuffer.write(feedL, feedR, writePos);

        // --- TRIGGER GRAINS ---
        if (samplesUntilNextGrain <= 0) {
            // Effective pitch ratio per channel
            float pitchL = curPitch * std::pow(2.0f, -curPitchOff / 1200.0f);
            float pitchR = curPitch * std::pow(2.0f,  curPitchOff / 1200.0f);

            // Effective splice lengths in samples
            float spliceSamplesL = std::ceil((curSplice / 1000.0f) * currentSampleRate);
            float spliceSamplesR = std::ceil(spliceSamplesL * (1.0f - (curSpliceOff / 100.0f)));

            // Total read distance per channel
            float totalReadSamplesL = spliceSamplesL * pitchL;
            float totalReadSamplesR = spliceSamplesR * pitchR;

            float spreadMs = juce::Random::getSystemRandom().nextFloat() * curSpread;

            float finalBaseDelay = curDelay + spreadMs;

            if (!paramReverse && pitchR > 1.0) {
                    float extraDelaySamplesL = std::max(0.0f, totalReadSamplesL - spliceSamplesL);
                    float extraDelaySamplesR = std::max(0.0f, totalReadSamplesR - spliceSamplesR);

                    float safeMsL = (extraDelaySamplesL / currentSampleRate) * 1000.0f;
                    float safeMsR = (extraDelaySamplesR / currentSampleRate) * 1000.0f;
                    float minSafeDelayMs = std::max(safeMsL, safeMsR);
                    
                    finalBaseDelay = std::max(finalBaseDelay, minSafeDelayMs);
            } 

            double delaySampL = (finalBaseDelay / 1000.0) * currentSampleRate;
            double delaySampR = delaySampL * (1.0 - (curDelayOff / 100.0));

            for (auto& g : grainPool) {
                if (!g.isActive) {

                    // Random pan, can add ping pong and dual later
                    float randomSide = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; 
                    float grainPan = 0.5f + (randomSide * 0.5f * curWidth);
                    float panRads = grainPan * juce::MathConstants<float>::halfPi;
                    float gainL = std::cos(panRads);
                    float gainR = std::sin(panRads);

                    g.trigger(
                        writePos,
                        (int)spliceSamplesL, (int)spliceSamplesR,
                        delaySampL, delaySampR,
                        (double)pitchL, (double)pitchR,
                        gainL, gainR,
                        paramReverse
                    );
                    break;
                }
            }

            samplesUntilNextGrain = static_cast<int>(spliceSamplesL / std::max(1.0f, curDensity));
        }
        samplesUntilNextGrain--;

        // --- PROCESS GRAINS ---
        float grainSumL = 0.0f;
        float grainSumR = 0.0f;
        
        for (auto& g : grainPool) {
            if (g.isActive) {
                float outL = 0.0f;
                float outR = 0.0f;
                
                // bool wasActive = g.isActive;
                g.process(circularBuffer, outL, outR, writePos, bufferSize-1, &rightChannelCollision, &rightChannelCollisionSamples);
                // if (wasActive && !g.isActive) { logGrainStats(g); }

                grainSumL += outL;
                grainSumR += outR;
                
            }
        }

        // --- MIX & OUTPUT ---
        float densityScale = 1.0f / std::sqrt(std::max(1.0f, curDensity));
        float wetL = grainSumL * densityScale;
        float wetR = grainSumR * densityScale;
        
        float dryGain = std::cos(curMix * juce::MathConstants<float>::halfPi);
        float wetGain = std::sin(curMix * juce::MathConstants<float>::halfPi);
        
        leftChannel[i] = (inputL * dryGain) + (wetL * wetGain);
        if (rightChannel) rightChannel[i] = (inputR * dryGain) + (wetR * wetGain);

        // --- FEEDBACK ---
        lastOutputL = wetL; 
        lastOutputR = wetR; 

        writePos = (writePos + 1) & (bufferSize - 1);
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
