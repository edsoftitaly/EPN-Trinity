// Dev tool: instantiate the processor + editor and save a PNG of the GUI.
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("trinity.png"));
    const bool powerOff = argc > 2 && juce::String (argv[2]) == "off";

    TrinityAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    if (powerOff)
        proc.apvts.getParameter (ParamID::power)->setValueNotifyingHost (0.0f);
    else
    {
        proc.apvts.getParameter (ParamID::lfoManual)->setValueNotifyingHost (1.0f);
        proc.apvts.getParameter (ParamID::intensity2)->setValueNotifyingHost (0.8f);
        proc.apvts.getParameter (ParamID::rate)->setValueNotifyingHost (0.3f);
    }

    // feed a little audio so the meters show something
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 40; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = 0.2f * std::sin (0.05f * (float) (b * 512 + i));
            buf.setSample (0, i, v); buf.setSample (1, i, v);
        }
        proc.processBlock (buf, midi);
    }
    proc.peakLit = true;

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (RackPanel::W, RackPanel::H);
    for (int i = 0; i < 12; ++i)
    {
        proc.meterL = 0.12f; proc.meterR = 0.09f;
        static_cast<TrinityAudioProcessorEditor*> (ed.get())->pumpMeters();
    }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
