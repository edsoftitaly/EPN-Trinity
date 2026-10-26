#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
class Led : public juce::Component
{
public:
    explicit Led (std::function<bool()> isLit) : lit (std::move (isLit)) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    void refresh() { const bool now = lit(); if (now != last) { last = now; repaint(); } }
private:
    std::function<bool()> lit;
    bool last = false;
};

//==============================================================================
class SquareButton : public juce::Button
{
public:
    SquareButton() : juce::Button ({}) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ({}) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

//==============================================================================
class LedMeter : public juce::Component
{
public:
    void setLevelDb (float vuDb, bool active);
    void paint (juce::Graphics&) override;
    static constexpr int numSegments = 24;
private:
    float level = -40.0f;
    bool on = true;
};

//==============================================================================
class RackPanel : public juce::Component
{
public:
    static constexpr int W = 1520, H = 230;

    explicit RackPanel (TrinityAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();

private:
    TrinityAudioProcessor& proc;
    RackLookAndFeel lnf;

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    SquareButton padBtn, effectBtn, modeBtn, presetBtn, manualBtn;
    PowerButton powerBtn;
    juce::Slider knob1, knob2, knob3, rateKnob;
    LedMeter meterL, meterR;

    std::unique_ptr<Led> peakLed, effectLed, modeLed, presetLed, manualLed;
    std::vector<std::unique_ptr<BA>> buttonAtts;
    std::vector<std::unique_ptr<SA>> sliderAtts;

    float smoothL = -40.0f, smoothR = -40.0f;
    int peakHold = 0;

    bool isPowered() const;
    bool param (const char* id) const;

    // layout rectangles (panel coordinates)
    juce::Rectangle<int> secPad, secEffect, secMode, secLfo, secKnobs, secMeter, secPower, secLogo, secTitle;
};

//==============================================================================
class TrinityAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TrinityAudioProcessorEditor (TrinityAudioProcessor&);
    ~TrinityAudioProcessorEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0d0d0d)); }
    void pumpMeters() { panel.tick(); } // used by the headless snapshot tool

private:
    void timerCallback() override { panel.tick(); }
    RackPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrinityAudioProcessorEditor)
};
