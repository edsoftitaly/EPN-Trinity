#include "PluginProcessor.h"
#include "PluginEditor.h"

TrinityAudioProcessor::TrinityAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRINITY", createLayout())
{
    p.power     = apvts.getRawParameterValue (ParamID::power);
    p.pad       = apvts.getRawParameterValue (ParamID::pad);
    p.effect    = apvts.getRawParameterValue (ParamID::effect);
    p.stereo    = apvts.getRawParameterValue (ParamID::stereo);
    p.lfoPreset = apvts.getRawParameterValue (ParamID::lfoPreset);
    p.lfoManual = apvts.getRawParameterValue (ParamID::lfoManual);
    p.i1        = apvts.getRawParameterValue (ParamID::intensity1);
    p.i2        = apvts.getRawParameterValue (ParamID::intensity2);
    p.i3        = apvts.getRawParameterValue (ParamID::intensity3);
    p.rate      = apvts.getRawParameterValue (ParamID::rate);
}

juce::AudioProcessorValueTreeState::ParameterLayout TrinityAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto onOff = [] (bool v, int) { return v ? String ("On") : String ("Off"); };

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::power, 1 }, "Power", true,
                    AudioParameterBoolAttributes().withStringFromValueFunction (onOff)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::pad, 1 }, "-12dB Pad", false,
                    AudioParameterBoolAttributes().withStringFromValueFunction (onOff)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::effect, 1 }, "Effect", true,
                    AudioParameterBoolAttributes().withStringFromValueFunction (onOff)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::stereo, 1 }, "Chorus Mode", true,
                    AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? String ("Stereo") : String ("Mono"); })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::lfoPreset, 1 }, "LFO Preset", true,
                    AudioParameterBoolAttributes().withStringFromValueFunction (onOff)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::lfoManual, 1 }, "LFO Manual", false,
                    AudioParameterBoolAttributes().withStringFromValueFunction (onOff)));

    auto knob = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                    NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); }));
    };
    layout.add (knob (ParamID::intensity1, "Intensity I",   5.0f));
    layout.add (knob (ParamID::intensity2, "Intensity II",  5.0f));
    layout.add (knob (ParamID::intensity3, "Intensity III", 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::rate, 1 }, "Rate",
                    NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.5f,
                    AudioParameterFloatAttributes().withLabel ("Hz")
                        .withStringFromValueFunction ([] (float v, int)
                        {
                            const float hz = epn::TriChorusEngine::rateToHz (v / 10.0f);
                            return String (hz, hz < 1.0f ? 2 : 1) + " Hz";
                        })));
    return layout;
}

bool TrinityAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TrinityAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    engine.prepare (sampleRate);
}

void TrinityAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, n); // mono in -> duplicate to R

    epn::TriChorusParams prm;
    prm.power     = p.power->load() > 0.5f;
    prm.pad       = p.pad->load() > 0.5f;
    prm.effectOn  = p.effect->load() > 0.5f;
    prm.stereo    = p.stereo->load() > 0.5f;
    prm.lfoPreset = p.lfoPreset->load() > 0.5f;
    prm.lfoManual = p.lfoManual->load() > 0.5f;
    prm.intensity[0] = p.i1->load() * 0.1f;
    prm.intensity[1] = p.i2->load() * 0.1f;
    prm.intensity[2] = p.i3->load() * 0.1f;
    prm.rate = p.rate->load() * 0.1f;
    engine.setParams (prm);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (numOut > 1 ? 1 : 0);
    engine.process (L, R, L, R, n);

    if (engine.consumePeak() && prm.power)
        peakLit.store (true);

    // meters: keep the loudest block RMS until the editor reads (and resets) it
    const float rmsL = buffer.getRMSLevel (0, 0, n);
    const float rmsR = buffer.getRMSLevel (numOut > 1 ? 1 : 0, 0, n);
    if (rmsL > meterL.load()) meterL.store (rmsL);
    if (rmsR > meterR.load()) meterR.store (rmsR);
}

void TrinityAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TrinityAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TrinityAudioProcessor::createEditor()
{
    return new TrinityAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrinityAudioProcessor();
}
