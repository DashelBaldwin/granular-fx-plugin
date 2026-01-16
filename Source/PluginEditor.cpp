#include "PluginProcessor.h"
#include "PluginEditor.h"

void AudioPluginAudioProcessorEditor::setupKnob(juce::String paramID, juce::String paramName) {
    auto component = std::make_unique<GuiComponent>();

    // Knob
    component->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    component->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(component->slider);

    // Label
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
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AudioPluginAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(20);
    
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
