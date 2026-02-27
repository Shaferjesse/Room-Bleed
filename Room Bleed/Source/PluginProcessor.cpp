#include "PluginProcessor.h"
#include "PluginEditor.h"

RoomBleedAudioProcessor::RoomBleedAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
       treeState (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

RoomBleedAudioProcessor::~RoomBleedAudioProcessor() {}

void RoomBleedAudioProcessor::reset()
{
    delayLine.reset();
    reverb.reset();
    lowcutFilter.reset();
    hicutFilter.reset();
    airAbsorptionFilter.reset();
    bleedBuffer.clear();
}

juce::AudioProcessorValueTreeState::ParameterLayout RoomBleedAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", -60.0f, 0.0f, -6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LOCUT", "Low-cut", juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("HICUT", "Hi-cut", juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SPACE", "Distance", 0.0f, 50.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EXTRAGAIN", "Extra Gain", 0.0f, 10.0f, 0.0f));
    
    // "None" added to the beginning. Total of 21 options now.
    juce::StringArray roomChoices { "None", "Living Room", "Studio", "Garage", "Concert Hall", "Club", "Parking Garage", "Football Field", "Arena", "Hallway", "Bathroom", "Small Closet", "Large Ballroom", "Outer Space", "Phone Booth", "Concrete Pipe", "Deep Well", "Cathedral", "Inside a Guitar", "Nuclear Silo", "Underwater" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("ROOM", "Room Type", roomChoices, 1)); // Defaults to "Living Room"
    
    return { params.begin(), params.end() };
}

void RoomBleedAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 2;

    delayLine.prepare(spec);
    lowcutFilter.prepare(spec);
    lowcutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    hicutFilter.prepare(spec);
    hicutFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    airAbsorptionFilter.prepare(spec);
    airAbsorptionFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    reverb.prepare(spec);
    bleedBuffer.setSize(2, samplesPerBlock);
    
    mixGain.reset(sampleRate, 0.05);
    delaySmoother.reset(sampleRate, 0.1);
    distanceAttenuation.reset(sampleRate, 0.1);
    extraSidechainGain.reset(sampleRate, 0.05);
    reset();
}

void RoomBleedAudioProcessor::updateRoomProfile()
{
    int type = static_cast<int>(treeState.getRawParameterValue("ROOM")->load());
    juce::dsp::Reverb::Parameters p;
    
    if (type == 0) {
        // "None" selected. Reverb acts as a pure dry passthrough.
        p.dryLevel = 1.0f;
        p.wetLevel = 0.0f;
        p.roomSize = 0.0f; p.damping = 0.0f; p.width = 0.0f;
    } else {
        // Normal room processing
        p.dryLevel = 0.0f;
        p.wetLevel = 1.0f;
        switch (type) {
            case 1:  p.roomSize = 0.42f; p.damping = 0.58f; p.width = 0.68f; break; // Living Room
            case 2:  p.roomSize = 0.16f; p.damping = 0.88f; p.width = 0.32f; break; // Studio
            case 3:  p.roomSize = 0.58f; p.damping = 0.32f; p.width = 0.82f; break; // Garage
            case 4:  p.roomSize = 0.94f; p.damping = 0.28f; p.width = 1.00f; break; // Concert Hall
            case 5:  p.roomSize = 0.72f; p.damping = 0.48f; p.width = 0.88f; break; // Club
            case 6:  p.roomSize = 0.86f; p.damping = 0.18f; p.width = 0.72f; break; // Parking Garage
            case 7:  p.roomSize = 0.99f; p.damping = 0.78f; p.width = 1.00f; break; // Football Field
            case 8:  p.roomSize = 0.96f; p.damping = 0.42f; p.width = 0.96f; break; // Arena
            case 9:  p.roomSize = 0.62f; p.damping = 0.44f; p.width = 0.12f; break; // Hallway
            case 10: p.roomSize = 0.28f; p.damping = 0.12f; p.width = 0.65f; break; // Bathroom
            case 11: p.roomSize = 0.07f; p.damping = 0.97f; p.width = 0.12f; break; // Small Closet
            case 12: p.roomSize = 0.89f; p.damping = 0.46f; p.width = 0.92f; break; // Large Ballroom
            case 13: p.roomSize = 1.00f; p.damping = 0.05f; p.width = 1.00f; break; // Outer Space
            case 14: p.roomSize = 0.04f; p.damping = 0.72f; p.width = 0.18f; break; // Phone Booth
            case 15: p.roomSize = 0.64f; p.damping = 0.08f; p.width = 0.32f; break; // Concrete Pipe
            case 16: p.roomSize = 0.82f; p.damping = 0.04f; p.width = 0.38f; break; // Deep Well
            case 17: p.roomSize = 0.98f; p.damping = 0.32f; p.width = 1.00f; break; // Cathedral
            case 18: p.roomSize = 0.32f; p.damping = 0.38f; p.width = 0.96f; break; // Inside a Guitar
            case 19: p.roomSize = 0.97f; p.damping = 0.08f; p.width = 0.82f; break; // Nuclear Silo
            case 20: p.roomSize = 0.62f; p.damping = 1.00f; p.width = 0.38f; break; // Underwater
        }
    }
    reverb.setParameters(p);
}

void RoomBleedAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);
    int numSamples = buffer.getNumSamples();
    bleedBuffer.clear();

    if (sidechainBuffer.getNumChannels() > 0) {
        float distFt = treeState.getRawParameterValue("SPACE")->load();
        
        float delaySamples = (distFt / 1130.0f) * (float)getSampleRate();
        delaySmoother.setTargetValue(delaySamples);
        
        float atten = 1.0f / (1.0f + distFt);
        distanceAttenuation.setTargetValue(atten);

        float airCutoff = 20000.0f / (1.0f + (distFt * 0.15f));
        airAbsorptionFilter.setCutoffFrequency(juce::jlimit(20.0f, 20000.0f, airCutoff));

        lowcutFilter.setCutoffFrequency(treeState.getRawParameterValue("LOCUT")->load());
        hicutFilter.setCutoffFrequency(treeState.getRawParameterValue("HICUT")->load());
        
        updateRoomProfile();

        for (int s = 0; s < numSamples; ++s) {
            delayLine.setDelay(delaySmoother.getNextValue());
            float curDistGain = distanceAttenuation.getNextValue();
            for (int ch = 0; ch < 2; ++ch) {
                float scIn = sidechainBuffer.getSample(juce::jmin(ch, sidechainBuffer.getNumChannels() - 1), s);
                delayLine.pushSample(ch, scIn);
                float sig = delayLine.popSample(ch) * curDistGain;
                sig = airAbsorptionFilter.processSample(ch, sig);
                sig = lowcutFilter.processSample(ch, sig);
                sig = hicutFilter.processSample(ch, sig);
                bleedBuffer.setSample(ch, s, sig);
            }
        }
        juce::dsp::AudioBlock<float> block(bleedBuffer);
        reverb.process(juce::dsp::ProcessContextReplacing<float>(block));
    }

    mixGain.setTargetValue(juce::Decibels::decibelsToGain(treeState.getRawParameterValue("MIX")->load()));
    float extraDb = treeState.getRawParameterValue("EXTRAGAIN")->load();
    extraSidechainGain.setTargetValue(juce::Decibels::decibelsToGain(extraDb));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* mainOut = buffer.getWritePointer(ch);
        const float* wetSrc = bleedBuffer.getReadPointer(juce::jmin(ch, 1));
        for (int s = 0; s < numSamples; ++s) {
            float wetContribution = wetSrc[s] * mixGain.getNextValue() * extraSidechainGain.getNextValue();
            mainOut[s] = juce::jlimit(-1.0f, 1.0f, mainOut[s] + wetContribution);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RoomBleedAudioProcessor(); }
bool RoomBleedAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* RoomBleedAudioProcessor::createEditor() { return new RoomBleedAudioProcessorEditor (*this); }
const juce::String RoomBleedAudioProcessor::getName() const { return JucePlugin_Name; }
bool RoomBleedAudioProcessor::acceptsMidi() const { return false; }
bool RoomBleedAudioProcessor::producesMidi() const { return false; }
bool RoomBleedAudioProcessor::isMidiEffect() const { return false; }
double RoomBleedAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int RoomBleedAudioProcessor::getNumPrograms() { return 1; }
int RoomBleedAudioProcessor::getCurrentProgram() { return 0; }
void RoomBleedAudioProcessor::setCurrentProgram (int index) {}
const juce::String RoomBleedAudioProcessor::getProgramName (int index) { return {}; }
void RoomBleedAudioProcessor::changeProgramName (int index, const juce::String& newName) {}
void RoomBleedAudioProcessor::releaseResources() {}
bool RoomBleedAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const { return true; }
void RoomBleedAudioProcessor::getStateInformation (juce::MemoryBlock& d) { auto s = treeState.copyState(); std::unique_ptr<juce::XmlElement> x (s.createXml()); copyXmlToBinary (*x, d); }
void RoomBleedAudioProcessor::setStateInformation (const void* d, int s) { std::unique_ptr<juce::XmlElement> x (getXmlFromBinary (d, s)); if (x != nullptr) treeState.replaceState (juce::ValueTree::fromXml (*x)); }
