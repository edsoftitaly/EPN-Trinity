#include "PluginEditor.h"

namespace
{
    const juce::Colour kPanel     { 0xff141414 };
    const juce::Colour kEar       { 0xff1c1c1c };
    const juce::Colour kLine      { 0xff9a9a9a };
    const juce::Colour kText      { 0xffeeeeea };
    const juce::Colour kTextDim   { 0xffb9b9b4 };
    const juce::Colour kLedRed    { 0xffff3a24 };
    const juce::Colour kLedOff    { 0xff3a1410 };
    const juce::Colour kEpnOrange { 0xffe8892a };

    juce::Font uiFont (float size, bool bold = true, float hScale = 0.88f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                   .withHorizontalScale (hScale);
    }

    void drawLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size,
                    juce::Justification j = juce::Justification::centred, juce::Colour c = kText, bool bold = true)
    {
        g.setColour (c);
        g.setFont (uiFont (size, bold));
        g.drawFittedText (t, r, j, 1, 0.8f);
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a6a), c.x - r, c.y - r,
                                                 juce::Colour (0xff1e1e1e), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colour (0xff0a0a0a));
        g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.0f);
        g.setColour (juce::Colour (0xff111111));
        const float k = r * 0.6f;
        g.drawLine (c.x - k, c.y, c.x + k, c.y, 1.6f);
        g.drawLine (c.x, c.y - k, c.x, c.y + k, 1.6f);
    }

    // VU scale: equally spaced tick labels, piecewise-linear dB mapping
    const float kScaleDb[]   { -20, -15, -10, -7, -4, -2, -1, 0, 1, 2, 3, 6 };
    const char* kScaleTxt[]  { "-20", "-15", "-10", "-7", "-4", "-2", "-1", "0", "+1", "+2", "+3", "+6" };
    constexpr int kNumTicks = 12;

    float dbToProportion (float db)
    {
        if (db <= kScaleDb[0]) return 0.0f;
        for (int i = 1; i < kNumTicks; ++i)
            if (db <= kScaleDb[i])
            {
                const float t = (db - kScaleDb[i - 1]) / (kScaleDb[i] - kScaleDb[i - 1]);
                return ((float) (i - 1) + t) / (float) (kNumTicks - 1);
            }
        return 1.0f;
    }
}

//==============================================================================
void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto c = bounds.getCentre();
    const float rDots = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
    const float rSkirt = rDots - 9.0f;
    const float rCap = rSkirt * 0.74f;

    // scale dots
    g.setColour (kTextDim);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const auto p = c.getPointOnCircumference (rDots, a);
        const float d = (i == 0 || i == 10 || i == 5) ? 3.2f : 2.2f;
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }

    // shadow + skirt
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - rSkirt + 1.5f, c.y - rSkirt + 2.5f, 2 * rSkirt, 2 * rSkirt);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4a), c.x, c.y - rSkirt,
                                             juce::Colour (0xff121212), c.x, c.y + rSkirt, false));
    g.fillEllipse (c.x - rSkirt, c.y - rSkirt, 2 * rSkirt, 2 * rSkirt);

    // knurl ticks on skirt
    g.setColour (juce::Colour (0xff2a2a2a));
    for (int i = 0; i < 36; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (rCap + 1.0f, a),
                                       c.getPointOnCircumference (rSkirt - 0.5f, a)), 1.0f);
    }

    // ivory cap
    juce::ColourGradient cap (juce::Colour (0xfff4eedc), c.x - rCap * 0.5f, c.y - rCap * 0.6f,
                              juce::Colour (0xffb8ae94), c.x + rCap * 0.6f, c.y + rCap * 0.8f, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rCap, c.y - rCap, 2 * rCap, 2 * rCap);
    g.setColour (juce::Colour (0xff6d6553));
    g.drawEllipse (c.x - rCap, c.y - rCap, 2 * rCap, 2 * rCap, 1.0f);

    // pointer
    const float angle = startAngle + pos * (endAngle - startAngle);
    g.setColour (juce::Colour (0xff141414));
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (rCap * 0.25f, angle),
                                   c.getPointOnCircumference (rSkirt - 1.0f, angle)), 3.0f);
}

//==============================================================================
void Led::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = last;
    if (on)
    {
        g.setColour (kLedRed.withAlpha (0.35f));
        g.fillEllipse (b.expanded (0.0f));
    }
    const auto core = b.reduced (b.getWidth() * 0.18f);
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffffb0a0) : juce::Colour (0xff5a2a24),
                                             core.getCentreX() - 1, core.getY(),
                                             on ? kLedRed : kLedOff, core.getCentreX(), core.getBottom(), true));
    g.fillEllipse (core);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (core, 1.0f);
}

