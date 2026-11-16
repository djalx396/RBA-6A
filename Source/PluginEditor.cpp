#include "PluginEditor.h"

using namespace juce;

static String u8 (const char* t) { return String::fromUTF8 (t); }
static Font font (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
static void textSpaced (Graphics& g, const String& s, Rectangle<int> r, Justification j, float h, bool bold, Colour c)
{
    g.setColour (c); g.setFont (font (h, bold).withExtraKerningFactor (0.08f)); g.drawText (s, r, j, false);
}

// ------------------------------------------------------------------------------------ look
void RbaLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const float d = (float) jmin (w, h) - 4.f;
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float alpha = s.isEnabled() ? 1.f : 0.4f;
    g.setColour (ui::knob.withMultipliedAlpha (alpha)); g.fillEllipse (c.x - d / 2, c.y - d / 2, d, d);
    g.setColour (ui::knobEdge.withMultipliedAlpha (alpha)); g.drawEllipse (c.x - d / 2, c.y - d / 2, d, d, 2.f);
    const float ang = a0 + pos * (a1 - a0);
    Path p; p.addRoundedRectangle (-1.5f, -d / 2 + 5.f, 3.f, d * 0.32f, 1.5f);
    g.setColour (ui::accent.withMultipliedAlpha (alpha));
    g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
}

// ------------------------------------------------------------------------------- segmented
Segmented::Segmented (RangedAudioParameter& p, StringArray l, Array<int> v) : param (p), labels (l), values (v)
{
    if (values.isEmpty()) for (int i = 0; i < labels.size(); ++i) values.add (i);
    startTimerHz (15);
}
int Segmented::current() const
{
    const float norm = param.getValue();
    const int idx = roundToInt (param.convertFrom0to1 (norm));
    return values.indexOf (idx);
}
void Segmented::paint (Graphics& g)
{
    const int n = visibleCount > 0 ? visibleCount : labels.size();
    const bool en = ! isEnabledFn || isEnabledFn();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colour (0xff4a443d)); g.drawRoundedRectangle (r, 6.f, 1.f);
    const int cur = current();
    const float w = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto b = Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (en && i == cur)
        {
            Path p; p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), 6.f, 6.f, i == 0, i == n - 1, i == 0, i == n - 1);
            g.setColour (ui::accent); g.fillPath (p);
        }
        if (i > 0) { g.setColour (ui::border); g.drawVerticalLine ((int) b.getX(), b.getY() + 1, b.getBottom() - 1); }
        g.setColour (! en ? ui::off : (i == cur ? Colour (0xff1a1612) : ui::cream));
        g.setFont (font (13.f, true).withExtraKerningFactor (0.04f));
        g.drawFittedText (labels[i].toUpperCase(), b.toNearestInt().reduced (3, 0), Justification::centred, 1, 0.75f);
    }
}
void Segmented::mouseUp (const MouseEvent& e)
{
    if (isEnabledFn && ! isEnabledFn()) return;
    const int n = visibleCount > 0 ? visibleCount : labels.size();
    const int i = jlimit (0, n - 1, (int) ((float) e.x / (float) getWidth() * (float) n));
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.convertTo0to1 ((float) values[i]));
    param.endChangeGesture();
    repaint();
}

// ------------------------------------------------------------------------------------ knob
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& t, const String& sc, int sz)
    : title (t), scale (sc), size (sz)
{
    slider.setSliderStyle (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    param = s.getParameter (id);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    startTimerHz (15);
}
void Knob::resized() { slider.setBounds ((getWidth() - size) / 2, 18, size, size); }
void Knob::timerCallback()
{
    const bool en = ! isEnabledFn || isEnabledFn();
    if (slider.isEnabled() != en) slider.setEnabled (en);
    repaint();
}
void Knob::paint (Graphics& g)
{
    textSpaced (g, title.toUpperCase(), { 0, 0, getWidth(), 14 }, Justification::centred, 12.f, true, ui::label);
    const String v = valueText ? valueText() : param->getCurrentValueAsText();
    g.setColour (ui::cream); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 12.f, Font::plain)));
    g.drawText (v, 0, 20 + size, getWidth(), 16, Justification::centred, false);
    const String sc = scaleFn ? scaleFn() : scale;
    if (sc.isNotEmpty())
    {
        g.setColour (ui::dim); g.setFont (font (11.f));
        g.drawFittedText (sc, 0, 37 + size, getWidth(), 14, Justification::centred, 1, 0.8f);
    }
}

