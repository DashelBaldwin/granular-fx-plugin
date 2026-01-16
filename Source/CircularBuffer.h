#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class CircularBuffer {
public:
    void respace(int samples) {
        buffer.setSize(2, samples);
        buffer.clear();

        // Used for bitwise modulo logic, which is faster than fmod, but only works if buffer size is a power of 2
        mask = samples - 1; 
    }

    void write(float sampleL, float sampleR, int index) {
        int wrapped = index & mask;
        buffer.setSample(0, wrapped, sampleL);
        buffer.setSample(1, wrapped, sampleR);
    }

    // Read from the buffer, lerp fractional indices
    float read(int channel, float index) const {
        int i1 = static_cast<int>(std::floor(index));
        float frac = index - static_cast<float>(i1);

        // Bitwise modulo
        int idx1 = i1 & mask;
        int idx2 = (i1 + 1) & mask;

        int ch = std::min(channel, buffer.getNumChannels() - 1);

        float s1 = buffer.getSample(ch, idx1);
        float s2 = buffer.getSample(ch, idx2);

        return s1 + frac * (s2 - s1);
    }

private:
    juce::AudioBuffer<float> buffer;
    int mask;
};