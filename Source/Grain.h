#pragma once

#include "CircularBuffer.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct Grain {
    double readPosL = 0.0;
    double readPosR = 0.0;

    int totalSamplesL = 0;
    int samplesProcessedL = 0;

    int totalSamplesR = 0;
    int samplesProcessedR = 0;

    double pitchStepL = 0.0;
    double pitchStepR = 0.0;

    float leftGain = 0.707f;
    float rightGain = 0.707f;

    bool isReverse = false;

    bool isActive = false;

    int startBufferSample = 0;
    double expectedSamplesL = 0.0;
    double expectedSamplesR = 0.0;
    double actualSamplesReadL = 0.0;
    double actualSamplesReadR = 0.0;
    bool collision = false;
    float cs = 0.0f;

    float initFinalBaseDelay = 0.0f;
    double initReadPosR = 0.0;
    int initWritePos = 0;
    int writePosAtCollision = 0;

    void trigger(int writePos, double sampleRate, 
             float baseDelayMs, float delayOffsetPercent,
             float basePitchRatio, float pitchOffsetCents,
             float baseSpliceMs, float spliceOffsetPercent,
             bool reverse, float newPan) {

        // Pitch offset
        double pitchMultiplierL = std::pow(2.0, -pitchOffsetCents / 1200.0);
        double pitchMultiplierR = std::pow(2.0, pitchOffsetCents / 1200.0);
        pitchStepL = (double)basePitchRatio * pitchMultiplierL;
        pitchStepR = (double)basePitchRatio * pitchMultiplierR;

        // Splice offset
        float durSamplesL = (baseSpliceMs / 1000.0f) * (float)sampleRate;
        float durSamplesR = durSamplesL * (1.0f - (spliceOffsetPercent / 100.0f));

        totalSamplesL = (int)std::ceil(durSamplesL);
        totalSamplesR = (int)std::ceil(durSamplesR);
        
        samplesProcessedL = 0;
        samplesProcessedR = 0;

        // Delay offset
        double samplesDelayL = (baseDelayMs / 1000.0) * sampleRate;
        double samplesDelayR = samplesDelayL * (1.0 - (delayOffsetPercent / 100.0));

        readPosL = (double)writePos - samplesDelayL;
        readPosR = (double)writePos - samplesDelayR;

        float panRads = newPan * juce::MathConstants<float>::halfPi;
        leftGain = std::cos(panRads);
        rightGain = std::sin(panRads);

        startBufferSample = writePos;
        expectedSamplesL = (double)totalSamplesL * std::abs(pitchStepL);
        expectedSamplesR = (double)totalSamplesR * std::abs(pitchStepR);
        actualSamplesReadL = 0.0;
        actualSamplesReadR = 0.0;
        
        isReverse = reverse;
        isActive = true;

        initFinalBaseDelay = baseDelayMs;
        initReadPosR = readPosR;
        initWritePos = writePos;

    }

    void process(const CircularBuffer& buffer, float& outL, float& outR,
                 int writePos, int mask, std::atomic<bool>* collisionFlag, std::atomic<float>* collisionSamples) {
        if (!isActive) {
            outL = 0.0f; outR = 0.0f; return;
        }

        float direction = isReverse ? -1.0f : 1.0f;

        // LEFT CHANNEL
        float sampleL = 0.0f;
        if (samplesProcessedL < totalSamplesL) {
            // Calculate envIndex on the fly for the window
            float envIndex = (float)samplesProcessedL / (float)totalSamplesL;
            float windowL = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndex));
            
            sampleL = buffer.read(0, readPosL) * windowL;
            readPosL += pitchStepL * direction;
            
            samplesProcessedL++;
            actualSamplesReadL += std::abs(pitchStepL);
        }

        // RIGHT CHANNEL
        float sampleR = 0.0f;
        if (samplesProcessedR < totalSamplesR) {
            if (collisionFlag != nullptr) {
                int size = mask + 1;
                int rInt = static_cast<int>(std::floor(readPosR));
                int wrappedDist = (rInt - writePos) & mask;

                if (wrappedDist > (size / 2)) {
                    wrappedDist -= size;
                }

                // If distance is positive, Read is AHEAD of Write (Collision)
                if (wrappedDist > 0) {
                    *collisionFlag = true;
                    *collisionSamples = (float)wrappedDist;
                    cs = (float)wrappedDist;
                    collision = true;
                    writePosAtCollision = writePos;
                }
            }

            float envIndex = (float)samplesProcessedR / (float)totalSamplesR;
            float windowR = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndex));
            
            sampleR = buffer.read(1, readPosR) * windowR;
            readPosR += pitchStepR * direction;
            
            samplesProcessedR++;
            actualSamplesReadR += std::abs(pitchStepR);
        }

        outL = sampleL * leftGain;
        outR = sampleR * rightGain;

        if (samplesProcessedL >= totalSamplesL && samplesProcessedR >= totalSamplesR) {
            isActive = false;
        }
    }
};

