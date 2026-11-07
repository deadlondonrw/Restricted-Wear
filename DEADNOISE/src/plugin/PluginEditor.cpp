// DEADNOISE - plugin editor.
#include "PluginEditor.h"

using namespace dnui;

//==============================================================================
juce::Font dnui::uiFont (float height, bool bold, float tracking)
{
    juce::FontOptions o;
   #if JUCE_WINDOWS
    o = o.withName (bold ? "Segoe UI Semibold" : "Segoe UI");
   #endif
    o = o.withHeight (height).withStyle (bold ? "Bold" : "Regular").withKerningFactor (tracking);
    return juce::Font (o);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::panelHi);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::track);
    setColour (juce::TextEditor::outlineColourId, colours::border);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::textDim);
    setColour (juce::CaretComponent::caretColourId, colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const bool large = s.getProperties().getWithDefault ("large", false);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (large ? 6.0f : 4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = large ? 4.0f : 3.0f;
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar parameters (gains) draw from the centre
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - fromAngle) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                             juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
        g.setColour (s.isEnabled() ? colours::text : colours::textFaint);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - lineW * 2.2f;
    g.setColour (colours::panelHi);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (colours::border);
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    const juce::Point<float> p1 = centre.getPointOnCircumference (bodyR * 0.35f, angle);
    const juce::Point<float> p2 = centre.getPointOnCircumference (bodyR * 0.85f, angle);
    g.setColour (s.isEnabled() ? colours::text : colours::textFaint);
    g.drawLine ({ p1, p2 }, large ? 2.5f : 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    juce::Colour fill = on ? colours::text : colours::panelHi;
    if (! on && over) fill = fill.brighter (0.06f);
    if (down) fill = fill.brighter (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::text : (over ? colours::textFaint : colours::border));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colours::background : (b.isEnabled() ? colours::text : colours::textFaint));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float sw = 34.0f, sh = 18.0f;
    juce::Rectangle<float> sw_r (r.getX(), r.getCentreY() - sh * 0.5f, sw, sh);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::text : colours::track);
    g.fillRoundedRectangle (sw_r, sh * 0.5f);
    if (over && ! on) { g.setColour (colours::textFaint); g.drawRoundedRectangle (sw_r, sh * 0.5f, 1.0f); }
    const float d = sh - 4.0f;
    const float cx = on ? sw_r.getRight() - d - 2.0f : sw_r.getX() + 2.0f;
    g.setColour (on ? colours::background : colours::textDim);
    g.fillEllipse (cx, sw_r.getY() + 2.0f, d, d);
    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (uiFont (12.0f, true, 0.08f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (sw + 10.0f), juce::Justification::centredLeft);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
            const juce::String& captionText, bool isLarge)
    : caption (captionText), large (isLarge)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, large ? 22 : 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.getProperties().set ("large", large);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (large ? 260 : 200);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (large ? 22 : 18);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (slider.isEnabled() ? colours::textDim : colours::textFaint);
    g.setFont (uiFont (large ? 12.0f : 10.5f, true, 0.12f));
    g.drawText (caption, getLocalBounds().removeFromTop (large ? 20 : 16), juce::Justification::centred);
}

//==============================================================================
Segmented::Segmented (juce::RangedAudioParameter& param, juce::StringArray l)
    : labels (std::move (l)),
      attachment (param, [this] (float v) { selected = juce::roundToInt (v); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const float w = r.getWidth() / (float) labels.size();
    g.setFont (uiFont (11.0f, true, 0.08f));
    for (int i = 0; i < labels.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (i == selected)
        {
            g.setColour (colours::text);
            g.fillRoundedRectangle (seg.reduced (2.0f), 3.0f);
            g.setColour (colours::background);
        }
        else
            g.setColour (colours::textDim);
        g.drawFittedText (labels[i].toUpperCase(), seg.toNearestInt().reduced (4, 0), juce::Justification::centred, 1);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    const int idx = juce::jlimit (0, labels.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) labels.size())));
    attachment.setValueAsCompleteGesture ((float) idx);
}

