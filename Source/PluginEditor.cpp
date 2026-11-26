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
        g.setFont (font (fontSize, true).withExtraKerningFactor (0.04f));
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

#ifndef RBA_VERSION
 #define RBA_VERSION "dev"
#endif

// ------------------------------------------------------------------------------ lit toggle
LitToggle::LitToggle (RangedAudioParameter& p, String on, String off, int onI, int offI)
    : param (p), textOn (on), textOff (off), onIndex (onI), offIndex (offI)
{
    startTimerHz (15);
    setMouseCursor (MouseCursor::PointingHandCursor);
}
bool LitToggle::isOn() const { return roundToInt (param.convertFrom0to1 (param.getValue())) == onIndex; }
void LitToggle::paint (Graphics& g)
{
    const bool on = isOn();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (on) { g.setColour (ui::knob); g.fillRoundedRectangle (r, 6.f); }
    g.setColour (on ? ui::accent : Colour (0xff4a443d)); g.drawRoundedRectangle (r, 6.f, 1.f);
    const String t = on ? textOn : textOff;
    auto f = font (14.f, true).withExtraKerningFactor (0.14f);
    const float tw = GlyphArrangement::getStringWidth (f, t);
    const float total = 10.f + 10.f + tw;
    const float x0 = r.getCentreX() - total / 2.f, cy = r.getCentreY();
    auto led = Rectangle<float> (x0, cy - 5.f, 10.f, 10.f);
    const float lv = on ? (ledLevel ? jlimit (0.f, 1.f, ledLevel()) : 1.f) : 0.f;
    if (on && lv > 0.f)
    {
        g.setColour (ui::accent.withAlpha (0.25f * lv)); g.fillEllipse (led.expanded (4.f));
        g.setColour (ui::accent.withAlpha (0.45f * lv)); g.fillEllipse (led.expanded (2.f));
        g.setColour (ui::border.interpolatedWith (ui::accent, 0.3f + 0.7f * lv)); g.fillEllipse (led);
    }
    else if (on)                                          // on but idle: a dimmed lamp
    {
        g.setColour (ui::border.interpolatedWith (ui::accent, 0.3f)); g.fillEllipse (led);
        g.setColour (ui::knobEdge); g.drawEllipse (led.reduced (0.5f), 1.f);
    }
    else
    {
        g.setColour (ui::border); g.fillEllipse (led);
        g.setColour (ui::knobEdge); g.drawEllipse (led.reduced (0.5f), 1.f);
    }
    g.setColour (on ? ui::cream : ui::sub); g.setFont (f);
    g.drawText (t, Rectangle<float> (x0 + 20.f, r.getY(), tw + 4.f, r.getHeight()), Justification::centredLeft, false);
}
void LitToggle::mouseUp (const MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.convertTo0to1 ((float) (isOn() ? offIndex : onIndex)));
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