// --------------------------------------------------------------------------------- GR meter
static float meterAngle (float db)
{
    const float f = std::pow (jlimit (0.f, 20.f, db) / 20.f, 0.6f);
    return degreesToRadians (335.f - 130.f * f);
}
void GrMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0f0d0b)); g.fillRoundedRectangle (r, 8.f);
    auto f = r.reduced (3.f);
    g.setColour (ui::face); g.fillRoundedRectangle (f, 6.f);
    const float sx = f.getWidth() / 300.f, sy = f.getHeight() / 150.f;
    auto P = [&] (float x, float y) { return Point<float> (f.getX() + x * sx, f.getY() + y * sy); };
    const float cx = 150.f, cy = 150.f, R = 118.f;
    Path arc; arc.addCentredArc (P (cx, cy).x, P (cx, cy).y, R * sx, R * sy, 0.f, degreesToRadians (205.f + 90.f), degreesToRadians (335.f + 90.f), true);
    g.setColour (ui::ink); g.strokePath (arc, PathStrokeType (1.5f));
    const float majors[] = { 0, 1, 2, 3, 5, 7, 10, 15, 20 };
    g.setFont (font (12.f));
    for (float d : majors)
    {
        const float a = meterAngle (d);
        g.drawLine (Line<float> (P (cx + R * std::cos (a), cy + R * std::sin (a)), P (cx + (R - 12) * std::cos (a), cy + (R - 12) * std::sin (a))), 2.f);
        const auto t = P (cx + (R + 14) * std::cos (a), cy + (R + 14) * std::sin (a));
        g.drawText (String ((int) d), Rectangle<float> (t.x - 12, t.y - 7, 24, 14), Justification::centred, false);
    }
    const float minors[] = { 0.5f, 1.5f, 2.5f, 4, 6, 8.5f, 12.5f, 17.5f };
    for (float d : minors)
    {
        const float a = meterAngle (d);
        g.drawLine (Line<float> (P (cx + R * std::cos (a), cy + R * std::sin (a)), P (cx + (R - 7) * std::cos (a), cy + (R - 7) * std::sin (a))), 1.f);
    }
    g.setFont (font (12.f, true).withExtraKerningFactor (0.1f));
    g.drawText ("GAIN REDUCTION dB", Rectangle<float> (P (60, 80), P (240, 100)), Justification::centred, false);
    const float a = meterAngle (value);
    g.setColour (ui::needle);
    g.drawLine (Line<float> (P (cx, cy), P (cx + (R - 10) * std::cos (a), cy + (R - 10) * std::sin (a))), 2.2f);
    g.setColour (Colour (0xff1a1612)); const auto pv = P (cx, cy); g.fillEllipse (pv.x - 11 * sx, pv.y - 11 * sy, 22 * sx, 22 * sy);
}

