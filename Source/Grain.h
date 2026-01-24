#pragma once

#include "CircularBuffer.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct Grain {
    float readPosL = 0.0f;
    float envIndexL = 0.0f;
    float envStepL = 0.0f;
    float pitchStepL = 0.0f;

    float readPosR = 0.0f;
    float envIndexR = 0.0f;
    float envStepR = 0.0f;
    float pitchStepR = 0.0f;

    float leftGain = 0.707f;
    float rightGain = 0.707f;
    bool isReverse = false;
    bool isActive = false;

    int startBufferSample = 0;
    float expectedSamplesL = 0.0f;
    float expectedSamplesR = 0.0f;
    float actualSamplesReadL = 0.0f;
    float actualSamplesReadR = 0.0f;
    bool collision = false;
    float cs = 0.0f;

    float initFinalBaseDelay = 0.0f;
    float initReadPosR = 0.0f;
    int initWritePos = 0;
    int writePosAtCollision = 0;

    void trigger(int writePos, double sampleRate, 
             float baseDelayMs, float delayOffsetPercent,
             float basePitchRatio, float pitchOffsetCents,
             float baseSpliceMs, float spliceOffsetPercent,
             bool reverse, float newPan) {

        // Pitch offset
        float pitchMultiplierL = std::pow(2.0f, -pitchOffsetCents / 1200.0f);
        float pitchMultiplierR = std::pow(2.0f, pitchOffsetCents / 1200.0f);
        pitchStepL = basePitchRatio * pitchMultiplierL;
        pitchStepR = basePitchRatio * pitchMultiplierR;

        // Splice offset
        float durSamplesL = (baseSpliceMs / 1000.0f) * (float)sampleRate;
        float durSamplesR = durSamplesL * (1.0f - (spliceOffsetPercent / 100.0f));
        envIndexL = 0.0f;
        envIndexR = 0.0f;
        envStepL = 1.0f / std::max(1.0f, durSamplesL);
        envStepR = 1.0f / std::max(1.0f, durSamplesR);

        // Delay offset
        float samplesDelayL = (baseDelayMs / 1000.0f) * (float)sampleRate;
        float samplesDelayR = samplesDelayL * (1.0f - (delayOffsetPercent / 100.0f));

        readPosL = (float)writePos - samplesDelayL;
        readPosR = (float)writePos - samplesDelayR;

        float panRads = newPan * juce::MathConstants<float>::halfPi;
        leftGain = std::cos(panRads);
        rightGain = std::sin(panRads);

        startBufferSample = writePos;
        expectedSamplesL = durSamplesL * std::abs(pitchStepL);
        expectedSamplesR = durSamplesR * std::abs(pitchStepR);
        actualSamplesReadL = 0.0f;
        actualSamplesReadR = 0.0f;
        
        isReverse = reverse;
        isActive = true;

        initFinalBaseDelay = baseDelayMs;
        initReadPosR = readPosR;
        initWritePos = writePos;

    }

    void process(const CircularBuffer& buffer, float& outL, float& outR,
                 int writePos, int mask, std::atomic<bool>* collisionFlag, std::atomic<float>* collisionSamples) {
        if (!isActive) {
            outL = 0.0f;
            outR = 0.0f;
            return;
        }

        float direction = isReverse ? -1.0f : 1.0f;

        float sampleL = 0.0f;
        if (envIndexL < 1.0f) {
            float windowL = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndexL));
            sampleL = buffer.read(0, readPosL) * windowL;

            readPosL += pitchStepL * direction;
            envIndexL += envStepL;

            actualSamplesReadL += std::abs(pitchStepL);
        }

        float sampleR = 0.0f;
        if (envIndexR < 1.0f) {
            if (collisionFlag != nullptr) {
                int size = mask + 1;
                
                int rInt = static_cast<int>(std::floor(readPosR));
                int wrappedDist = (rInt - writePos) & mask;

                if (wrappedDist > (size / 2)) {
                    wrappedDist -= size;
                }

                if (wrappedDist > 0) {
                    *collisionFlag = true;
                    *collisionSamples = (float)wrappedDist + (readPosR - std::floor(readPosR));
                    cs = (float)wrappedDist + (readPosR - std::floor(readPosR));
                    collision = true;
                    writePosAtCollision = writePos;
                }
            }

            float windowR = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * envIndexR));
            sampleR = buffer.read(1, readPosR) * windowR;

            readPosR += pitchStepR * direction;
            envIndexR += envStepR;

            actualSamplesReadR += std::abs(pitchStepR);
        }

        outL = sampleL * leftGain;
        outR = sampleR * rightGain;

        if (envIndexL >= 1.0f && envIndexR >= 1.0f) {
            isActive = false;
        }
    }
};