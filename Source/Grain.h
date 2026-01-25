#pragma once

#include "CircularBuffer.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct GrainChannel {
    double readPos = 0.0;
    double pitchStep = 0.0;
    int totalSamples = 0;
    int samplesProcessed = 0;

    // Debug
    double actualSamplesRead = 0.0;

    void reset(int durationSamples, double startReadPos, double step) {
        totalSamples = durationSamples;
        readPos = startReadPos;
        pitchStep = step;
        samplesProcessed = 0;
        actualSamplesRead = 0.0;
    }

    // Calculate and set the output level for this channel, return false when the channel is finished playing
    bool getNextSample(
        const CircularBuffer& buffer, 
        int channelIndex, 
        float& outputSample, 
        bool reverse, 
        int writePos = 0, 
        int mask = 0, 
        // Debug params
        std::atomic<bool>* collisionFlag = nullptr, 
        std::atomic<float>* collisionSamples = nullptr, 
        float* debugCollisionPos = nullptr, 
        int* debugWritePosCol = nullptr
    ) {
        if (samplesProcessed >= totalSamples) {
            outputSample = 0.0f;
            return false;
        }

        // Debug
        if (collisionFlag != nullptr) {
            int size = mask + 1;
            int rInt = static_cast<int>(std::floor(readPos));
            int wrappedDist = (rInt - writePos) & mask;

            if (wrappedDist > (size / 2)) { wrappedDist -= size; }

            if (wrappedDist > 0) {
                *collisionFlag = true;
                if (collisionSamples) *collisionSamples = (float)wrappedDist;
                if (debugCollisionPos) *debugCollisionPos = (float)wrappedDist;
                if (debugWritePosCol) *debugWritePosCol = writePos;
            }
        }

        // Calculate hanning window
        float envIndex = (float)samplesProcessed / (float)totalSamples;
        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndex));

        // Read from buffer and advance
        outputSample = buffer.read(channelIndex, readPos) * window;
        
        float direction = reverse ? -1.0f : 1.0f;
        readPos += pitchStep * direction;
        
        samplesProcessed++;

        actualSamplesRead += std::abs(pitchStep);

        return true;
    }
};

struct Grain {
    GrainChannel chL;
    GrainChannel chR;

    float leftGain = 0.707f;
    float rightGain = 0.707f;
    bool isReverse = false;
    bool isActive = false;

    // Debug 
    int startBufferSample = 0;
    double expectedSamplesL = 0.0;
    double expectedSamplesR = 0.0;
    float initFinalBaseDelay = 0.0f;
    double initReadPosR = 0.0;
    int initWritePos = 0;
    
    // Debug
    bool collision = false;
    float cs = 0.0f;
    int writePosAtCollision = 0;

    // Debug
    double getActualSamplesReadL() const { return chL.actualSamplesRead; }
    double getActualSamplesReadR() const { return chR.actualSamplesRead; }

    // These parameters are assumed to be safe; a minimum safe delay must be calculated and enforced beforehand
    void trigger(
        int writePos, 
        int durSamplesL, int durSamplesR,
        double delaySamplesL, double delaySamplesR,
        double stepL, double stepR,
        float gainL, float gainR,
        bool reverse
    ) {
        chL.reset(durSamplesL, (double)writePos - delaySamplesL, stepL);
        chR.reset(durSamplesR, (double)writePos - delaySamplesR, stepR);

        leftGain = gainL;
        rightGain = gainR;
        
        isReverse = reverse;
        isActive = true;

        // Debug 
        startBufferSample = writePos;
        expectedSamplesL = (double)chL.totalSamples * std::abs(stepL);
        expectedSamplesR = (double)chR.totalSamples * std::abs(stepR);
        initWritePos = writePos;
        collision = false;
    }

    void process(const CircularBuffer& buffer, float& outL, float& outR, int writePos, 
        int mask, std::atomic<bool>* collisionFlag, std::atomic<float>* collisionSamples
    ) {
        if (!isActive) {
            outL = 0.0f; outR = 0.0f; return;
        }

        float sampleL = 0.0f;
        float sampleR = 0.0f;

        bool activeL = chL.getNextSample(buffer, 0, sampleL, isReverse);

        bool activeR = chR.getNextSample(buffer, 1, sampleR, isReverse, 
                                         writePos, mask, 
                                         collisionFlag, collisionSamples, 
                                         &cs, &writePosAtCollision);
        
        if (cs > 0.0f) collision = true; // Debug

        outL = sampleL * leftGain;
        outR = sampleR * rightGain;

        if (!activeL && !activeR) {
            isActive = false;
        }
    }
};