//==============================================================================
void SquareButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    // bezel
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (juce::Colour (0xff3c3c3c));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);

    auto capR = b.reduced (5.0f);
    if (down) capR = capR.translated (0.0f, 1.0f);
    const float shade = down ? 0.8f : (over ? 1.08f : 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8c8c8c).withMultipliedBrightness (shade), capR.getX(), capR.getY(),
                                             juce::Colour (0xff2c2c2c).withMultipliedBrightness (shade), capR.getRight(), capR.getBottom(), false));
    g.fillRoundedRectangle (capR, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5e5e5e), capR.getCentreX(), capR.getY() + 3,
                                             juce::Colour (0xff3a3a3a), capR.getCentreX(), capR.getBottom() - 3, false));
    g.fillRoundedRectangle (capR.reduced (3.0f), 3.0f);
}

void PowerButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    juce::ignoreUnused (over);
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (b, 3.0f);
    auto cap = b.reduced (4.0f);
    if (down) cap = cap.translated (0, 1);
    const bool on = getToggleState();
    const juce::Colour base = on ? juce::Colour (0xffe8321e) : juce::Colour (0xff6a1a12);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.35f), cap.getX(), cap.getY(),
                                             base.darker (0.6f), cap.getRight(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.0f);
    // rocker facets
    juce::Path facet;
    facet.addTriangle (cap.getX(), cap.getY(), cap.getRight(), cap.getY(), cap.getCentreX(), cap.getCentreY());
    g.setColour (juce::Colours::white.withAlpha (on ? 0.18f : 0.06f));
    g.fillPath (facet);
    if (on)
    {
        g.setColour (juce::Colour (0x40ff6040));
        g.drawRoundedRectangle (b.expanded (1.0f), 4.0f, 2.0f);
    }
}

//==============================================================================
void LedMeter::setLevelDb (float vuDb, bool active)
{
    if (std::abs (vuDb - level) > 0.05f || active != on)
    {
        level = vuDb;
        on = active;
        repaint();
    }
}

void LedMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff6f5a2c));
    g.drawRect (b, 1.5f);
    g.setColour (juce::Colour (0xff050505));
    g.fillRect (b.reduced (2.0f));

    const auto inner = b.reduced (4.0f, 4.0f);
    const float segW = inner.getWidth() / (float) numSegments;
    const float lit = on ? dbToProportion (level) * (float) numSegments : 0.0f;
    const int zeroSeg = (int) std::round (dbToProportion (0.0f) * numSegments);
    const int redSeg  = (int) std::round (dbToProportion (3.0f) * numSegments);

    for (int i = 0; i < numSegments; ++i)
    {
        juce::Colour c = i < zeroSeg ? juce::Colour (0xff3cff6a)
                       : i < redSeg  ? juce::Colour (0xffffc23a)
                                     : juce::Colour (0xffff3a24);
        const bool isOn = (float) i < lit;
        auto seg = juce::Rectangle<float> (inner.getX() + segW * (float) i + 0.8f, inner.getY(), segW - 1.6f, inner.getHeight());
        g.setColour (isOn ? c : c.withAlpha (0.10f));
        g.fillRect (seg);
    }
}