// ---------------------------------------------------------------------------------- editor
RbaEditor::RbaEditor (RbaProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      unitSeg (P (ID::unit), { "El Rey", "Vintage RCA" }),
      powerSeg (P (ID::power), { "On", "Off" }, { 1, 0 }),
      bumpSeg (P (ID::bump), { "Off", "Pre", "Post" }),
      detSeg (P (ID::link), { "Linked", "Dual mono" }),
      modeSeg (P (ID::msmode), { "L / R", "M / S" }),
      rcaRelSeg (P (ID::rcarel), { "Single", "Dual" }),
      clipSeg (P (ID::clip), { "On", "Off" }, { 1, 0 }),
      preSeg (P (ID::pre), { "On", "Off" }, { 1, 0 }),
      linesSeg (P (ID::prelines), { "Mono", "Stereo" }),
      threeDSeg (P (ID::threed), { "Off", "On" }, { 0, 1 }),
      osSeg (P (ID::oslive), { "Off", "2x", "4x", "8x" }),
      renderOsSeg (P (ID::osrender), { "Off", "2x", "4x", "8x" }),
      renderModeSeg (P (ID::rendermode), { "Auto", "Always", "Never" }),
      drive (p.apvts, ID::input, "Drive", u8 ("\xe2\x88\x92" "18 \xc2\xb7 +18")),
      threshold (p.apvts, ID::threshold, "Threshold", u8 ("0 \xc2\xb7 40")),
      attack (p.apvts, ID::attack, "Attack", u8 ("1 \xc2\xb7 2 \xc2\xb7 3")),
      release (p.apvts, ID::release, "Release", u8 ("1 \xc2\xb7 2 \xc2\xb7 3 \xc2\xb7 4 \xc2\xb7 5")),
      filter (p.apvts, ID::filter, "SC Filter", u8 ("OFF \xc2\xb7 250 Hz")),
      rcaFilter (p.apvts, ID::rcafilter, "SC Filter", u8 ("OFF \xc2\xb7 100\xe2\x80\x93" "500 Hz")),
      balance (p.apvts, ID::balance, "P-P Balance", u8 ("\xe2\x88\x92" "100 \xc2\xb7 +100")),
      crush (p.apvts, ID::crush, "Crush", u8 ("0 \xc2\xb7 40")),
      clipMakeup (p.apvts, ID::clipmakeup, "Make Up", u8 ("0 \xc2\xb7 10 dB")),
      preDrive (p.apvts, ID::predrive, "Drive", u8 ("\xe2\x88\x92" "30 \xc2\xb7 +30")),
      makeup (p.apvts, ID::makeup, "Make Up", "", 44),
      mix (p.apvts, ID::mix, "Dry/Wet", "", 44)
{
    setLookAndFeel (&look);
    for (auto* c : std::initializer_list<Component*> { &unitSeg, &powerSeg, &bumpSeg, &detSeg, &modeSeg, &rcaRelSeg, &clipSeg,
             &preSeg, &linesSeg, &threeDSeg, &osSeg, &renderOsSeg, &renderModeSeg, &drive, &threshold, &attack, &release,
             &filter, &rcaFilter, &balance, &crush, &clipMakeup, &preDrive, &makeup, &mix, &gr })
        addAndMakeVisible (c);

    attack.isEnabledFn = [this] { return ! isRca(); };
    attack.valueText = [this] { return isRca() ? String ("fixed") : P (ID::attack).getCurrentValueAsText(); };
    attack.scaleFn = [this] { return isRca() ? String ("RCA network") : u8 ("1 \xc2\xb7 2 \xc2\xb7 3"); };
    auto clipOn = [this] { return P (ID::clip).getValue() > 0.5f; };
    auto preOn  = [this] { return P (ID::pre).getValue() > 0.5f; };
    crush.isEnabledFn = clipOn; clipMakeup.isEnabledFn = clipOn;
    preDrive.isEnabledFn = preOn; linesSeg.isEnabledFn = preOn;
    threeDSeg.isEnabledFn = [this] {
        return P (ID::pre).getValue() > 0.5f && P (ID::prelines).getValue() > 0.5f && roundToInt (P (ID::bump).convertFrom0to1 (P (ID::bump).getValue())) != 0;
    };
    const int mf = proc.maxFactorForRate();
    const int nOs = mf >= 8 ? 4 : (mf >= 4 ? 3 : 2);
    osSeg.setVisibleCount (nOs); renderOsSeg.setVisibleCount (nOs);

    setSize (1120, 620);
    updateUnitVisibility();
    startTimerHz (30);
}

RbaEditor::~RbaEditor() { setLookAndFeel (nullptr); }

void RbaEditor::updateUnitVisibility()
{
    const bool rca = isRca();
    release.setVisible (! rca); filter.setVisible (! rca);
    rcaRelSeg.setVisible (rca); rcaFilter.setVisible (rca);
}

void RbaEditor::timerCallback()
{
    updateUnitVisibility();

    const float target = proc.grMeter.load();
    grSmooth = target > grSmooth ? target : grSmooth + (target - grSmooth) * 0.12f;    // meter ballistics
    gr.setValue (grSmooth);
    for (int c = 0; c < 2; ++c)
    {
        const float v = proc.outPeak[c].load();
        pkSmooth[c] = v > pkSmooth[c] ? v : pkSmooth[c] * 0.92f;
    }
    repaint (footer); repaint (secInput); repaint (secComp.withHeight (40)); repaint (secPre);
}

void RbaEditor::paint (Graphics& g)
{
    g.fillAll (ui::bg);
    // header
    g.setColour (ui::cream); g.setFont (font (40.f, true).withExtraKerningFactor (0.04f));
    g.drawText ("RBA-6A", 24, 18, 170, 56, Justification::centredLeft, false);
    textSpaced (g, "VARI-MU LIMITING AMPLIFIER", { 190, 26, 360, 18 }, Justification::centredLeft, 13.f, true, ui::sub);
    const bool rca = isRca();
    textSpaced (g, rca ? "Vintage RCA  " + String::fromUTF8 ("\xc2\xb7") + "  hard-knee limiting  " + String::fromUTF8 ("\xc2\xb7") + "  single / dual release"
                       : "El Rey  " + String::fromUTF8 ("\xc2\xb7") + "  soft knee  " + String::fromUTF8 ("\xc2\xb7") + "  smooth RMS compression",
                { 190, 46, 420, 18 }, Justification::centredLeft, 13.f, false, ui::accent);
    textSpaced (g, "UNIT", { 590, 30, 42, 30 }, Justification::centredRight, 12.f, true, ui::sub);
    g.setColour (ui::border); g.fillRect (932, 29, 1, 32);
    textSpaced (g, "POWER", { 940, 30, 52, 30 }, Justification::centredLeft, 12.f, true, ui::sub);

    auto section = [&] (Rectangle<int> r, const String& t, const String& right)
    {
        g.setColour (ui::panel); g.fillRoundedRectangle (r.toFloat(), 10.f);
        g.setColour (ui::border); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.f, 1.f);
        textSpaced (g, t, { r.getX() + 14, r.getY() + 12, r.getWidth() - 28, 20 }, Justification::centredLeft, 15.f, true, ui::cream);
        if (right.isNotEmpty()) textSpaced (g, right, { r.getX() + 14, r.getY() + 12, r.getWidth() - 28, 20 }, Justification::centredRight, 12.f, false, ui::sub);
    };
    section (secInput, "1 " + String::fromUTF8 ("\xc2\xb7") + " INPUT", "INTO COMP");
    section (secComp, "2 " + String::fromUTF8 ("\xc2\xb7") + " COMPRESSOR", "");
    textSpaced (g, rca ? "VINTAGE RCA" : "EL REY", { secComp.getRight() - 160, secComp.getY() + 12, 146, 20 }, Justification::centredRight, 12.f, false, ui::accent);
    section (secClip, "3 " + String::fromUTF8 ("\xc2\xb7") + " CLIPPER", "");
    section (secPre, "4 " + String::fromUTF8 ("\xc2\xb7") + " PREAMP", "ALWAYS LAST");

    // small labels
    const int bump = roundToInt (P (ID::bump).convertFrom0to1 (P (ID::bump).getValue()));
    textSpaced (g, "BUMP", { secInput.getX() + 14, bumpSeg.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    g.setColour (ui::dim); g.setFont (font (11.f));
    g.drawFittedText (bump == 0 ? "Bump off: flat" : (bump == 1 ? "Pre: the compressor hears the bump" : "Post: bump tone after the compressor"),
                      secInput.getX() + 14, bumpSeg.getBottom() + 6, secInput.getWidth() - 28, 30, Justification::topLeft, 2, 1.0f);
    textSpaced (g, "DETECTOR", { detSeg.getX(), detSeg.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "MODE", { modeSeg.getX(), modeSeg.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    if (rca)
    {
        textSpaced (g, "RELEASE", { rcaRelArea.getX(), rcaRelArea.getY(), rcaRelArea.getWidth(), 14 }, Justification::centred, 12.f, true, ui::label);
        textSpaced (g, "SINGLE " + String::fromUTF8 ("\xc2\xb7") + " DUAL", { rcaRelArea.getX(), rcaRelSeg.getBottom() + 20, rcaRelArea.getWidth(), 14 },
                    Justification::centred, 11.f, false, ui::dim);
    }
    textSpaced (g, "LINES", { linesSeg.getX(), linesSeg.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "3D BUMP", { threeDSeg.getX(), threeDSeg.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    const bool d3ok = P (ID::pre).getValue() > 0.5f && P (ID::prelines).getValue() > 0.5f && bump != 0;
    textSpaced (g, d3ok ? "Detuned bump on one line" : "Needs Bump + Preamp on Stereo",
                { threeDSeg.getX(), threeDSeg.getBottom() + 6, threeDSeg.getWidth(), 14 }, Justification::centredLeft, 11.f, false, ui::dim);

    // footer
    g.setColour (ui::panel); g.fillRoundedRectangle (footer.toFloat(), 10.f);
    g.setColour (ui::border); g.drawRoundedRectangle (footer.toFloat().reduced (0.5f), 10.f, 1.f);
    textSpaced (g, "OUTPUT", { footer.getX() + 18, footer.getY(), 80, footer.getHeight() }, Justification::centredLeft, 15.f, true, ui::cream);
    for (int c = 0; c < 2; ++c)
    {
        auto bar = Rectangle<int> (meterArea.getX() + 20, meterArea.getY() + 8 + c * 18, meterArea.getWidth() - 20, 10);
        textSpaced (g, c == 0 ? "L" : "R", { meterArea.getX(), bar.getY() - 3, 14, 16 }, Justification::centredLeft, 12.f, true, ui::label);
        g.setColour (ui::border); g.fillRoundedRectangle (bar.toFloat(), 5.f);
        const float db = Decibels::gainToDecibels (pkSmooth[c], -60.f);
        const float frac = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        g.setColour (db > -0.1f ? ui::needle : ui::accent);
        g.fillRoundedRectangle (bar.toFloat().withWidth (bar.getWidth() * frac), 5.f);
    }
    {
        const float l = Decibels::gainToDecibels (pkSmooth[0], -99.f), r = Decibels::gainToDecibels (pkSmooth[1], -99.f);
        g.setColour (ui::dim); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f, Font::plain)));
        g.drawText (String (l, 1) + " / " + String (r, 1) + " dBFS peak", meterArea.getX(), meterArea.getY() + 46, meterArea.getWidth(), 14, Justification::centredLeft, false);
    }
    g.setColour (ui::border); g.fillRect (osSeg.getX() - 16, footer.getY() + 18, 1, footer.getHeight() - 36);
    textSpaced (g, "OVERSAMPLE", { osSeg.getX(), osSeg.getY() - 18, 140, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "RENDER OS", { renderOsSeg.getX(), renderOsSeg.getY() - 18, 140, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "USE RENDER OS", { renderModeSeg.getX(), renderModeSeg.getY() - 18, 160, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    const String stat = String (proc.sampleRateNow() / 1000.0, 1) + " kHz  " + String::fromUTF8 ("\xc2\xb7") + "  running "
                      + (proc.activeFactor.load() > 1 ? String (proc.activeFactor.load()) + "x" : String ("1x"))
                      + (proc.rendering.load() ? " (render)" : "") + "  " + String::fromUTF8 ("\xc2\xb7") + "  latency "
                      + String (proc.latencyNow.load()) + " samples (fixed)";
    g.setColour (ui::dim); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f, Font::plain)));
    g.drawText (stat, statusArea, Justification::centredLeft, false);
}

void RbaEditor::resized()
{
    unitSeg.setBounds (640, 27, 280, 36);
    powerSeg.setBounds (996, 27, 100, 36);

    const int top = 88, h = 372;
    secInput = { 24, top, 190, h };
    secComp  = { 226, top, 474, h };
    secClip  = { 712, top, 168, h };
    secPre   = { 892, top, 204, h };
    footer   = { 24, top + h + 14, 1072, 104 };

    // input
    drive.setBounds (secInput.getX() + 53, top + 44, 84, 112);
    bumpSeg.setBounds (secInput.getX() + 14, top + 196, secInput.getWidth() - 28, 36);

    // compressor
    gr.setBounds (secComp.getX() + 14, top + 44, 300, 150);
    detSeg.setBounds (secComp.getX() + 326, top + 74, 136, 36);
    modeSeg.setBounds (secComp.getX() + 326, top + 144, 136, 36);
    const int ky = top + 222, kx = secComp.getX() + 14, kw = (secComp.getWidth() - 28) / 5;
    threshold.setBounds (kx, ky, kw, 130);
    attack.setBounds (kx + kw, ky, kw, 130);
    release.setBounds (kx + 2 * kw, ky, kw, 130);
    rcaRelArea = { kx + 2 * kw, ky, kw, 130 };
    rcaRelSeg.setBounds (kx + 2 * kw - 8, ky + 30, kw + 16, 36);
    filter.setBounds (kx + 3 * kw, ky, kw, 130);
    rcaFilter.setBounds (kx + 3 * kw, ky, kw, 130);
    balance.setBounds (kx + 4 * kw, ky, kw, 130);

    // clipper
    clipSeg.setBounds (secClip.getX() + 14, top + 44, secClip.getWidth() - 28, 36);
    crush.setBounds (secClip.getX() + 42, top + 96, 84, 130);
    clipMakeup.setBounds (secClip.getX() + 42, top + 228, 84, 130);

    // preamp
    preSeg.setBounds (secPre.getX() + 14, top + 44, secPre.getWidth() - 28, 36);
    preDrive.setBounds (secPre.getX() + 60, top + 92, 84, 112);
    linesSeg.setBounds (secPre.getX() + 14, top + 230, secPre.getWidth() - 28, 36);
    threeDSeg.setBounds (secPre.getX() + 14, top + 300, secPre.getWidth() - 28, 36);

    // footer
    makeup.setBounds (footer.getX() + 96, footer.getY() + 10, 78, 90);
    mix.setBounds (footer.getX() + 176, footer.getY() + 10, 78, 90);
    meterArea = { footer.getX() + 268, footer.getY() + 22, 210, 64 };
    osSeg.setBounds (footer.getX() + 510, footer.getY() + 30, 170, 32);
    renderOsSeg.setBounds (footer.getX() + 694, footer.getY() + 30, 170, 32);
    renderModeSeg.setBounds (footer.getX() + 878, footer.getY() + 30, 180, 32);
    statusArea = { footer.getX() + 510, footer.getY() + 70, 548, 22 };
}
