// Dev tool: load the built VST3 like a DAW would and stress it.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: hosttest <plugin.vst3>\n"); return 2; }

    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, juce::String (argv[1]));
    if (descs.isEmpty()) { std::printf ("FAIL: no plugin found\n"); return 1; }
    std::printf ("found: %s by %s\n", descs[0]->name.toRawUTF8(), descs[0]->manufacturerName.toRawUTF8());

    int fails = 0;
    juce::MemoryBlock savedState;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int bs : { 1, 17, 64, 256, 1024, 4096 })
        {
            juce::String err;
            auto inst = fmt.createInstanceFromDescription (*descs[0], sr, bs, err);
            if (! inst) { std::printf ("FAIL: create %s\n", err.toRawUTF8()); return 1; }
            inst->setPlayConfigDetails (2, 2, sr, bs);
            inst->prepareToPlay (sr, bs);

            auto& params = inst->getParameters();
            juce::Random rnd (42);
            juce::AudioBuffer<float> buf (2, bs);
            juce::MidiBuffer midi;
            float peak = 0; bool finite = true;
            const int blocks = (int) (sr * 2.0 / bs) + 1;
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < bs; ++i)
                {
                    const float v = 0.5f * std::sin ((float) (b * bs + i) * 0.03f) + 0.05f * (rnd.nextFloat() - 0.5f);
                    buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
                }
                if (b % 7 == 0) // automate random params
                    params[rnd.nextInt (params.size())]->setValue (rnd.nextFloat());
                inst->processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < bs; ++i)
                    {
                        const float s = buf.getSample (ch, i);
                        if (! std::isfinite (s)) finite = false;
                        peak = std::max (peak, std::abs (s));
                    }
            }
            if (! finite || peak > 4.0f) { std::printf ("FAIL sr=%.0f bs=%d finite=%d peak=%.2f\n", sr, bs, finite, peak); ++fails; }

            // state round trip
            juce::MemoryBlock st; inst->getStateInformation (st);
            inst->setStateInformation (st.getData(), (int) st.getSize());
            juce::MemoryBlock st2; inst->getStateInformation (st2);
            if (st != st2) { std::printf ("FAIL state round trip sr=%.0f bs=%d\n", sr, bs); ++fails; }
            savedState = st;

            if (sr == 48000.0 && bs == 256)
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
                if (! ed) { std::printf ("FAIL: no editor\n"); ++fails; }
                else std::printf ("editor %dx%d OK, %d params\n", ed->getWidth(), ed->getHeight(), params.size());
            }
            inst->releaseResources();
        }

    std::printf (fails ? "%d FAILURES\n" : "HOST TEST PASSED (5 rates x 6 block sizes, automation, state, editor)\n", fails);
    return fails ? 1 : 0;
}
