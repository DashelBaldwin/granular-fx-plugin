#include "PluginProcessor.h"
#include "PluginEditor.h"

void AudioPluginAudioProcessorEditor::timerCallback() {
    waveformVisualizer.processorBuffer = &processorRef.circularBuffer;
    waveformVisualizer.grainPool = &processorRef.grainPool;
    waveformVisualizer.currentWritePos = processorRef.writePos;

    if (processorRef.rightChannelCollision.load()) {
        float s = processorRef.rightChannelCollisionSamples.load();
        collisionSamplesText = std::max(s, collisionSamplesText);

        collisionVisualTrigger = true;
        collisionDecay = 12;
        
        processorRef.rightChannelCollision.store(false);
    } 
    else if (collisionDecay > 0) {
        collisionDecay--;
        if (collisionDecay < 12) collisionVisualTrigger = false;
    } 
    else {
        collisionSamplesText = 0.0f;
        processorRef.rightChannelCollisionSamples = 0.0f;
    }


    waveformVisualizer.repaint();
    repaint();
}

void AudioPluginAudioProcessorEditor::setupKnob(juce::String paramID, juce::String paramName) {
    auto component = std::make_unique<GuiComponent>();

    component->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    component->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(component->slider);

    component->label.setText(paramName, juce::dontSendNotification);
    component->label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(component->label);

    component->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, paramID, component->slider);

    guiComponents.push_back(std::move(component));
}

void AudioPluginAudioProcessorEditor::setupToggle(juce::String paramID, juce::String buttonText) {
    auto component = std::make_unique<GuiComponent>();

    component->reverseButton.setButtonText(buttonText);
    addAndMakeVisible(component->reverseButton);

    component->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, paramID, component->reverseButton);

    guiComponents.push_back(std::move(component));
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p) {
    juce::ignoreUnused (processorRef);

    addAndMakeVisible(waveformVisualizer);

    setupKnob("splice", "Splice (ms)");
    setupKnob("delay", "Delay (ms)");
    setupKnob("density", "Density");
    setupKnob("pitch", "Pitch");
    setupKnob("spread", "Spread (s)");
    setupKnob("feedback", "Feedback");
    setupKnob("width", "Width");
    setupKnob("tone", "Tone");
    setupKnob("mix", "Mix");

    setupKnob("spliceOffset", "Splice Offset (%)");
    setupKnob("delayOffset", "Delay Offset (%)");
    setupKnob("pitchOffset", "Pitch Offset (cents)");

    setupToggle("reverse", "Reverse");

    setSize (700, 450);
    startTimerHz(60);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
    stopTimer();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colour (0xff0B0C0D));

    g.setColour(juce::Colour (0x88ffffff));
    if (collisionDecay > 0) {
        g.setColour(juce::Colours::red);
    }

    if (collisionVisualTrigger) {
        g.drawRect(getLocalBounds(), 5.0f);
    }

    g.setFont(18.0f);
    g.drawText(juce::String(collisionSamplesText, 1), 
                getLocalBounds().reduced(40), 
                juce::Justification::bottomRight);
}

void AudioPluginAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(20);

    auto waveformArea = area.removeFromTop(80); 
    waveformVisualizer.setBounds(waveformArea);
    
    area.removeFromTop(20);
    
    const int cols = 5;
    const int rows = 3;
    const int width = area.getWidth() / cols;
    const int height = area.getHeight() / rows;

    for (int i = 0; i < guiComponents.size(); ++i) {
        int column = i % cols;
        int row = i / cols;

        auto x = area.getX() + (column * width);
        auto y = area.getY() + (row * height);

        juce::Rectangle<int> slot (x, y, width, height);
        
        if (guiComponents[i]->slider.isVisible()) {
            guiComponents[i]->label.setBounds(slot.removeFromTop(20));
            guiComponents[i]->slider.setBounds(slot.reduced(5));
        } 
        else if (guiComponents[i]->reverseButton.isVisible()) {
            guiComponents[i]->reverseButton.setBounds(slot.reduced(10, 20));
        }
    }
}

