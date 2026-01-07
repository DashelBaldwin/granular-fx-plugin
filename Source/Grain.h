#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct Grain {
    float startPos = 0;
    float currentPos = 0;
    int durationSamples = 0;
    int samplesProcessed = 0;
    bool isActive = false;

    float processSample(const juce::AudioBuffer<float>& buffer, int channel, int bufferSize, bool shouldAdvance) 
    {
        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * samplesProcessed / durationSamples));
        
        int readIndex = static_cast<int>(currentPos) % bufferSize;
        float sample = buffer.getSample(channel, readIndex);
        
        if (shouldAdvance) 
        {
            currentPos += 1.0f; 
            samplesProcessed++;
            if (samplesProcessed >= durationSamples) isActive = false;
        }
        
        return sample * window;
    }
};