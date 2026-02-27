#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class OutboardLF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toX = bounds.getCentreX();
        auto toY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colour(0xff181818));
        g.fillEllipse(toX - radius, toY - radius, radius * 2.0f, radius * 2.0f);
        
        g.setColour(juce::Colours::black);
        g.drawEllipse(toX - radius, toY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        juce::Path p;
        auto pointerLength = radius * 0.85f;
        auto pointerThickness = 3.5f;
        p.addRoundedRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength, 1.0f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(toX, toY));
        
        g.setColour(juce::Colours::white);
        g.fillPath(p);
    }
};

class RoomBleedAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    RoomBleedAudioProcessorEditor (RoomBleedAudioProcessor&);
    ~RoomBleedAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RoomBleedAudioProcessor& audioProcessor;
    OutboardLF outboardLF;

    juce::Slider bleedSlider, spaceSlider, locutSlider, hicutSlider, outputGainSlider;
    juce::ComboBox roomSelector;
    juce::Label roomTypeLabel; // Added Label for Room Type
    juce::TextButton instructionsButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedAtt, spaceAtt, locutAtt, hicutAtt, gainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roomAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomBleedAudioProcessorEditor)
};
