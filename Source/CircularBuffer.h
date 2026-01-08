#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class CircularBuffer {
public:
    void respace(int samples) {
        buffer.setSize(1, samples);
        buffer.clear();

        // Used for bitwise modulo logic, which is faster than fmod, but only works if buffer size is a power of 2
        mask = samples - 1; 
    }

    void write(float sample, int index) {
        buffer.setSample(0, index % buffer.getNumSamples(), sample);
    }

    // Read from the buffer, lerp fractional indices
    float read(float index) const {
        int len = buffer.getNumSamples();
        int i1 = (int)index;
        int i2 = (i1 + 1) % len;
        float frac = index - i1;
        
        float s1 = buffer.getSample(0, i1);
        float s2 = buffer.getSample(0, i2);
        return s1 + frac * (s2 - s1);
    }

private:
    juce::AudioBuffer<float> buffer;
    int mask;
};