//==============================================================================
RackPanel::RackPanel (TrinityAudioProcessor& p) : proc (p)
{
    for (auto* b : { &padBtn, &effectBtn, &modeBtn, &presetBtn, &manualBtn })
        addAndMakeVisible (*b);
    addAndMakeVisible (powerBtn);

    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, ParamID::pad,       padBtn));
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, ParamID::effect,    effectBtn));
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, ParamID::stereo,    modeBtn));
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, ParamID::lfoPreset, presetBtn));
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, ParamID::lfoManual, manualBtn));
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, ParamID::power,     powerBtn));

    padBtn.setTooltip ("-12 dB input pad (more headroom, a touch more hiss)");
    effectBtn.setTooltip ("Effect on / bypass");
    modeBtn.setTooltip ("Chorus mode: stereo (lit) / mono");
    presetBtn.setTooltip ("Preset LFO: fixed slow sweep + vibrato");
    manualBtn.setTooltip ("Manual LFO: Rate + Intensity I/II/III");

    const char* ids[] { ParamID::intensity1, ParamID::intensity2, ParamID::intensity3, ParamID::rate };
    juce::Slider* knobs[] { &knob1, &knob2, &knob3, &rateKnob };
    for (int i = 0; i < 4; ++i)
    {
        auto& k = *knobs[i];
        k.setLookAndFeel (&lnf);
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        k.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<SA> (proc.apvts, ids[i], k));
        if (auto* prm = proc.apvts.getParameter (ids[i]))
            k.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }

    auto mk = [this] (std::function<bool()> f)
    {
        auto l = std::make_unique<Led> (std::move (f));
        addAndMakeVisible (*l);
        return l;
    };
    peakLed   = mk ([this] { return isPowered() && peakHold > 0; });
    effectLed = mk ([this] { return isPowered() && param (ParamID::effect); });
    modeLed   = mk ([this] { return isPowered() && param (ParamID::stereo); });
    presetLed = mk ([this] { return isPowered() && param (ParamID::lfoPreset); });
    manualLed = mk ([this] { return isPowered() && param (ParamID::lfoManual); });

    addAndMakeVisible (meterL);
    addAndMakeVisible (meterR);

    powerBtn.onClick = [this] { repaint(); };

    setSize (W, H); // last: resized() needs every child to exist
}

bool RackPanel::param (const char* id) const
{
    return proc.apvts.getRawParameterValue (id)->load() > 0.5f;
}

bool RackPanel::isPowered() const { return param (ParamID::power); }

void RackPanel::resized()
{
    const int top = 22, h = 186;
    secTitle = { 64, top, 170, h };
    int x = 234;
    auto take = [&] (int w) { juce::Rectangle<int> r (x, top, w, h); x += w; return r; };
    secPad    = take (92);
    secEffect = take (92);
    secMode   = take (104);
    secLfo    = take (156);
    secKnobs  = take (320);
    secMeter  = take (228);
    secPower  = take (80);
    secLogo   = { x, top, 1456 - x, h };

    auto placeSingle = [] (juce::Rectangle<int> s, juce::Component& btn, Led& led, int ledXOffset)
    {
        led.setBounds (s.getCentreX() + ledXOffset, s.getY() + 40, 16, 16);
        btn.setBounds (s.getCentreX() - 24, s.getY() + 92, 48, 48);
    };
    placeSingle (secPad,    padBtn,    *peakLed,   10);
    placeSingle (secEffect, effectBtn, *effectLed, 4);
    placeSingle (secMode,   modeBtn,   *modeLed,   18);

    const int lfoColW = secLfo.getWidth() / 2;
    presetLed->setBounds (secLfo.getX() + lfoColW / 2 + 4,           secLfo.getY() + 40, 16, 16);
    manualLed->setBounds (secLfo.getX() + lfoColW + lfoColW / 2 + 4, secLfo.getY() + 40, 16, 16);
    presetBtn.setBounds (secLfo.getX() + lfoColW / 2 - 24,           secLfo.getY() + 92, 48, 48);
    manualBtn.setBounds (secLfo.getX() + lfoColW + lfoColW / 2 - 24, secLfo.getY() + 92, 48, 48);

    const int kw = secKnobs.getWidth() / 4;
    juce::Slider* knobs[] { &knob1, &knob2, &knob3, &rateKnob };
    for (int i = 0; i < 4; ++i)
        knobs[i]->setBounds (secKnobs.getX() + kw * i + (kw - 72) / 2, secKnobs.getY() + 74, 72, 72);

    const int mx = secMeter.getX() + 44, mw = secMeter.getWidth() - 56;
    meterL.setBounds (mx, secMeter.getY() + 56, mw, 20);
    meterR.setBounds (mx, secMeter.getY() + 116, mw, 20);

    powerBtn.setBounds (secPower.getCentreX() - 26, secPower.getY() + 74, 52, 52);
}

void RackPanel::tick()
{
    const bool on = isPowered();
    const bool fxOn = on && param (ParamID::effect);
    juce::ignoreUnused (fxOn);

    if (proc.peakLit.exchange (false)) peakHold = 6; // ~200 ms at 30 Hz
    else if (peakHold > 0) --peakHold;

    // 0 VU = -18 dBFS RMS, VU-ish ballistics
    auto toVu = [] (float rms) { return 20.0f * std::log10 (juce::jmax (rms, 1.0e-6f)) + 18.0f; };
    const float tL = toVu (proc.meterL.exchange (0.0f));
    const float tR = toVu (proc.meterR.exchange (0.0f));
    smoothL += (tL - smoothL) * (tL > smoothL ? 0.55f : 0.18f);
    smoothR += (tR - smoothR) * (tR > smoothR ? 0.55f : 0.18f);
    meterL.setLevelDb (smoothL, on);
    meterR.setLevelDb (smoothR, on);

    for (auto* l : { peakLed.get(), effectLed.get(), modeLed.get(), presetLed.get(), manualLed.get() })
        l->refresh();
}