//==============================================================================
void LevelMeter::pushPeak (float linearPeak, float dt)
{
    const float db = juce::Decibels::gainToDecibels (linearPeak, -100.0f);
    // fast attack, ~20 dB/s fall
    levelDb = db > levelDb ? db : juce::jmax (db, levelDb - 20.0f * dt);
    if (db >= holdDb) { holdDb = db; holdTime = 1.2f; }
    else if ((holdTime -= dt) <= 0.0f) holdDb = juce::jmax (levelDb, holdDb - 30.0f * dt);
    if (linearPeak >= 1.0f) clipped = true;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (uiFont (10.5f, true, 0.12f));
    g.drawText (caption, r.removeFromBottom (16), juce::Justification::centred);
    auto top = r.removeFromTop (16);
    g.setFont (uiFont (10.5f));
    g.setColour (clipped ? colours::clip : colours::text);
    g.drawText (holdDb <= -99.0f ? juce::String ("-inf") : juce::String (holdDb, 1), top, juce::Justification::centred);

    auto bar = r.reduced (0, 4).withSizeKeepingCentre (14, r.getHeight() - 8).toFloat();
    const float minDb = -60.0f, maxDb = 6.0f;
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, bar.getBottom(), bar.getY()); };

    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (bar, 2.0f);
    const float yl = yFor (levelDb);
    if (levelDb > minDb)
    {
        juce::ColourGradient grad (colours::textFaint, 0, bar.getBottom(), colours::text, 0, bar.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bar.withTop (yl), 2.0f);
    }
    if (holdDb > minDb)
    {
        g.setColour (holdDb > 0.0f ? colours::clip : colours::text);
        g.fillRect (bar.getX(), yFor (holdDb) - 1.0f, bar.getWidth(), 2.0f);
    }

    g.setFont (uiFont (9.0f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float y = yFor (db);
        g.setColour (colours::textFaint);
        g.fillRect (bar.getRight() + 2.0f, y, 4.0f, 1.0f);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (bar.getRight() + 7.0f, y - 6.0f, 24.0f, 12.0f),
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
void SpectrumView::refresh()
{
    denoiser.getDisplaySpectra (in, out, nz);
    valid = true;
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float minDb = -130.0f, maxDb = -10.0f;
    constexpr int N = deadnoise::SpectralDenoiser::kDisplayPoints;
    const double nyq = denoiser.getSampleRate() * 0.5;

    auto xForHz = [&] (double hz) { return r.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, r.getBottom(), r.getY()); };

    g.setFont (uiFont (9.5f));
    for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xForHz (hz);
        g.setColour (colours::border);
        g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        g.setColour (colours::textFaint);
        const juce::String t = hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz);
        g.drawText (t, juce::Rectangle<float> (x + 3.0f, r.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
    }
    for (float db : { -120.0f, -100.0f, -80.0f, -60.0f, -40.0f, -20.0f })
    {
        g.setColour (colours::border.withAlpha (0.6f));
        g.fillRect (r.getX(), yForDb (db), r.getWidth(), 1.0f);
        g.setColour (colours::textFaint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getX() + 3.0f, yForDb (db) - 12.0f, 34.0f, 11.0f),
                    juce::Justification::centredLeft);
    }
    if (! valid) return;

    auto makePath = [&] (const float* v, bool closed)
    {
        juce::Path p;
        bool started = false;
        for (int i = 0; i < N; ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, (double) i / (N - 1));
            if (hz > nyq) break;
            const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1);
            const float y = yForDb (v[i]);
            if (! started) { p.startNewSubPath (x, closed ? r.getBottom() : y); if (closed) p.lineTo (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (closed && started) { p.lineTo (p.getCurrentPosition().x, r.getBottom()); p.closeSubPath(); }
        return p;
    };

    const auto inPath = makePath (in, true);
    g.setColour (juce::Colour (0xff2a2a2f));
    g.fillPath (inPath);
    g.setColour (colours::textFaint);
    g.strokePath (makePath (in, false), juce::PathStrokeType (1.0f));

    juce::Path nzPath = makePath (nz, false), dashed;
    const float dash[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, nzPath, dash, 2);
    g.setColour (colours::warn.withAlpha (0.85f));
    g.fillPath (dashed);

    g.setColour (colours::text);
    g.strokePath (makePath (out, false), juce::PathStrokeType (1.5f));

    // legend
    auto lg = r.removeFromTop (16.0f).removeFromRight (300.0f);
    g.setFont (uiFont (9.5f, true, 0.1f));
    auto item = [&] (juce::Colour c, const juce::String& t, float w)
    {
        auto a = lg.removeFromLeft (w);
        g.setColour (c);
        g.fillRect (a.getX(), a.getCentreY() - 1.0f, 12.0f, 2.0f);
        g.setColour (colours::textDim);
        g.drawText (t, a.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
    };
    item (colours::textFaint, "INPUT", 70.0f);
    item (colours::text, "OUTPUT", 80.0f);
    item (colours::warn, "NOISE ESTIMATE", 130.0f);
}

//==============================================================================
DeadnoiseAudioProcessorEditor::DeadnoiseAudioProcessorEditor (DeadnoiseAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      spectrum (p.getEngine().getDenoiser()),
      reduction (p.getState(), ParamID::reduction, "NOISE REDUCTION", true),
      voice (p.getState(), ParamID::voice, "VOICE PRESERVATION"),
      mix (p.getState(), ParamID::mix, "WET / DRY"),
      inGain (p.getState(), ParamID::inGain, "INPUT"),
      outGain (p.getState(), ParamID::outGain, "OUTPUT"),
      gateThresh (p.getState(), ParamID::gateThresh, "THRESHOLD"),
      gateRange (p.getState(), ParamID::gateRange, "RANGE"),
      mode (*p.getState().getParameter (ParamID::mode), { "Natural", "Maximum" }),
      monitor (*p.getState().getParameter (ParamID::monitor), { "Processed", "Original", "Removed" })
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<juce::Component*> { &spectrum, &reduction, &voice, &mix, &inGain, &outGain,
                                                            &gateThresh, &gateRange, &mode, &monitor, &learnButton,
                                                            &clearButton, &resetButton, &gateToggle, &inMeter, &outMeter })
        addAndMakeVisible (c);

    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.getState(), ParamID::gateOn, gateToggle);

    learnButton.onClick = [this]
    {
        const auto st = proc.getEngine().getDenoiser().getLearnState();
        if (st == deadnoise::LearnState::requested || st == deadnoise::LearnState::capturing)
            proc.cancelLearning();
        else
            proc.startLearning();
        updateLearnStatus();
    };
    clearButton.onClick = [this] { proc.clearProfile(); updateLearnStatus(); };
    resetButton.onClick = [this] { proc.resetToDefaults(); };

    learnButton.setTooltip ("Stay silent for 4 seconds while DEADNOISE measures the microphone's background noise.");
    resetButton.setTooltip ("Restore all controls to their default settings (keeps the learned noise profile).");
    clearButton.setTooltip ("Forget the learned noise profile and fall back to adaptive noise tracking.");

    setSize (932, 588);
    updateLearnStatus();
    startTimerHz (30);
}

