#include "PluginProcessor.h"
#include "PluginEditor.h"

RoomBleedAudioProcessorEditor::RoomBleedAudioProcessorEditor (RoomBleedAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (550, 680);
    
    // Lambda for setting up vertical sliders
    auto setupSlider = [this](juce::Slider& s, juce::String pid, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, pid, s);
    };

    setupSlider(bleedSlider, "MIX", bleedAtt);
    setupSlider(spaceSlider, "SPACE", spaceAtt);
    setupSlider(locutSlider, "LOCUT", locutAtt);
    setupSlider(hicutSlider, "HICUT", hicutAtt);

    // Extra Gain Knob setup
    outputGainSlider.setLookAndFeel(&outboardLF);
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(outputGainSlider);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "EXTRAGAIN", outputGainSlider);

    // Room Selector Label
    roomTypeLabel.setText("Room Type", juce::dontSendNotification);
    roomTypeLabel.setFont(juce::Font("Helvetica", 14.0f, juce::Font::bold));
    roomTypeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(roomTypeLabel);

    // Room Selector Dropdown (Added "None" at the start)
    roomSelector.addItemList({ "None", "Living Room", "Studio", "Garage", "Concert Hall", "Club", "Parking Garage", "Football Field", "Arena", "Hallway", "Bathroom", "Small Closet", "Large Ballroom", "Outer Space", "Phone Booth", "Concrete Pipe", "Deep Well", "Cathedral", "Inside a Guitar", "Nuclear Silo", "Underwater" }, 1);
    addAndMakeVisible(roomSelector);
    roomAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.treeState, "ROOM", roomSelector);

    // Instructions Button (Renamed from Manual)
    instructionsButton.setButtonText("Instructions");
    instructionsButton.onClick = [this]() {
        juce::String myInstructions = "1) Use the sidechain function to pick your input source.\n"
                                      "2) Pick a room type.\n"
                                      "3) Use the distance slider to determine how far away you want the sound source to be.\n\n"
                                      "Use mix, Lo-cut and Hi-cut to your liking. There is 10db of extra gain that ONLY effects the wet, side-chained signal should you need it.\n\n"
                                      "Ex) Put the Room Bleed plugin on a guitar track. Select your drum bus in the side chain section. Envision the room setting - let's say a studio setting where the drums are about 20 ft. away from the guitar. Use the mix knob accordingly.\n\n"
                                      "NOTE - When soloing or muting tracks, consider routing logic. For example, if separate drum tracks are sends-only to the drum bus, in the case mentioned above, when the guitar track is soloed nothing will be heard. Therefore, it is recommended to use this plugin in the context of the whole mix to glue instruments together.";
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Room Bleed Instructions", myInstructions, "Got it");
    };
    addAndMakeVisible(instructionsButton);
}

RoomBleedAudioProcessorEditor::~RoomBleedAudioProcessorEditor()
{
    outputGainSlider.setLookAndFeel(nullptr);
}

void RoomBleedAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background and Header
    g.fillAll (juce::Colour(0xff3c4a52));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Copperplate", 36.0f, juce::Font::bold));
    g.drawText("ROOM BLEED", 0, 20, getWidth(), 40, juce::Justification::centred);
    g.setFont(juce::Font("Helvetica", 14.0f, juce::Font::italic));
    g.drawText("by Jesse Shafer", 0, 55, getWidth(), 20, juce::Justification::centred);

    // Extra Gain Markings
    auto gb = outputGainSlider.getBounds();
    g.setFont(12.0f);
    g.drawText("0dB", gb.getX() - 35, gb.getBottom() - 30, 35, 20, juce::Justification::right);
    g.drawText("+10dB", gb.getRight(), gb.getBottom() - 30, 45, 20, juce::Justification::left);
    
    // Fixed "Extra Gain" Label (Widened bounding box to 120 and centered to prevent truncation)
    g.setFont(14.0f);
    g.drawText("EXTRA GAIN", gb.getCentreX() - 60, gb.getBottom() + 5, 120, 20, juce::Justification::centred);

    // Slider Scales
    g.setFont(11.0f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    auto drawScale = [&](juce::Slider& s, juce::String t, juce::String b) {
        auto r = s.getBounds();
        g.drawText(t, r.getX()-45, r.getY()-7, 40, 14, juce::Justification::right);
        g.drawText(b, r.getX()-45, r.getBottom()-7, 40, 14, juce::Justification::right);
    };
    drawScale(bleedSlider, "0dB", "-60dB");
    drawScale(spaceSlider, "50ft", "0ft");
    drawScale(locutSlider, "2kHz", "20Hz");
    drawScale(hicutSlider, "20kHz", "500Hz");

    // Distance Arrow Icon
    auto sb = spaceSlider.getBounds().toFloat();
    float centerX = sb.getX() - 18.0f;
    float centerY = sb.getCentreY();
    juce::Path p;
    p.addArrow (juce::Line<float> (centerX, centerY + 15.0f, centerX, centerY - 15.0f), 2.0f, 6.0f, 6.0f);
    g.setColour(juce::Colours::white);
    g.fillPath(p);

    // Main Control Labels
    g.setFont(14.0f);
    g.drawText("MIX", bleedSlider.getX(), bleedSlider.getBottom() + 5, bleedSlider.getWidth(), 20, juce::Justification::centred);
    g.drawText("DISTANCE", spaceSlider.getX(), spaceSlider.getBottom() + 5, spaceSlider.getWidth(), 20, juce::Justification::centred);
    g.drawText("LO-CUT", locutSlider.getX(), locutSlider.getBottom() + 5, locutSlider.getWidth(), 20, juce::Justification::centred);
    g.drawText("HI-CUT", hicutSlider.getX(), hicutSlider.getBottom() + 5, hicutSlider.getWidth(), 20, juce::Justification::centred);
}

void RoomBleedAudioProcessorEditor::resized()
{
    // Layout variables
    int sw = 60, sh = 350, sy = 140, sp = 110;
    
    // Slider Positioning
    bleedSlider.setBounds (60, sy, sw, sh);
    spaceSlider.setBounds (60 + sp, sy, sw, sh);
    locutSlider.setBounds (60 + sp * 2, sy, sw, sh);
    hicutSlider.setBounds (60 + sp * 3, sy, sw, sh);
    
    // Knob Positioning
    outputGainSlider.setBounds(60 + sp * 1.5, sy + sh + 40, 80, 80);
    
    // Room Selector & Label Positioning
    roomTypeLabel.setBounds(20, 75, 200, 20);
    roomSelector.setBounds(20, 95, 200, 25);
    
    // Top Right Buttons
    instructionsButton.setBounds(getWidth() - 110, 20, 95, 30);
}
