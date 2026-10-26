#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TriChorusDSP.h"

namespace ParamID
{
    inline constexpr auto power      = "power";
    inline constexpr auto pad        = "pad";
    inline constexpr auto effect     = "effect";
    inline constexpr auto stereo     = "stereo";
    inline constexpr auto lfoPreset  = "lfoPreset";
    inline constexpr auto lfoManual  = "lfoManual";
    inline constexpr auto intensity1 = "intensity1";
    inline constexpr auto intensity2 = "intensity2";
    inline constexpr auto intensity3 = "intensity3";
    inline constexpr auto rate       = "rate";
}

class TrinityAudioProcessor : public juce::AudioProcessor
{
public:
    TrinityAudioProcessor();
    ~TrinityAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // GUI feedback
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f }; // RMS, linear
    std::atomic<bool>  peakLit { false };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    epn::TriChorusEngine engine;

    struct Cached
    {
        std::atomic<float>* power; std::atomic<float>* pad; std::atomic<float>* effect;
        std::atomic<float>* stereo; std::atomic<float>* lfoPreset; std::atomic<float>* lfoManual;
        std::atomic<float>* i1; std::atomic<float>* i2; std::atomic<float>* i3; std::atomic<float>* rate;
    } p {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrinityAudioProcessor)
};
