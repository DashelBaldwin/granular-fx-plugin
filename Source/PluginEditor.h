#pragma once

#include "PluginProcessor.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor, public juce::Timer {
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback();

private:
    bool collisionVisualTrigger = false;
    float collisionSamplesText = 0.0f;
    int collisionDecay = 0;

    struct WaveformVisualizer : public juce::Component {
        CircularBuffer* processorBuffer = nullptr;

        std::array<Grain, 32>* grainPool = nullptr;

        int currentWritePos = 0;

        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colours::black.withAlpha(0.5f));
            
            if (processorBuffer == nullptr) return;

            const auto& rawBuffer = processorBuffer->getRawBuffer();
            auto* readerL = rawBuffer.getReadPointer(0);
            auto* readerR = rawBuffer.getReadPointer(1);
            int mask = processorBuffer->getMask();
            int totalSamples = mask + 1;

            float width = (float)getWidth();
            float height = (float)getHeight();
            float midY = height / 2.0f;
            float samplesPerPixel = (float)totalSamples / width;

            for (int x = 0; x < getWidth(); ++x) {
                int startSample = (int)(x * samplesPerPixel);
                int endSample = (int)((x + 1) * samplesPerPixel);
                
                float maxL = 0.0f;
                float maxR = 0.0f;

                for (int s = startSample; s < endSample; s += 10) {
                    int idx = s & mask;

                    maxL = std::max(maxL, std::abs(readerL[idx]));
                    maxR = std::max(maxR, std::abs(readerR[idx]));
                }

                auto scale = [](float boost, float val) {
                    if (val == 0) return 0.0f;
                    return std::pow(std::abs(val), 1.0f - boost);
                };

                const float boostInner = -0.2f;
                const float boostOuter = 0.5f;
                const juce::Colour colourInner = juce::Colours::aquamarine.withAlpha(0.75f);
                const juce::Colour colourOuter = juce::Colours::deepskyblue.withAlpha(0.35f);

                // L excursion
                float leftTopYOuter = midY - (scale(boostOuter, maxL) * midY * 0.95f);
                g.setColour(colourOuter);
                g.drawVerticalLine(x, leftTopYOuter, midY);

                float leftTopYInner = midY - (scale(boostInner, maxL) * midY * 0.95f);
                g.setColour(colourInner);
                g.drawVerticalLine(x, leftTopYInner, midY);

                // R excursion
                float rightBottomYOuter = midY + (scale(boostOuter, maxR) * midY * 0.95f);
                g.setColour(colourOuter);
                g.drawVerticalLine(x, midY, rightBottomYOuter);

                float rightBottomYInner = midY + (scale(boostInner, maxR) * midY * 0.95f);
                g.setColour(colourInner);
                g.drawVerticalLine(x, midY, rightBottomYInner);
            }

            // Grain playheads & bounds
            if (grainPool != nullptr && processorBuffer != nullptr) {
                int mask = processorBuffer->getMask();
                int totalSamples = mask + 1;
                float width = (float)getWidth();
                float height = (float)getHeight();
                float midY = height / 2.0f;

                for (const auto& grain : *grainPool) {
                    if (!grain.isActive) continue;

                    auto drawGrain = [&](float yStart, float yEnd, float readPos, float envIndex, float envStep, float pitchStep, float direction, juce::Colour color) {
                        float grainLenSamples = 1.0f / envStep;
                        
                        float distanceCovered = (envIndex / envStep) * pitchStep;

                        float startSampleRaw = readPos - (distanceCovered * direction);
                        if (direction < 0.0f) startSampleRaw -= (grainLenSamples * pitchStep);
                        
                        int startSampleWrapped = (int)(startSampleRaw) & mask;
                        
                        float pixelPerSample = width / (float)totalSamples;
                        
                        float xStart = (float)startSampleWrapped * pixelPerSample;
                        float xWidth = (grainLenSamples * pitchStep) * pixelPerSample;
                        float xReadPos = ((int)readPos & mask) * pixelPerSample;

                        g.setColour(color);


                        if (xStart + xWidth > width) {
                            g.fillRect(xStart, yStart, width - xStart, yEnd - yStart);
                            g.fillRect(0.0f, yStart, xWidth - (width - xStart), yEnd - yStart);
                        }
                        else {
                            g.fillRect(xStart, yStart, xWidth, yEnd - yStart);
                        }

                        g.setColour(juce::Colours::white.withAlpha(0.15f));
                        g.drawVerticalLine((int)xReadPos, yStart, yEnd);
                    };

                    float direction = grain.isReverse ? -1.0f : 1.0f;

                    float progressL = 0.0f;
                    float stepL = 0.0f;

                    if (grain.totalSamplesL > 0) {
                        progressL = (float)grain.samplesProcessedL / (float)grain.totalSamplesL;
                        stepL = 1.0f / (float)grain.totalSamplesL;
                    }

                    drawGrain(0.0f, midY, 
                            grain.readPosL, progressL, stepL, grain.pitchStepL, 
                            direction, juce::Colours::white.withAlpha(0.015f));


                    float progressR = 0.0f;
                    float stepR = 0.0f;

                    if (grain.totalSamplesR > 0) {
                        progressR = (float)grain.samplesProcessedR / (float)grain.totalSamplesR;
                        stepR = 1.0f / (float)grain.totalSamplesR;
                    }

                    drawGrain(midY, height, 
                            grain.readPosR, progressR, stepR, grain.pitchStepR, 
                            direction, juce::Colours::white.withAlpha(0.015f));
                }
            }

            // Midline
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawHorizontalLine((int)midY, 0, width);

            // Playhead
            float playheadX = ((float)(currentWritePos & mask) / (float)totalSamples) * width;
            g.setColour(juce::Colours::red.withAlpha(0.4f));
            g.drawVerticalLine((int)playheadX, 0, height);

            

        }
    };

    struct GuiComponent {
        juce::Slider slider;
        juce::Label label;
        juce::ToggleButton reverseButton;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    std::vector<std::unique_ptr<GuiComponent>> guiComponents;

    WaveformVisualizer waveformVisualizer;

    void setupKnob(juce::String paramID, juce::String paramName);
    void setupToggle(juce::String paramID, juce::String paramName);

    AudioPluginAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
