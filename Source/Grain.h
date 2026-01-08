#pragma once

#include "CircularBuffer.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct Grain {
    float readPos = 0.0f;
    float envIndex = 0.0f;
    float envStep = 0.0f;
    float pitchRatio = 1.0f;
    float pan = 0.5f;
    bool isReverse = false;
    bool isActive = false;

    void trigger(int writePos, float totalOffset, float pitch, int durationSamples, bool reverse, float newPan) {
        readPos = (float)writePos - totalOffset; // Start position in # samples behind write head
        
        envIndex = 0.0f;
        envStep = 1.0f / (float)durationSamples;
        pitchRatio = pitch;
        isReverse = reverse;
        pan = newPan;
        isActive = true;
    }

    float process(const CircularBuffer& buffer, int bufferLength) {
        // Deactivate if finished
        if (envIndex >= 1.0f) {
            isActive = false;
            return 0.0f;
        }

        if (!isActive) return 0.0f;

        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndex));
        
        // Wrap read position and read from the buffer
        float wrappedReadPos = std::fmod(readPos, (float)bufferLength);
        if (wrappedReadPos < 0) 
            wrappedReadPos += (float)bufferLength;
        
        float sample = buffer.read(wrappedReadPos) * window;

        // Advance ptrs
        float direction = isReverse ? -1.0f : 1.0f;
        readPos += pitchRatio * direction;
        envIndex += envStep;

        return sample;
    }
};