void RackPanel::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // rack ears
    g.setColour (kEar);
    g.fillRect (full);
    for (auto ear : { juce::Rectangle<float> (0, 0, 64, (float) H), juce::Rectangle<float> ((float) W - 64, 0, 64, (float) H) })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262626), ear.getX(), 0,
                                                 juce::Colour (0xff141414), ear.getRight(), 0, false));
        g.fillRect (ear);
        for (float yy : { 26.0f, (float) H - 44.0f })
        {
            juce::Rectangle<float> slot (ear.getCentreX() - 18, yy, 36, 18);
            g.setColour (juce::Colour (0xffd8d8d8));
            g.fillRoundedRectangle (slot, 9.0f);
            g.setColour (juce::Colour (0xff3a3a3a));
            g.drawRoundedRectangle (slot, 9.0f, 1.5f);
        }
    }

    // front panel
    auto face = juce::Rectangle<float> (64, 0, (float) W - 128, (float) H);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a1a), 0, 0, juce::Colour (0xff0e0e0e), 0, (float) H, false));
    g.fillRect (face);
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    for (int yy = 0; yy < H; yy += 3) g.drawHorizontalLine (yy, face.getX(), face.getRight()); // brushed hint
    g.setColour (juce::Colour (0xff050505));
    g.drawRect (face, 1.0f);

    drawScrew (g, { 84.0f, 44.0f }, 7.0f);
    drawScrew (g, { 84.0f, (float) H - 44.0f }, 7.0f);
    drawScrew (g, { (float) W - 84.0f, 44.0f }, 7.0f);
    drawScrew (g, { (float) W - 84.0f, (float) H - 44.0f }, 7.0f);

    // title block
    {
        auto t = secTitle.withTrimmedLeft (36);
        drawLabel (g, "TRINITY", t.withHeight (40).withY (t.getY() + 38), 34.0f, juce::Justification::centredLeft);
        drawLabel (g, "STEREO CHORUS", t.withHeight (20).withY (t.getY() + 80), 15.0f, juce::Justification::centredLeft);
        g.setColour (kEpnOrange);
        g.fillRect (t.getX(), t.getY() + 106, 104, 2);
        drawLabel (g, "T-3  BBD x3", t.withHeight (16).withY (t.getY() + 114), 12.0f, juce::Justification::centredLeft, kTextDim);
    }

    // section frames
    g.setColour (kLine);
    for (auto s : { secPad, secEffect, secMode, secLfo, secKnobs, secMeter, secPower })
        g.drawRect (s, 1);

    auto header = [] (juce::Rectangle<int> s) { return s.withHeight (22).withY (s.getY() + 8); };
    drawLabel (g, "-12dB PAD",    header (secPad),    15.0f);
    drawLabel (g, "EFFECT",       header (secEffect), 15.0f);
    drawLabel (g, "CHORUS MODE",  header (secMode),   15.0f);
    drawLabel (g, "LFO MODE",     header (secLfo),    15.0f);
    drawLabel (g, "OUT LEVEL",    header (secMeter),  15.0f);
    drawLabel (g, "POWER",        header (secPower),  15.0f);

    auto ledCaption = [&] (Led& led, const juce::String& txt)
    {
        auto lb = led.getBounds();
        drawLabel (g, txt, { lb.getX() - 58, lb.getY(), 54, lb.getHeight() }, 11.0f, juce::Justification::centredRight, kTextDim);
    };
    ledCaption (*peakLed,   "PEAK");
    ledCaption (*effectLed, "ON");
    ledCaption (*modeLed,   "STEREO");
    ledCaption (*presetLed, "ON");
    ledCaption (*manualLed, "ON");

    const int lfoColW = secLfo.getWidth() / 2;
    drawLabel (g, "PRESET", { secLfo.getX(),           secLfo.getY() + 150, lfoColW, 16 }, 12.0f);
    drawLabel (g, "MANUAL", { secLfo.getX() + lfoColW, secLfo.getY() + 150, lfoColW, 16 }, 12.0f);
    g.setColour (kLine.withAlpha (0.5f));
    g.drawVerticalLine (secLfo.getX() + lfoColW, (float) secLfo.getY() + 40, (float) secLfo.getBottom() - 14);

    // knobs: INTENSITY header bar over the first three, RATE over the 4th
    {
        const int kw = secKnobs.getWidth() / 4;
        auto hdr = header (secKnobs);
        auto intensityArea = juce::Rectangle<int> (secKnobs.getX() + 10, hdr.getY(), kw * 3 - 20, hdr.getHeight());
        drawLabel (g, "INTENSITY", intensityArea, 15.0f);
        g.setColour (kText);
        const int lineY = intensityArea.getCentreY();
        g.drawHorizontalLine (lineY, (float) intensityArea.getX() + 6, (float) intensityArea.getCentreX() - 46);
        g.drawHorizontalLine (lineY, (float) intensityArea.getCentreX() + 46, (float) intensityArea.getRight() - 6);
        drawLabel (g, "RATE", { secKnobs.getX() + kw * 3, hdr.getY(), kw, hdr.getHeight() }, 15.0f);

        const char* roman[] { "I", "II", "III" };
        juce::Slider* knobs[] { &knob1, &knob2, &knob3, &rateKnob };
        for (int i = 0; i < 4; ++i)
        {
            auto kb = knobs[i]->getBounds();
            if (i < 3)
                drawLabel (g, roman[i], { kb.getX(), kb.getY() - 22, kb.getWidth(), 16 }, 14.0f);
            const juce::String lo = i < 3 ? "MIN" : "SLOW", hi = i < 3 ? "MAX" : "FAST";
            drawLabel (g, lo, { kb.getX() - 2,      kb.getBottom() + 2, 34, 14 }, 10.5f, juce::Justification::centredLeft, kTextDim);
            drawLabel (g, hi, { kb.getRight() - 32, kb.getBottom() + 2, 34, 14 }, 10.5f, juce::Justification::centredRight, kTextDim);
        }
    }

    // meter captions + scale
    {
        drawLabel (g, "Ch.L", { secMeter.getX() + 6, meterL.getY(), 36, meterL.getHeight() }, 13.0f, juce::Justification::centredLeft);
        drawLabel (g, "Ch.R", { secMeter.getX() + 6, meterR.getY(), 36, meterR.getHeight() }, 13.0f, juce::Justification::centredLeft);
        const float x0 = (float) meterL.getX() + 4, x1 = (float) meterL.getRight() - 4;
        const int sy = meterL.getBottom() + 10;
        for (int i = 0; i < kNumTicks; ++i)
        {
            const float px = x0 + (x1 - x0) * (float) i / (float) (kNumTicks - 1);
            drawLabel (g, kScaleTxt[i], { (int) px - 12, sy, 24, 14 }, 9.5f, juce::Justification::centred,
                       i >= 7 ? juce::Colour (0xffffc23a) : kTextDim, false);
        }
        drawLabel (g, "dB", { meterL.getRight() - 20, sy + 14, 22, 12 }, 9.0f, juce::Justification::centredRight, kTextDim, false);
    }

    // EPN AUDIO logo block
    {
        auto lb = secLogo.reduced (14, 0);
        g.setColour (kEpnOrange);
        g.setFont (juce::Font (juce::FontOptions (46.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText ("EPN", lb.withY (secLogo.getY() + 46).withHeight (52), juce::Justification::centred);
        drawLabel (g, "A  U  D  I  O", lb.withY (secLogo.getY() + 98).withHeight (16), 13.0f, juce::Justification::centred);
        g.setColour (kEpnOrange.withAlpha (0.8f));
        g.fillRect (lb.getCentreX() - 40, secLogo.getY() + 120, 80, 1);
    }

    if (! isPowered())
    {
        // powered-down: everything slightly dimmer
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRect (face.withTrimmedLeft (170).withTrimmedRight (80 + 150));
    }
}

//==============================================================================
TrinityAudioProcessorEditor::TrinityAudioProcessorEditor (TrinityAudioProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    const float ratio = (float) RackPanel::W / (float) RackPanel::H;
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (RackPanel::W / 2, RackPanel::H / 2, RackPanel::W * 2, RackPanel::H * 2);
    setSize ((int) (RackPanel::W * 0.8f), (int) (RackPanel::H * 0.8f));
    startTimerHz (30);
}

TrinityAudioProcessorEditor::~TrinityAudioProcessorEditor()
{
    stopTimer();
}

void TrinityAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) RackPanel::W;
    panel.setTransform (juce::AffineTransform::scale (s));
}