DeadnoiseAudioProcessorEditor::~DeadnoiseAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DeadnoiseAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title) const
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (uiFont (10.5f, true, 0.16f));
        g.drawText (title, r.reduced (14, 0).removeFromTop (28), juce::Justification::centredLeft);
    }
}

void DeadnoiseAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // ---- header
    auto h = headerArea;
    g.setColour (colours::text);
    g.setFont (uiFont (26.0f, true, 0.32f));
    g.drawText ("DEADNOISE", h.removeFromLeft (260).withTrimmedLeft (18), juce::Justification::centredLeft);
    g.setColour (colours::textFaint);
    g.setFont (uiFont (10.0f, true, 0.2f));
    g.drawText ("MICROPHONE NOISE SUPPRESSION", h.removeFromLeft (240), juce::Justification::centredLeft);

    auto chip = [&] (juce::Rectangle<int> a, const juce::String& label, const juce::String& value, juce::Colour dot)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (a.toFloat(), 4.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 4.0f, 1.0f);
        auto t = a.reduced (10, 0);
        g.setColour (dot);
        g.fillEllipse ((float) t.getX(), (float) t.getCentreY() - 3.0f, 6.0f, 6.0f);
        t.removeFromLeft (12);
        g.setFont (uiFont (9.5f, true, 0.14f));
        g.setColour (colours::textFaint);
        const int lw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label)) + 8;
        g.drawText (label, t.removeFromLeft (lw), juce::Justification::centredLeft);
        g.setFont (uiFont (10.5f, true, 0.06f));
        g.setColour (colours::text);
        g.drawText (value, t, juce::Justification::centredLeft);
    };
    auto chips = headerArea.reduced (16, 14).removeFromRight (420);
    const double sr = proc.getCurrentSampleRate();
    const int lat = proc.getCurrentLatency();
    const juce::String latText = sr > 0 ? juce::String (1000.0 * lat / sr, 1) + " ms" : "--";
    chip (chips.removeFromRight (128), "LATENCY", latText, colours::textFaint);
    chips.removeFromRight (8);
    chip (chips.removeFromRight (140), "ENGINE", "SPECTRAL DSP", colours::text);
    chips.removeFromRight (8);
    chip (chips.removeFromRight (136), "VOICE", speechActivity > 0.5f ? "ACTIVE" : "--",
          speechActivity > 0.5f ? colours::text : colours::textFaint);

    // ---- panels
    drawPanel (g, spectrumArea, {});
    drawPanel (g, learnArea, "BACKGROUND PROFILE");
    drawPanel (g, reductionArea, {});
    drawPanel (g, voiceArea, {});
    drawPanel (g, gateArea, {});
    drawPanel (g, monitorArea, "MONITORING");

    // learn panel text
    {
        auto a = learnArea.reduced (16, 0);
        a.removeFromTop (28 + 44 + 10);
        auto bar = a.removeFromTop (4).toFloat();
        g.setColour (colours::track);
        g.fillRoundedRectangle (bar, 2.0f);
        if (learning || learnProgress > 0.0f)
        {
            g.setColour (colours::text);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, learnProgress)), 2.0f);
        }
        a.removeFromTop (12);
        auto statusRow = a.removeFromTop (18);
        g.setColour (profileColour);
        g.fillEllipse ((float) statusRow.getX(), (float) statusRow.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setFont (uiFont (11.5f, true, 0.08f));
        g.setColour (colours::text);
        g.drawText (profileStatus, statusRow.withTrimmedLeft (12), juce::Justification::centredLeft);
        g.setFont (uiFont (11.0f));
        g.setColour (colours::textDim);
        g.drawFittedText (profileDetail, a.removeFromTop (34), juce::Justification::topLeft, 2);
        if (learnMessage.isNotEmpty())
        {
            g.setColour (colours::warn);
            g.drawFittedText (learnMessage, a.removeFromTop (30), juce::Justification::topLeft, 2);
        }
    }

    // reduction read-out
    {
        auto a = reductionArea.reduced (14, 10);
        auto line = a.removeFromBottom (36 + 10).removeFromTop (14);
        juce::ignoreUnused (line);
        auto ro = reductionArea.withTrimmedTop (reductionArea.getHeight() - 70).removeFromTop (16).reduced (14, 0);
        g.setFont (uiFont (10.0f, true, 0.12f));
        g.setColour (colours::textFaint);
        g.drawText ("CURRENT REDUCTION", ro, juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText (juce::String (reductionDb, 1) + " dB", ro, juce::Justification::centredRight);
    }

    // gate state dot
    {
        const bool on = proc.getState().getRawParameterValue (ParamID::gateOn)->load() > 0.5f;
        auto a = gateArea.reduced (16, 0).removeFromTop (40);
        auto t = a.removeFromRight (90);
        g.setFont (uiFont (10.0f, true, 0.12f));
        g.setColour (on ? (gateOpen ? colours::text : colours::textFaint) : colours::textFaint);
        g.drawText (on ? (gateOpen ? "OPEN" : "CLOSED") : "OFF", t, juce::Justification::centredRight);
    }

    g.setColour (colours::textFaint);
    g.setFont (uiFont (9.0f));
    g.drawText ("v" DEADNOISE_VERSION_STRING, getLocalBounds().removeFromBottom (14).reduced (18, 0), juce::Justification::centredRight);
}

void DeadnoiseAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (60);
    r.reduce (16, 0);
    r.removeFromBottom (18);

    auto left = r.removeFromLeft (84);
    auto right = r.removeFromRight (84);
    r.reduce (12, 0);

    // side columns: meter + gain knob
    auto placeColumn = [] (juce::Rectangle<int> col, dnui::LevelMeter& m, dnui::Knob& k)
    {
        k.setBounds (col.removeFromBottom (98));
        col.removeFromBottom (8);
        m.setBounds (col.withSizeKeepingCentre (52, col.getHeight()));
    };
    placeColumn (left, inMeter, inGain);
    placeColumn (right, outMeter, outGain);

    spectrumArea = r.removeFromTop (172);
    spectrum.setBounds (spectrumArea.reduced (8, 8));
    r.removeFromTop (12);

    auto row2 = r.removeFromTop (214);
    r.removeFromTop (12);
    auto row3 = r;

    learnArea = row2.removeFromLeft (232);
    row2.removeFromLeft (12);
    voiceArea = row2.removeFromRight (212);
    row2.removeFromRight (12);
    reductionArea = row2;

    {
        auto a = learnArea.reduced (16, 0);
        a.removeFromTop (28);
        learnButton.setBounds (a.removeFromTop (44));
        clearButton.setBounds (learnArea.reduced (14, 12).removeFromBottom (24).removeFromRight (64));
    }
    {
        auto a = reductionArea.reduced (14, 10);
        mode.setBounds (a.removeFromBottom (30));
        a.removeFromBottom (36);
        reduction.setBounds (a.withSizeKeepingCentre (170, a.getHeight()));
    }
    {
        auto a = voiceArea.reduced (8, 10);
        voice.setBounds (a.removeFromTop (a.getHeight() / 2 + 6).withSizeKeepingCentre (130, a.getHeight() / 2 + 6));
        mix.setBounds (a.withSizeKeepingCentre (130, a.getHeight()));
    }

    resetButton.setBounds (row3.removeFromRight (96).withSizeKeepingCentre (96, 40));
    row3.removeFromRight (12);
    monitorArea = row3.removeFromRight (272);
    row3.removeFromRight (12);
    gateArea = row3;
    {
        auto a = monitorArea.reduced (14, 0);
        a.removeFromTop (34);
        monitor.setBounds (a.removeFromTop (32));
    }
    {
        auto a = gateArea.reduced (16, 0);
        gateToggle.setBounds (a.removeFromTop (40).removeFromLeft (150));
        auto knobs = gateArea.reduced (10, 6).withTrimmedTop (34);
        gateThresh.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).withSizeKeepingCentre (100, knobs.getHeight()));
        gateRange.setBounds (knobs.withSizeKeepingCentre (100, knobs.getHeight()));
    }
}

