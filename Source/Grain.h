#pragma once

#include "CircularBuffer.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct Grain {
    float readPos = 0.0f;
    float envIndex = 0.0f;
    float envStep = 0.0f;
    float pitchRatio = 1.0f;
    bool isReverse = false;
    bool isActive = false;

    void trigger(int writePos, float delayInSamples, float pitch, int durationSamples, bool reverse) {
        // Start position in # samples behind write head
        readPos = (float)writePos - delayInSamples + (isReverse ? durationSamples : 0);
        
        envIndex = 0.0f;
        envStep = 1.0f / (float)durationSamples;
        pitchRatio = pitch;
        isReverse = reverse;
        isActive = true;
    }

    float process(const CircularBuffer& buffer, int bufferLength) {
        if (!isActive) return 0.0f;

        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndex));
        
        // Wrap read position and read from the buffer
        float wrappedReadPos = std::fmod(readPos, (float)bufferLength);
        if (wrappedReadPos < 0) 
            wrappedReadPos += bufferLength;
        
        float sample = buffer.read(wrappedReadPos) * window;

        // Advance ptrs
        float direction = isReverse ? -1.0f : 1.0f;
        readPos += pitchRatio * direction;
        envIndex += envStep;

        // Deactivate when finished
        if (envIndex >= 1.0f) {
            isActive = false;
        }

        return sample;
    }
};