// hardware-style geometry (matches the design canvas): dial centre at (w/2, 46), numbers on a 39 px radius
static void drawHardwareKnob (Graphics& g, int w, int size, const StringArray& ticks, bool ends, bool en,
                              const String& value, const String& title)
{
    const float cx = (float) w / 2.f, cy = 18.f + (float) size / 2.f, r = (float) size / 2.f + 11.f;
    const int n = ticks.size();
    g.setFont (font (11.f));
    g.setColour (Colour (0xffb5ab9b).withMultipliedAlpha (en ? 1.f : 0.4f));
    for (int i = 0; i < n; ++i)
    {
        const float a = degreesToRadians (n > 1 ? -135.f + 270.f * (float) i / (float) (n - 1) : 0.f);
        const float x = cx + r * std::sin (a), y = cy - r * std::cos (a);
        g.drawText (ticks[i], Rectangle<float> (x - 20.f, y - 7.f, 40.f, 14.f), Justification::centred, false);
    }
    if (ends)
    {
        const float dx = r * MathConstants<float>::sqrt2 / 2.f;
        textSpaced (g, "SLOW", Rectangle<float> (cx - dx - 20.f, 82.f, 40.f, 13.f).toNearestInt(), Justification::centred, 10.f, true, ui::sub);
        textSpaced (g, "FAST", Rectangle<float> (cx + dx - 20.f, 82.f, 40.f, 13.f).toNearestInt(), Justification::centred, 10.f, true, ui::sub);
    }
    g.setColour (ui::cream); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 12.f, Font::plain)));
    g.drawText (value, 0, 100, w, 16, Justification::centred, false);
    textSpaced (g, title.toUpperCase(), { 0, 118, w, 15 }, Justification::centred, 12.f, true, ui::label);
}
void Knob::timerCallback()
{
    const bool en = ! isEnabledFn || isEnabledFn();
    if (slider.isEnabled() != en) slider.setEnabled (en);
    repaint();
}
void Knob::paint (Graphics& g)
{
    if (hardware)
    {
        drawHardwareKnob (g, getWidth(), size, ticksFn ? ticksFn() : StringArray(), showEnds && showEnds(),
                          ! isEnabledFn || isEnabledFn(), valueText ? valueText() : param->getCurrentValueAsText(), title);
        return;
    }
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
    if (showEnds && showEnds())
    {
        // like the hardware: printed at the two ends of the knob's travel
        const int cx = getWidth() / 2, y = 18 + size - 6;
        g.setColour (ui::sub); g.setFont (font (9.5f).boldened());
        g.drawText ("SLOW", cx - size / 2 - 30, y, 30 + size / 2 - 16, 12, Justification::centredRight, false);
        g.drawText ("FAST", cx + 16, y, 30 + size / 2 - 16, 12, Justification::centredLeft, false);
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
      unitSeg (P (ID::unit), { "Modern", "Vintage" }),
      bumpSeg (P (ID::bump), { "Off", "Pre", "Post" }),
      rcaRelSeg (P (ID::rcarel), { "Single", "Dual" }),
      linesSeg (P (ID::prelines), { "Mono", "Stereo" }),
      threeDSeg (P (ID::threed), { "Off", "On" }, { 0, 1 }),
      osSeg (P (ID::oslive), { "Off", "2x", "4x", "8x" }),
      renderOsSeg (P (ID::osrender), { "Off", "2x", "4x", "8x" }),
      renderModeSeg (P (ID::rendermode), { "Auto", "Always", "Never" }),
      rangeSeg (P (ID::rcahot), { "Normal", "+9" }),
      filterSeg (P (ID::bandlimit), { "Off", "Clean", "Air" }),
      phaseSeg (P (ID::phase), { "Analog", "Linear" }),
      meterSeg (P (ID::meter), { "Pk", "Avg" }),
      powerTog (P (ID::power), "ON", "OFF"),
      detTog (P (ID::link), "LINK ON", "LINK OFF", 0, 1),
      clipTog (P (ID::clip), "CLIPPER", "CLIPPER"),
      preTog (P (ID::pre), "PREAMP ON", "PREAMP OFF"),
      drive (p.apvts, ID::input, "Drive", u8 ("\xe2\x88\x92" "18 \xc2\xb7 +18")),
      threshold (p.apvts, ID::threshold, "THR", u8 ("0 \xc2\xb7 40")),
      attack (p.apvts, ID::attack, "ATT", u8 ("1 \xc2\xb7 2 \xc2\xb7 3")),
      release (p.apvts, ID::release, "REL", u8 ("1 \xc2\xb7 2 \xc2\xb7 3 \xc2\xb7 4 \xc2\xb7 5")),
      filter (p.apvts, ID::filter, "SC HPF", u8 ("OFF \xc2\xb7 250 Hz")),
      rcaFilter (p.apvts, ID::rcafilter, "SC HPF", u8 ("OFF \xc2\xb7 100\xe2\x80\x93" "500 Hz")),
      balance (p.apvts, ID::balance, "P-P", u8 ("\xe2\x88\x92" "100 \xc2\xb7 +100")),
      crush (p.apvts, ID::crush, "Crush", u8 ("0 \xc2\xb7 40")),
      clipMakeup (p.apvts, ID::clipmakeup, "Make Up", u8 ("0 \xc2\xb7 10 dB")),
      preDrive (p.apvts, ID::predrive, "Drive", u8 ("\xe2\x88\x92" "30 \xc2\xb7 +30")),
      makeup (p.apvts, ID::makeup, "Make Up", "", 44),
      mix (p.apvts, ID::mix, "Dry/Wet", "", 44)
{
    setLookAndFeel (&look);
    for (auto* c : std::initializer_list<Component*> { &unitSeg, &powerTog, &bumpSeg, &detTog, &rcaRelSeg, &clipTog,
             &preTog, &linesSeg, &threeDSeg, &osSeg, &renderOsSeg, &renderModeSeg, &rangeSeg, &filterSeg, &phaseSeg, &meterSeg, &drive, &threshold, &attack, &release,
             &filter, &rcaFilter, &balance, &crush, &clipMakeup, &preDrive, &makeup, &mix, &gr })
        addAndMakeVisible (c);

    for (auto* k : { &threshold, &attack, &release, &filter, &rcaFilter, &balance }) k->hardware = true;
    threshold.ticksFn = [] { return StringArray { "0", "10", "20", "30", "40" }; };
    attack.ticksFn    = [this] { return isRca() ? StringArray() : StringArray { "1", "2", "3" }; };
    release.ticksFn   = [] { return StringArray { "1", "2", "3", "4", "5" }; };
    filter.ticksFn    = [] { return StringArray { "OFF", "125", "250" }; };
    rcaFilter.ticksFn = [] { return StringArray { "OFF", "300", "500" }; };
    balance.ticksFn   = [] { return StringArray { u8 ("\xe2\x88\x92" "100"), "0", "+100" }; };
    attack.isEnabledFn = [this] { return ! isRca(); };
    meterSeg.fontSize = 10.f;
    // Clipper lamp: dimmed while the Clipper is on but idle; 10 steps of 0.5 dB, fully lit from about 5 dB of peak reduction
    clipTog.ledLevel = [this] { return (float) jlimit (0, 10, roundToInt (clipShow * 2.f)) / 10.f; };
    attack.showEnds  = [this] { return ! isRca(); };
    release.showEnds = [] { return true; };
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
        const float v = proc.outPeak[c].load(), vi = proc.inPeak[c].load();
        pkSmooth[c] = v > pkSmooth[c] ? v : pkSmooth[c] * 0.92f;
        inSmooth[c] = vi > inSmooth[c] ? vi : inSmooth[c] * 0.92f;
    }
    const float cr = proc.clipRed.exchange (0.f);
    clipShow = jmax (cr, clipShow - 0.25f);              // hold the peak, then fall about 7.5 dB per second
    clipTog.repaint();
    repaint (footer); repaint (secInput); repaint (secComp.withHeight (40)); repaint (secPre);
}

void RbaEditor::paint (Graphics& g)
{
    g.fillAll (ui::bg);
    // header
    g.setColour (ui::cream); g.setFont (font (40.f, true).withExtraKerningFactor (0.04f));
    g.drawText ("RBA-6A", 24, 14, 170, 44, Justification::centredLeft, false);
    g.setColour (ui::dim); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f, Font::plain)));
    g.drawText ("v" RBA_VERSION, 26, 58, 120, 14, Justification::centredLeft, false);
    textSpaced (g, "VARI-MU LIMITING AMPLIFIER", { 190, 26, 360, 18 }, Justification::centredLeft, 13.f, true, ui::sub);
    const bool rca = isRca();
    textSpaced (g, rca ? "Vintage  " + String::fromUTF8 ("\xc2\xb7") + "  hard-knee limiting  " + String::fromUTF8 ("\xc2\xb7") + "  single / dual release"
                       : "Modern  " + String::fromUTF8 ("\xc2\xb7") + "  soft knee  " + String::fromUTF8 ("\xc2\xb7") + "  smooth RMS compression",
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
    textSpaced (g, rca ? "VINTAGE" : "MODERN", { secComp.getRight() - 160, secComp.getY() + 12, 146, 20 }, Justification::centredRight, 12.f, false, ui::accent);
    section (secClip, "3 " + String::fromUTF8 ("\xc2\xb7") + " CLIPPER", "");
    section (secPre, "4 " + String::fromUTF8 ("\xc2\xb7") + " PREAMP", "ALWAYS LAST");

    // small labels
    const int bump = roundToInt (P (ID::bump).convertFrom0to1 (P (ID::bump).getValue()));
    textSpaced (g, "BUMP", { secInput.getX() + 14, bumpSeg.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    g.setColour (ui::dim); g.setFont (font (11.f));
    g.drawFittedText (bump == 0 ? "Bump off: flat" : (bump == 1 ? "Pre: the compressor hears the bump" : "Post: bump tone after the compressor"),
                      secInput.getX() + 14, bumpSeg.getBottom() + 6, secInput.getWidth() - 28, 30, Justification::topLeft, 2, 1.0f);
    textSpaced (g, "DETECTOR", { detTog.getX(), detTog.getY() - 20, 120, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "RANGE", { rangeSeg.getX(), rangeSeg.getY() - 20, 136, 16 }, Justification::centredLeft, 12.f, true, ui::label);
    if (rca)
    {
        textSpaced (g, "REL", { rcaRelArea.getX(), rcaRelArea.getY() + 118, rcaRelArea.getWidth(), 15 }, Justification::centred, 12.f, true, ui::label);
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
    // meters, top to bottom: L OUT, L IN, R IN, R OUT (the inputs sit between the outputs)
    {
        const bool avg = roundToInt (P (ID::meter).convertFrom0to1 (P (ID::meter).getValue())) == 1;
        const float outV[2] = { avg ? proc.outRms[0].load() : pkSmooth[0], avg ? proc.outRms[1].load() : pkSmooth[1] };
        const float inV[2]  = { avg ? proc.inRms[0].load()  : inSmooth[0], avg ? proc.inRms[1].load()  : inSmooth[1] };
        struct Row { const char* label; float v; bool out; };
        const Row rows[4] = { { "L OUT", outV[0], true }, { "L IN", inV[0], false }, { "R IN", inV[1], false }, { "R OUT", outV[1], true } };
        for (int k = 0; k < 4; ++k)
        {
            const int y = meterArea.getY() + k * 11 + (k >= 2 ? 4 : 0);
            auto bar = Rectangle<int> (meterArea.getX() + 34, y, meterArea.getWidth() - 34, 7);
            g.setColour (rows[k].out ? ui::label : ui::dim); g.setFont (font (9.f, true));
            g.drawText (rows[k].label, meterArea.getX(), y - 2, 32, 11, Justification::centredLeft, false);
            g.setColour (ui::border); g.fillRoundedRectangle (bar.toFloat(), 3.5f);
            const float db = Decibels::gainToDecibels (rows[k].v, -60.f);
            const float frac = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
            g.setColour (db > -0.1f ? ui::needle : (rows[k].out ? ui::accent : ui::accent.withAlpha (0.55f)));
            g.fillRoundedRectangle (bar.toFloat().withWidth (bar.getWidth() * frac), 3.5f);
        }
        const float l = Decibels::gainToDecibels (outV[0], -99.f), r = Decibels::gainToDecibels (outV[1], -99.f);
        g.setColour (ui::dim); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 10.f, Font::plain)));
        g.drawFittedText (String (l, 1) + "/" + String (r, 1), meterArea.getX(), meterSeg.getY(), meterSeg.getX() - meterArea.getX() - 3, meterSeg.getHeight(),
                          Justification::centredLeft, 1, 0.8f);
    }
    g.setColour (ui::border); g.fillRect (filterSeg.getX() - 12, footer.getY() + 18, 1, footer.getHeight() - 36);
    textSpaced (g, "FILTER", { filterSeg.getX(), filterSeg.getY() - 18, 140, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "PHASE", { phaseSeg.getX(), phaseSeg.getY() - 18, 120, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "OVERSAMPLE", { osSeg.getX(), osSeg.getY() - 18, 140, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "RENDER OS", { renderOsSeg.getX(), renderOsSeg.getY() - 18, 140, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    textSpaced (g, "USE RENDER OS", { renderModeSeg.getX(), renderModeSeg.getY() - 18, 160, 14 }, Justification::centredLeft, 12.f, true, ui::label);
    const String stat = String (proc.sampleRateNow() / 1000.0, 1) + " kHz  " + String::fromUTF8 ("\xc2\xb7") + "  running "
                      + (proc.activeFactor.load() > 1 ? String (proc.activeFactor.load()) + "x" : String ("1x"))
                      + (proc.rendering.load() ? " (render)" : "") + "  " + String::fromUTF8 ("\xc2\xb7") + "  latency "
                      + String (proc.latencyNow.load()) + " samples (fixed)"
                      + ((P (ID::bandlimit).getValue() > 0.25f && proc.sampleRateNow() * proc.activeFactor.load() < 88000.0)
                             ? "  " + String::fromUTF8 ("\xc2\xb7") + "  filter needs 88.2 kHz+ inside" : String());
    g.setColour (ui::dim); g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f, Font::plain)));
    g.drawText (stat, statusArea, Justification::centredLeft, false);
}

void RbaEditor::resized()
{
    unitSeg.setBounds (640, 27, 280, 36);
    powerTog.setBounds (996, 27, 100, 36);

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
    detTog.setBounds (secComp.getX() + 326, top + 76, 136, 32);
    rangeSeg.setBounds (secComp.getX() + 326, top + 150, 136, 32);
    const int ky = top + 222, kx = secComp.getX() + 14, kw = (secComp.getWidth() - 28) / 5;
    threshold.setBounds (kx, ky, kw, 136);
    attack.setBounds (kx + kw, ky, kw, 136);
    release.setBounds (kx + 2 * kw, ky, kw, 136);
    rcaRelArea = { kx + 2 * kw, ky, kw, 136 };
    rcaRelSeg.setBounds (kx + 2 * kw - 8, ky + 28, kw + 16, 36);
    filter.setBounds (kx + 3 * kw, ky, kw, 136);
    rcaFilter.setBounds (kx + 3 * kw, ky, kw, 136);
    balance.setBounds (kx + 4 * kw, ky, kw, 136);

    // clipper
    clipTog.setBounds (secClip.getX() + 14, top + 44, secClip.getWidth() - 28, 36);
    crush.setBounds (secClip.getX() + 42, top + 96, 84, 130);
    clipMakeup.setBounds (secClip.getX() + 42, top + 228, 84, 130);

    // preamp
    preTog.setBounds (secPre.getX() + 14, top + 44, secPre.getWidth() - 28, 36);
    preDrive.setBounds (secPre.getX() + 60, top + 92, 84, 112);
    linesSeg.setBounds (secPre.getX() + 14, top + 230, secPre.getWidth() - 28, 36);
    threeDSeg.setBounds (secPre.getX() + 14, top + 300, secPre.getWidth() - 28, 36);

    // footer
    makeup.setBounds (footer.getX() + 90, footer.getY() + 10, 78, 90);
    mix.setBounds (footer.getX() + 164, footer.getY() + 10, 78, 90);
    meterArea = { footer.getX() + 252, footer.getY() + 18, 128, 68 };
    meterSeg.setBounds (meterArea.getRight() - 50, meterArea.getY() + 50, 50, 18);
    filterSeg.setBounds (footer.getX() + 394, footer.getY() + 30, 132, 32);
    phaseSeg.setBounds (footer.getX() + 536, footer.getY() + 30, 116, 32);
    osSeg.setBounds (footer.getX() + 664, footer.getY() + 30, 110, 32);
    renderOsSeg.setBounds (footer.getX() + 786, footer.getY() + 30, 110, 32);
    renderModeSeg.setBounds (footer.getX() + 908, footer.getY() + 30, 150, 32);
    statusArea = { footer.getX() + 394, footer.getY() + 70, 664, 22 };
}