void DeadnoiseAudioProcessorEditor::updateLearnStatus()
{
    auto& d = proc.getEngine().getDenoiser();
    const auto st = d.getLearnState();
    learning = st == deadnoise::LearnState::requested || st == deadnoise::LearnState::capturing
            || st == deadnoise::LearnState::captureDone || st == deadnoise::LearnState::analysing;
    learnProgress = learning ? d.getLearnProgress() : 0.0f;
    learnButton.setButtonText (st == deadnoise::LearnState::requested || st == deadnoise::LearnState::capturing
                                   ? "CANCEL" : "LEARN BACKGROUND");
    learnButton.setToggleState (learning, juce::dontSendNotification);
    clearButton.setEnabled (d.hasProfile() && ! learning);

    if (st == deadnoise::LearnState::failedActivity)
        learnMessage = "Too much sound during learning. Stay silent and try again.";
    else if (st == deadnoise::LearnState::failedNoSignal)
        learnMessage = "No input signal. Check the microphone routing to this insert.";
    else if (learning || st == deadnoise::LearnState::succeeded)
        learnMessage.clear();

    if (st == deadnoise::LearnState::requested)
    {
        profileStatus = "WAITING FOR AUDIO";
        profileDetail = "Learning starts when the host sends audio.";
        profileColour = colours::warn;
    }
    else if (st == deadnoise::LearnState::capturing)
    {
        profileStatus = "LEARNING " + juce::String (juce::roundToInt (learnProgress * 100.0f)) + " %";
        profileDetail = "Stay silent: measuring background noise.";
        profileColour = colours::warn;
    }
    else if (st == deadnoise::LearnState::captureDone || st == deadnoise::LearnState::analysing)
    {
        profileStatus = "ANALYSING";
        profileDetail = "Building noise profile...";
        profileColour = colours::warn;
    }
    else if (d.hasProfile())
    {
        const auto prof = d.getProfile();
        profileStatus = "PROFILE LEARNED";
        profileDetail = "Noise floor " + juce::String (prof.levelDb, 1) + " dBFS, "
                      + juce::String (juce::roundToInt (prof.acceptedFraction * 100.0f)) + " % clean frames";
        if (! d.profileMatchesCurrentRate())
            profileDetail = "Learned at " + juce::String (prof.sampleRate / 1000.0, 1) + " kHz. Relearn recommended.";
        profileColour = colours::text;
    }
    else
    {
        profileStatus = "ADAPTIVE (NO PROFILE)";
        profileDetail = "Press Learn and stay silent for 4 s for best results.";
        profileColour = colours::textFaint;
    }
}

void DeadnoiseAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = lastTimerMs > 0.0 ? (float) juce::jlimit (0.001, 0.2, (now - lastTimerMs) * 0.001) : 1.0f / 30.0f;
    lastTimerMs = now;

    auto& e = proc.getEngine();
    inMeter.pushPeak (e.takeInputPeak(), dt);
    outMeter.pushPeak (e.takeOutputPeak(), dt);
    spectrum.refresh();

    const float red = e.getDenoiser().getAverageReductionDb();
    reductionDb = 0.8f * reductionDb + 0.2f * red;
    speechActivity = e.getDenoiser().getSpeechActivity();
    gateOpen = e.isGateOpen();

    const bool enableGate = proc.getState().getRawParameterValue (ParamID::gateOn)->load() > 0.5f;
    gateThresh.slider.setEnabled (enableGate);
    gateRange.slider.setEnabled (enableGate);

    updateLearnStatus();
    repaint (headerArea);
    repaint (learnArea);
    repaint (reductionArea);
    repaint (gateArea);
}
