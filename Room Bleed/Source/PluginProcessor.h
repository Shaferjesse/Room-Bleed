#pragma once
#include <JuceHeader.h>

class RoomBleedAudioProcessor  : public juce::AudioProcessor
{
public:
    RoomBleedAudioProcessor();
    ~RoomBleedAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState treeState;

private:
    void updateRoomProfile();
    
    juce::dsp::DelayLine<float> delayLine { 192000 };
    juce::dsp::StateVariableTPTFilter<float> lowcutFilter, hicutFilter, airAbsorptionFilter;
    juce::dsp::Reverb reverb;
    
    juce::AudioBuffer<float> bleedBuffer;
    juce::SmoothedValue<float> mixGain, delaySmoother, distanceAttenuation, extraSidechainGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomBleedAudioProcessor)
};
