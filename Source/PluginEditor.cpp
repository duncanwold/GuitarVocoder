#include "PluginEditor.h"

//==============================================================================
// AGILookAndFeel
//==============================================================================
AGILookAndFeel::AGILookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, AGI::bg);
    setColour(juce::TextButton::buttonColourId, AGI::surfaceW);
    setColour(juce::TextButton::textColourOffId, AGI::textDim);
    setColour(juce::ComboBox::backgroundColourId, AGI::surface);
    setColour(juce::ComboBox::outlineColourId, AGI::border);
    setColour(juce::ComboBox::textColourId, AGI::text);
    setColour(juce::PopupMenu::backgroundColourId, AGI::surface);
    setColour(juce::PopupMenu::textColourId, AGI::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, AGI::surfaceW);
    setColour(juce::ScrollBar::thumbColourId, AGI::border);
    setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    setDefaultSansSerifTypefaceName("Menlo");
}

void AGILookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float sliderPos, float, float,
                                       juce::Slider::SliderStyle, juce::Slider& slider)
{
    float trackY = (float)y + (float)h * 0.5f;
    float trackH = 3.0f;

    bool isVoice   = slider.getName().startsWith("voice_");
    bool isMix     = slider.getName().startsWith("mix_");
    bool isBipolar = slider.getName().startsWith("bipolar_");

    // Track background
    g.setColour(AGI::knobTrack);
    g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, (float)w, trackH, 2.0f);

    // Fill
    if (isMix)
    {
        float fillW = sliderPos - (float)x;
        juce::ColourGradient grad(AGI::textDim, (float)x, trackY,
                                   AGI::purple, (float)(x + w), trackY, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, fillW, trackH, 2.0f);
    }
    else if (isBipolar)
    {
        float centerX = (float)x + (float)w * 0.5f;
        float fillStart = std::min(centerX, sliderPos);
        float fillEnd = std::max(centerX, sliderPos);
        g.setColour(AGI::guitar);
        g.fillRoundedRectangle(fillStart, trackY - trackH * 0.5f,
                               fillEnd - fillStart, trackH, 2.0f);
        // Center detent tick
        g.setColour(AGI::textMid);
        g.fillRect(centerX - 0.5f, trackY - 5.0f, 1.0f, 10.0f);
    }
    else
    {
        float fillW = sliderPos - (float)x;
        g.setColour(isVoice ? AGI::voice : AGI::guitar);
        g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, fillW, trackH, 2.0f);
    }

    // Thumb
    float thumbR = isMix ? 7.0f : 5.0f;
    juce::Colour thumbBorder = isMix ? AGI::purple : (isVoice ? AGI::voice : AGI::guitar);
    g.setColour(AGI::text);
    g.fillEllipse(sliderPos - thumbR, trackY - thumbR, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour(thumbBorder);
    g.drawEllipse(sliderPos - thumbR, trackY - thumbR, thumbR * 2.0f, thumbR * 2.0f, 2.0f);
}

void AGILookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                       juce::Slider&)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;
    float strokeWidth = 3.5f;

    juce::Colour color = AGI::guitar;
    float range = rotaryEndAngle - rotaryStartAngle;
    float valueAngle = rotaryStartAngle + sliderPos * range;

    // Build arcs manually using the same coordinate transform as the thumb
    // to guarantee perfect alignment. JUCE slider angles: 0 = north, CW.
    // Screen position: x = cx + r*sin(a), y = cy - r*cos(a)
    auto makeArc = [&](float fromAngle, float toAngle) -> juce::Path {
        juce::Path p;
        const int steps = 64;
        float step = (toAngle - fromAngle) / (float)steps;
        for (int i = 0; i <= steps; ++i)
        {
            float a = fromAngle + (float)i * step;
            float px = cx + radius * std::sin(a);
            float py = cy - radius * std::cos(a);
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        return p;
    };

    // Track arc (full range)
    g.setColour(AGI::knobTrack);
    g.strokePath(makeArc(rotaryStartAngle, rotaryEndAngle),
                 juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Value arc (from start to current value)
    if (sliderPos > 0.005f)
    {
        g.setColour(color.withAlpha(0.8f));
        g.strokePath(makeArc(rotaryStartAngle, valueAngle),
                     juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    // Thumb
    float thumbX = cx + radius * std::sin(valueAngle);
    float thumbY = cy - radius * std::cos(valueAngle);
    g.setColour(AGI::text);
    g.fillEllipse(thumbX - 5.0f, thumbY - 5.0f, 10.0f, 10.0f);
    g.setColour(color);
    g.drawEllipse(thumbX - 5.0f, thumbY - 5.0f, 10.0f, 10.0f, 1.5f);
}

//==============================================================================
// VisualizerComponent — Split Mirror (full width)
//==============================================================================
VisualizerComponent::VisualizerComponent(GuitarVocoderProcessor& p)
    : proc(p), vocoder(p.getVocoder())
{
    smoothVoice.fill(0.0f);
    smoothGuitar.fill(0.0f);
    startTimerHz(30);
}

float VisualizerComponent::toDisplay(float linear)
{
    if (linear < kDisplayThreshold) return 0.0f;
    float db = 20.0f * std::log10(linear);
    return juce::jlimit(0.0f, 1.0f, (db + kDbRange) / kDbRange);
}

void VisualizerComponent::timerCallback()
{
    int numBands = vocoder.activeBandCount.load();

    bool anyChanged = false;
    for (int i = 0; i < numBands; ++i)
    {
        float vRaw = vocoder.modulatorEnvelopes[(size_t)i].load(std::memory_order_relaxed);
        float gRaw = vocoder.carrierEnvelopes[(size_t)i].load(std::memory_order_relaxed);
        if (vRaw != prevVoiceRaw[i] || gRaw != prevGuitarRaw[i])
            anyChanged = true;
        prevVoiceRaw[i] = vRaw;
        prevGuitarRaw[i] = gRaw;
    }

    if (anyChanged) staleFrames = 0; else staleFrames++;
    bool isStale = staleFrames >= kStaleThreshold;

    bool scNow = !isStale && proc.sideChainActive.load(std::memory_order_relaxed);
    bool monitorOn = proc.getAPVTS().getRawParameterValue("monitorGuitar")->load() >= 0.5f;
    for (int i = 0; i < numBands; ++i)
    {
        float vTarget = (isStale || monitorOn) ? 0.0f : toDisplay(prevVoiceRaw[i]);
        float gTarget = (isStale || !scNow) ? 0.0f : toDisplay(prevGuitarRaw[i]);
        smoothVoice[(size_t)i]  += (vTarget - smoothVoice[(size_t)i])  * kSmoothing;
        smoothGuitar[(size_t)i] += (gTarget - smoothGuitar[(size_t)i]) * kSmoothing;
    }
    for (int i = numBands; i < Vocoder::kMaxBands; ++i)
    {
        smoothVoice[(size_t)i] = 0.0f;
        smoothGuitar[(size_t)i] = 0.0f;
    }

    gateOpen = isStale ? false : proc.voiceGateGain.load(std::memory_order_relaxed) > 0.5f;
    scActive = isStale ? false : proc.sideChainActive.load(std::memory_order_relaxed);

    computeDiagnostics(numBands);

    if (diagBar != nullptr)
    {
        bool vClip = proc.voiceClip.load(std::memory_order_relaxed);
        bool gClip = proc.guitarClip.load(std::memory_order_relaxed);
        diagBar->updateState(gateOpen, scActive, matchPct, regionCoverage,
                             vClip, gClip);
    }

    repaint();
}

void VisualizerComponent::computeDiagnostics(int numBands)
{
    int loEnd = numBands / 4;
    int midEnd = numBands * 5 / 8;
    struct Region { int start; int end; };
    Region regions[3] = {{0, loEnd}, {loEnd, midEnd}, {midEnd, numBands}};

    int totalActive = 0, totalMatched = 0;
    for (int r = 0; r < 3; ++r)
    {
        int active = 0, matched = 0;
        for (int i = regions[r].start; i < regions[r].end; ++i)
        {
            if (smoothVoice[(size_t)i] > 0.06f)
            {
                active++; totalActive++;
                if (smoothGuitar[(size_t)i] > smoothVoice[(size_t)i] * 0.25f)
                { matched++; totalMatched++; }
            }
        }
        regionCoverage[r] = active > 0 ? (float)matched / (float)active : 1.0f;
    }
    matchPct = totalActive > 0 ? (int)(totalMatched * 100.0f / totalActive) : 0;
}

void VisualizerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(AGI::surface);
    g.fillRoundedRectangle(bounds, 10.0f);

    int numBands = vocoder.activeBandCount.load();
    if (numBands <= 0) numBands = 16;

    float pad = 12.0f;
    float plotX = bounds.getX() + pad;
    float plotY = bounds.getY() + pad;
    float plotW = bounds.getWidth() - pad * 2.0f;
    float plotH = bounds.getHeight() - pad * 2.0f - 14.0f;
    float midY = plotH * 0.5f;

    g.saveState();
    g.reduceClipRegion((int)plotX, (int)plotY, (int)plotW, (int)plotH);
    g.addTransform(juce::AffineTransform::translation(plotX, plotY));

    // Center line
    g.setColour(AGI::border);
    g.drawHorizontalLine((int)midY, 0.0f, plotW);

    // Grid lines
    g.setColour(AGI::knobTrack);
    g.drawHorizontalLine((int)(midY * 0.5f), 0.0f, plotW);
    g.drawHorizontalLine((int)(midY * 1.5f), 0.0f, plotW);

    // Per-band bars
    float bandW = plotW / (float)numBands;
    float barW = bandW * 0.6f;

    for (int i = 0; i < numBands; ++i)
    {
        float cx = ((float)i + 0.5f) * bandW;
        float vVal = juce::jlimit(0.0f, 1.0f, smoothVoice[(size_t)i]);
        float gVal = juce::jlimit(0.0f, 1.0f, smoothGuitar[(size_t)i]);
        float vH = vVal * (midY - 3.0f);
        float gH = gVal * (midY - 3.0f);

        if (vH > 0.5f)
        {
            g.setColour(AGI::voice.withAlpha(0.3f));
            g.fillRoundedRectangle(cx - barW * 0.5f, midY - vH, barW, vH, 1.5f);
            g.setColour(AGI::voice.withAlpha(0.5f));
            g.drawRoundedRectangle(cx - barW * 0.5f, midY - vH, barW, vH, 1.5f, 0.7f);
        }

        if (gH > 0.5f)
        {
            g.setColour(AGI::guitar.withAlpha(0.25f));
            g.fillRoundedRectangle(cx - barW * 0.5f, midY, barW, gH, 1.5f);
            g.setColour(AGI::guitar.withAlpha(0.5f));
            g.drawRoundedRectangle(cx - barW * 0.5f, midY, barW, gH, 1.5f, 0.7f);
        }

        if (vH > 2.0f && gH > 2.0f)
        {
            float overlap = juce::jmin(vH, gH);
            float purpleH = overlap * 0.4f;
            float purpleAlpha = 0.35f + (overlap / midY) * 0.4f;
            g.setColour(AGI::purple.withAlpha(purpleAlpha));
            g.fillRoundedRectangle(cx - barW * 0.5f, midY - purpleH,
                                   barW, purpleH * 2.0f, 2.0f);
            g.setColour(AGI::purple.withAlpha(juce::jmin(0.9f, purpleAlpha + 0.2f)));
            g.fillRect(cx - barW * 0.5f, midY - 0.5f, barW, 1.0f);
        }

        if (vVal > 0.08f && gVal < vVal * 0.25f)
        {
            float dotY = midY + gH + 8.0f;
            if (dotY < plotH - 4.0f)
            {
                g.setColour(AGI::amber.withAlpha(0.7f));
                g.fillEllipse(cx - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
                g.setColour(AGI::amber.withAlpha(0.3f));
                for (float dy = midY + gH + 1.0f; dy < dotY - 3.0f; dy += 3.0f)
                    g.fillRect(cx - 0.3f, dy, 0.6f, 1.5f);
            }
        }
    }

    // Labels inside the plot
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(10.0f)));
    g.setColour(AGI::voice.withAlpha(0.6f));
    g.drawText("VOICE", 3, 2, 40, 10, juce::Justification::left);
    g.setColour(AGI::guitar.withAlpha(0.6f));
    g.drawText("GUITAR", 3, (int)plotH - 12, 45, 10, juce::Justification::left);

    g.restoreState();

    // Frequency axis
    float freqY = plotY + plotH + 2.0f;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(10.0f)));
    g.setColour(AGI::textDim);
    const char* freqLabels[] = {"80", "250", "800", "2.5k", "8k", "12k"};
    for (int i = 0; i < 6; ++i)
    {
        float fx = plotX + (float)i * plotW / 5.0f;
        if (i == 0)
            g.drawText(freqLabels[i], (int)fx, (int)freqY, 40, 12, juce::Justification::left);
        else if (i == 5)
            g.drawText(freqLabels[i], (int)(plotX + plotW - 28), (int)freqY, 28, 12, juce::Justification::right);
        else
            g.drawText(freqLabels[i], (int)(fx - 20), (int)freqY, 40, 12, juce::Justification::centred);
    }
}

//==============================================================================
// DiagnosticsBarComponent
//==============================================================================
DiagnosticsBarComponent::DiagnosticsBarComponent()
{
    std::fill(chipState, chipState + cCount, 0);
    std::fill(chipLatchTime, chipLatchTime + cCount, 0.0);
}

void DiagnosticsBarComponent::updateState(bool go, bool sa, int mp, const float rc[3],
                                           bool vClip, bool gClip)
{
    gateOpen = go;
    scActive = sa;
    matchPct = mp;
    for (int i = 0; i < 3; ++i) regionCoverage[i] = rc[i];

    currentTime += 1.0 / 30.0;

    analysisFrameCount++;
    if (regionCoverage[2] < 0.4f && gateOpen && scActive) framesWithHiDeficit++;
    if (regionCoverage[0] < 0.4f && gateOpen && scActive) framesWithLoDeficit++;
    if (regionCoverage[1] < 0.4f && gateOpen && scActive) framesWithMidDeficit++;
    if (gateOpen && scActive && matchPct > 60) framesWithGoodMatch++;
    if (vClip) voiceClipped = true;
    if (gClip) guitarClipped = true;

    if (analysisFrameCount >= kAnalysisWindow)
    {
        analysisFrameCount = 0;
        framesWithHiDeficit = framesWithLoDeficit = framesWithMidDeficit = 0;
        framesWithGoodMatch = 0;
        voiceClipped = false;
        guitarClipped = false;
    }

    updateChips();
    repaint();
}

void DiagnosticsBarComponent::updateChips()
{
    float threshold = 0.3f;
    int window = juce::jmax(1, analysisFrameCount);

    bool matchActive = gateOpen && scActive && matchPct > 60;
    bool loActive = gateOpen && scActive && (float)framesWithLoDeficit / window > threshold;
    bool midActive = gateOpen && scActive && (float)framesWithMidDeficit / window > threshold;
    bool hiActive = gateOpen && scActive && (float)framesWithHiDeficit / window > threshold;
    bool clipActive = voiceClipped || guitarClipped;

    bool active[cCount] = { matchActive, loActive, midActive, hiActive, clipActive };

    for (int i = 0; i < cCount; ++i)
    {
        if (active[i])
        {
            chipState[i] = 2;
            chipLatchTime[i] = currentTime;
        }
        else if (chipState[i] == 2)
        {
            if (i == cMatch)
                chipState[i] = 0;
            else
            {
                chipState[i] = 1;
                chipLatchTime[i] = currentTime;
            }
        }
        else if (chipState[i] == 1)
        {
            if (currentTime - chipLatchTime[i] > kLatchDecaySeconds)
                chipState[i] = 0;
        }
    }
}

void DiagnosticsBarComponent::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < cCount; ++i)
    {
        if (chipState[i] == 1 && getChipBounds(i).contains(e.getPosition()))
        {
            chipState[i] = 0;
            repaint();
            return;
        }
    }
    // CLEAR button
    bool hasLatched = false;
    for (int i = 0; i < cCount; ++i)
        if (chipState[i] == 1) hasLatched = true;
    if (hasLatched)
    {
        auto clearBounds = juce::Rectangle<int>(getWidth() - 60, getHeight() - 22, 55, 18);
        if (clearBounds.contains(e.getPosition()))
        {
            for (int i = 0; i < cCount; ++i)
                if (chipState[i] == 1) chipState[i] = 0;
            repaint();
        }
    }
}

void DiagnosticsBarComponent::mouseMove(const juce::MouseEvent& e)
{
    int newHovered = -1;
    for (int i = 0; i < cCount; ++i)
        if (getChipBounds(i).contains(e.getPosition()))
            newHovered = i;
    if (newHovered != hoveredChip) { hoveredChip = newHovered; repaint(); }
}

void DiagnosticsBarComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoveredChip >= 0) { hoveredChip = -1; repaint(); }
}

juce::String DiagnosticsBarComponent::getChipLabel(int i) const
{
    const char* labels[] = { "Match", "Lows", "Mids", "Highs", "Clip" };
    return labels[i];
}

juce::String DiagnosticsBarComponent::getChipFixText(int i) const
{
    switch (i) {
        case cMatch:  return "";
        case cLows:   return juce::CharPointer_UTF8("Tone \xe2\x86\x92 left, Octave left, or Spread");
        case cMids:   return "Spread, or less scooped guitar tone";
        case cHighs:  return juce::CharPointer_UTF8("Tone \xe2\x86\x92 right, Drive, or Spread");
        case cClip:   return "Reduce input gain";
        default: return "";
    }
}

bool DiagnosticsBarComponent::isChipGood(int i) const { return i == cMatch; }

juce::Rectangle<int> DiagnosticsBarComponent::getChipBounds(int chipIndex) const
{
    float px = 14.0f;
    float dy = 10.0f;
    float div2X = px + 68.0f + 12.0f + 110.0f + 200.0f + 4.0f;
    float chipX = div2X + 12.0f;
    float chipW = ((float)getWidth() - px - chipX) / (float)cCount;
    float chipH = 26.0f;
    return juce::Rectangle<float>(chipX + chipIndex * chipW, dy, chipW - 5.0f, chipH).toNearestInt();
}

void DiagnosticsBarComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(AGI::surface);
    g.fillRoundedRectangle(bounds, 8.0f);

    float px = 14.0f;
    float dy = bounds.getY() + 10.0f;

    // === Voice/Guitar LEDs (stacked) ===
    float ledX = px;
    auto drawLed = [&](bool on, juce::Colour color, const juce::String& label, float y) {
        g.setColour(on ? color : AGI::knobTrack);
        g.fillEllipse(ledX, y, 9.0f, 9.0f);
        g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
        g.setColour(on ? AGI::text : AGI::textDim);
        g.drawText(label, (int)(ledX + 14.0f), (int)(y - 1.0f), 50, 12, juce::Justification::left);
    };
    drawLed(gateOpen, AGI::voice, "Voice", dy);
    drawLed(scActive, AGI::guitar, "Guitar", dy + 16.0f);

    // Divider
    float divX = ledX + 68.0f;
    g.setColour(AGI::border);
    g.drawVerticalLine((int)divX, dy, dy + 30.0f);

    // === Match % ===
    float matchX = divX + 12.0f;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
    g.setColour(AGI::textMid);
    g.drawText("Match", (int)matchX, (int)dy, 50, 14, juce::Justification::left);

    juce::Colour matchCol = (matchPct > 60) ? AGI::green :
                            (matchPct > 30) ? AGI::amber : AGI::voice;
    if (!scActive) matchCol = AGI::textDim;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(20.0f).withStyle("Bold")));
    g.setColour(matchCol);
    g.drawText(juce::String(matchPct) + "%", (int)(matchX + 52.0f), (int)(dy - 2.0f), 50, 20,
               juce::Justification::left);

    // === Lo/Mid/Hi coverage meters ===
    float covX = matchX + 110.0f;
    float covW = 200.0f;
    float segGap = 8.0f;
    const char* covLabels[] = { "Lo", "Mid", "Hi" };
    float segWidths[] = { covW * 0.25f, covW * 0.45f, covW * 0.3f };
    float cx = covX;

    for (int r = 0; r < 3; ++r)
    {
        float sw = segWidths[r] - segGap;
        float cov = regionCoverage[r];
        juce::Colour covCol = (cov > 0.6f) ? AGI::green :
                              (cov > 0.3f) ? AGI::amber : juce::Colour(0xffFF4444);
        if (!scActive) covCol = AGI::textDim;

        g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
        g.setColour(AGI::textMid);
        g.drawText(covLabels[r], (int)cx, (int)dy, (int)sw, 12, juce::Justification::left);

        float barY = dy + 15.0f;
        g.setColour(AGI::knobTrack);
        g.fillRoundedRectangle(cx, barY, sw, 6.0f, 3.0f);
        g.setColour(covCol);
        g.fillRoundedRectangle(cx, barY, juce::jlimit(0.0f, sw, cov * sw), 6.0f, 3.0f);

        cx += segWidths[r];
    }

    // Divider
    float div2X = covX + covW + 4.0f;
    g.setColour(AGI::border);
    g.drawVerticalLine((int)div2X, dy, dy + 30.0f);

    // === Hint Chips ===
    float chipX = div2X + 12.0f;
    float chipW = (bounds.getRight() - px - chipX) / (float)cCount;
    float chipH = 26.0f;

    for (int i = 0; i < cCount; ++i)
    {
        auto chipBounds = juce::Rectangle<float>(chipX + i * chipW, dy, chipW - 5.0f, chipH);

        int state = chipState[i];
        bool good = isChipGood(i);
        juce::Colour chipCol;
        if (good) chipCol = AGI::green;
        else if (i == cClip) chipCol = juce::Colour(0xffFF4444);
        else chipCol = AGI::amber;
        bool focused = (hoveredChip == i) && (state > 0);

        if (state > 0)
        {
            float alpha = focused ? 0.12f : (state == 2 ? 0.1f : 0.04f);
            g.setColour(chipCol.withAlpha(alpha));
            g.fillRoundedRectangle(chipBounds, 5.0f);
        }

        juce::Colour brdCol = (state == 0) ? AGI::border :
                              (state == 1) ? chipCol.withAlpha(0.3f) : chipCol.withAlpha(0.5f);
        if (focused) brdCol = chipCol.withAlpha(0.6f);
        g.setColour(brdCol);
        g.drawRoundedRectangle(chipBounds, 5.0f, 1.0f);

        float dotX = chipBounds.getX() + 10.0f;
        float dotY = chipBounds.getCentreY() - 4.0f;
        juce::Colour dotCol = (state == 0) ? AGI::knobTrack :
                              (state == 1) ? chipCol.withAlpha(0.5f) : chipCol;
        g.setColour(dotCol);
        g.fillEllipse(dotX, dotY, 8.0f, 8.0f);

        g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(12.0f)
                    .withStyle(state == 2 ? "Bold" : "")));
        juce::Colour txtCol = (state == 0) ? AGI::textDim :
                              (state == 1) ? chipCol.withAlpha(0.7f) : chipCol;
        g.setColour(txtCol);
        g.drawText(getChipLabel(i), (int)(dotX + 12.0f), (int)(chipBounds.getY()),
                   (int)(chipBounds.getWidth() - 24.0f), (int)chipH, juce::Justification::centredLeft);
    }

    // === Fix text area ===
    float fixY = dy + chipH + 6.0f;
    juce::String fixText;

    if (hoveredChip >= 0 && chipState[hoveredChip] > 0)
        fixText = getChipFixText(hoveredChip);
    else
    {
        for (int i = 0; i < cCount; ++i)
            if (chipState[i] == 2 && !isChipGood(i) && getChipFixText(i).isNotEmpty())
            { fixText = getChipFixText(i); break; }
    }

    if (fixText.isNotEmpty())
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.setColour(AGI::textMid.withAlpha(0.85f));
        g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x86\x92 ")) + fixText,
                   (int)chipX, (int)fixY, (int)(bounds.getRight() - px - chipX - 60), 16,
                   juce::Justification::centredLeft);
    }
    else if (hoveredChip >= 0 && chipState[hoveredChip] == 1)
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.setColour(AGI::textDim.withAlpha(0.5f));
        g.drawText("Click to clear", (int)chipX, (int)fixY, 120, 16, juce::Justification::centredLeft);
    }
    else
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.setColour(AGI::textDim.withAlpha(0.3f));
        g.drawText("Hover a chip for details", (int)chipX, (int)fixY, 200, 16, juce::Justification::centredLeft);
    }

    // CLEAR button
    bool hasLatched = false;
    for (int i = 0; i < cCount; ++i)
        if (chipState[i] == 1) hasLatched = true;
    if (hasLatched)
    {
        auto clearBounds = juce::Rectangle<float>(bounds.getRight() - px - 50.0f, fixY, 46.0f, 18.0f);
        g.setColour(AGI::border);
        g.drawRoundedRectangle(clearBounds, 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
        g.setColour(AGI::textDim);
        g.drawText("CLEAR", clearBounds.toNearestInt(), juce::Justification::centred);
    }
}

//==============================================================================
// InputMeter
//==============================================================================
InputMeter::InputMeter(GuitarVocoderProcessor& p) : proc(p)
{
    startTimerHz(30);
}

void InputMeter::timerCallback()
{
    float vp = proc.voicePeakLevel.load(std::memory_order_relaxed);
    float gp = proc.guitarPeakLevel.load(std::memory_order_relaxed);
    voiceLevel = (vp > voiceLevel) ? vp : voiceLevel * kDecay;
    guitarLevel = (gp > guitarLevel) ? gp : guitarLevel * kDecay;

    if (proc.voiceClip.exchange(false, std::memory_order_relaxed))
        voiceClipTimer = kClipHoldFrames;
    if (proc.guitarClip.exchange(false, std::memory_order_relaxed))
        guitarClipTimer = kClipHoldFrames;

    voiceClipHeld = (voiceClipTimer > 0);
    guitarClipHeld = (guitarClipTimer > 0);
    if (voiceClipTimer > 0) voiceClipTimer--;
    if (guitarClipTimer > 0) guitarClipTimer--;

    repaint();
}

void InputMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float labelH = 10.0f;
    float clipH = 4.0f;
    float meterW = (b.getWidth() - 3.0f) * 0.5f;
    float meterH = b.getHeight() - clipH - 2.0f - labelH;

    float vx = b.getX();
    float vy = b.getY() + clipH + 2.0f;
    g.setColour(AGI::knobTrack);
    g.fillRoundedRectangle(vx, vy, meterW, meterH, 2.0f);
    float vFill = juce::jlimit(0.0f, 1.0f, voiceLevel);
    float vBarH = vFill * meterH;
    g.setColour(AGI::voice);
    g.fillRoundedRectangle(vx, vy + meterH - vBarH, meterW, vBarH, 2.0f);

    g.setColour(voiceClipHeld ? juce::Colours::red : AGI::knobTrack);
    g.fillRoundedRectangle(vx, b.getY(), meterW, clipH, 1.5f);

    float gx = vx + meterW + 3.0f;
    g.setColour(AGI::knobTrack);
    g.fillRoundedRectangle(gx, vy, meterW, meterH, 2.0f);
    float gFill = juce::jlimit(0.0f, 1.0f, guitarLevel);
    float gBarH = gFill * meterH;
    g.setColour(AGI::guitar);
    g.fillRoundedRectangle(gx, vy + meterH - gBarH, meterW, gBarH, 2.0f);

    g.setColour(guitarClipHeld ? juce::Colours::red : AGI::knobTrack);
    g.fillRoundedRectangle(gx, b.getY(), meterW, clipH, 1.5f);

    float ly = vy + meterH + 1.0f;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.setColour(AGI::voice);
    g.drawText("V", (int)vx, (int)ly, (int)meterW, (int)labelH, juce::Justification::centred);
    g.setColour(AGI::guitar);
    g.drawText("G", (int)gx, (int)ly, (int)meterW, (int)labelH, juce::Justification::centred);
}

//==============================================================================
// BandSelector
//==============================================================================
BandSelector::BandSelector(juce::AudioProcessorValueTreeState& apvts)
{
    hiddenBox.addItemList({"8", "16", "24", "32"}, 1);
    hiddenBox.setVisible(false);
    addAndMakeVisible(hiddenBox);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "numBands", hiddenBox);
    hiddenBox.onChange = [this]() { updateButtons(); };

    auto setupBtn = [this](juce::TextButton& b, int idx)
    {
        b.setClickingTogglesState(false);
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::TextButton::buttonOnColourId, AGI::voice);
        b.setColour(juce::TextButton::textColourOffId, AGI::textDim);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(b);
        b.onClick = [this, idx]()
        {
            hiddenBox.setSelectedItemIndex(idx, juce::sendNotificationSync);
            updateButtons();
        };
    };

    setupBtn(btn8, 0);
    setupBtn(btn16, 1);
    setupBtn(btn24, 2);
    setupBtn(btn32, 3);
    updateButtons();
}

void BandSelector::updateButtons()
{
    int sel = hiddenBox.getSelectedItemIndex();
    btn8.setToggleState(sel == 0, juce::dontSendNotification);
    btn16.setToggleState(sel == 1, juce::dontSendNotification);
    btn24.setToggleState(sel == 2, juce::dontSendNotification);
    btn32.setToggleState(sel == 3, juce::dontSendNotification);
}

void BandSelector::paint(juce::Graphics& g)
{
    g.setColour(AGI::surfaceW);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
}

void BandSelector::resized()
{
    auto b = getLocalBounds().reduced(3, 0);
    int btnW = b.getWidth() / 4;
    btn8.setBounds(b.removeFromLeft(btnW));
    btn16.setBounds(b.removeFromLeft(btnW));
    btn24.setBounds(b.removeFromLeft(btnW));
    btn32.setBounds(b);
}

//==============================================================================
// AdvancedPanel
//==============================================================================
AdvancedPanel::AdvancedPanel(GuitarVocoderProcessor& p) : proc(p), apvts(p.getAPVTS())
{
    startTimerHz(10);
}

void AdvancedPanel::timerCallback()
{
    if (!expanded) return;

    response  = *apvts.getRawParameterValue("response");
    clarity   = *apvts.getRawParameterValue("clarity");
    loAtk     = *apvts.getRawParameterValue("loAttack");
    loRel     = *apvts.getRawParameterValue("loRelease");
    hiAtk     = *apvts.getRawParameterValue("hiAttack");
    hiRel     = *apvts.getRawParameterValue("hiRelease");
    noise     = *apvts.getRawParameterValue("noise");
    uvSens    = *apvts.getRawParameterValue("unvoicedSens");
    dynamics  = *apvts.getRawParameterValue("voiceDynamics");
    hiBoost   = *apvts.getRawParameterValue("voiceHighBoost");
    deEssDisplay = *apvts.getRawParameterValue("deEss");
    noiseFloorDisplay = proc.noiseFloorDb.load(std::memory_order_relaxed);
    gateThreshDisplay = proc.gateThresholdDb.load(std::memory_order_relaxed);

    repaint();
}

int AdvancedPanel::getRequiredHeight() const
{
    return expanded ? 130 : 24;
}

void AdvancedPanel::resized()
{
}

void AdvancedPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y < 24)
    {
        expanded = !expanded;
        if (onToggle) onToggle();
    }
}

void AdvancedPanel::paint(juce::Graphics& g)
{
    int W = getWidth();

    // Top border
    g.setColour(AGI::border);
    g.drawLine(0.0f, 0.0f, (float)W, 0.0f, 1.0f);

    // Header: "ADVANCED" + arrow
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(12.0f)));
    g.setColour(AGI::textDim);

    juce::String arrow = expanded ? juce::CharPointer_UTF8("\xe2\x96\xbe") : juce::CharPointer_UTF8("\xe2\x96\xb8");
    g.drawText(arrow + " ADVANCED", 22, 6, 120, 16, juce::Justification::centredLeft);

    if (!expanded) return;

    int pad = 22;
    int gap = 16;  // gap between columns for dividers
    int colW = (W - pad * 2 - gap * 2) / 3;
    int col1X = pad;
    int col2X = pad + colW + gap;
    int col3X = pad + colW * 2 + gap * 2;
    int y0 = 28;
    int rowH = 15;
    int barW = 50;
    int barH = 3;

    auto drawRow = [&](int x, int cw, int& yPos, const juce::String& name, const juce::String& value,
                       float normalized, bool isFixed = false)
    {
        g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
        g.setColour(isFixed ? AGI::textDim : AGI::textMid);
        g.drawText(name, x, yPos, cw - barW - 12, rowH, juce::Justification::centredLeft);
        g.drawText(value, x + cw - barW - 60, yPos, 56, rowH, juce::Justification::centredRight);

        if (!isFixed)
        {
            int bx = x + cw - barW;
            int by = yPos + (rowH - barH) / 2;
            g.setColour(AGI::knobTrack);
            g.fillRoundedRectangle((float)bx, (float)by, (float)barW, (float)barH, 1.5f);
            g.setColour(AGI::textMid);
            float fillW = juce::jlimit(0.0f, 1.0f, normalized) * (float)barW;
            g.fillRoundedRectangle((float)bx, (float)by, fillW, (float)barH, 1.5f);
        }

        yPos += rowH;
    };

    // === Column 1: Response ===
    int ry = y0;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(12.0f)));
    g.setColour(AGI::voice);
    g.drawText(juce::String(juce::CharPointer_UTF8("Response \xe2\x86\x92 ")) + juce::String((int)(response * 100)) + "%",
               col1X, ry, colW, 14, juce::Justification::centredLeft);
    ry += 16;

    drawRow(col1X, colW, ry, "Lo Attack",  juce::String(loAtk, 1) + " ms", (loAtk - 0.5f) / 49.5f);
    drawRow(col1X, colW, ry, "Lo Release", juce::String((int)loRel) + " ms", (loRel - 15.0f) / 385.0f);
    drawRow(col1X, colW, ry, "Hi Attack",  juce::String(hiAtk, 1) + " ms", (hiAtk - 0.5f) / 19.5f);
    drawRow(col1X, colW, ry, "Hi Release", juce::String((int)hiRel) + " ms", (hiRel - 5.0f) / 115.0f);

    // Column divider
    g.setColour(AGI::border);
    g.drawVerticalLine(col1X + colW + gap / 2, (float)y0, (float)(y0 + 16 + rowH * 5));

    // === Column 2: Clarity ===
    int cy = y0;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(12.0f)));
    g.setColour(AGI::voice);
    g.drawText(juce::String(juce::CharPointer_UTF8("Clarity \xe2\x86\x92 ")) + juce::String((int)(clarity * 100)) + "%",
               col2X, cy, colW, 14, juce::Justification::centredLeft);
    cy += 16;

    drawRow(col2X, colW, cy, "Noise Blend", juce::String((int)(noise * 100)) + "% mix", noise / 0.45f);
    drawRow(col2X, colW, cy, "Unvoiced Det", juce::String((int)juce::jmin(100.0f, uvSens * 100)) + "%", juce::jmin(1.0f, (uvSens - 0.1f) / 0.55f));
    drawRow(col2X, colW, cy, "Dynamics",    juce::String((int)(dynamics * 100)) + "%", dynamics);
    drawRow(col2X, colW, cy, "Hi Boost",    juce::String(hiBoost, 1) + " dB", hiBoost / 10.0f);

    float deEssMinGain = 0.5f - deEssDisplay * 0.4f;
    float deEssDb = (deEssDisplay > 0.001f) ? -20.0f * std::log10(deEssMinGain) : 0.0f;
    drawRow(col2X, colW, cy, "De-Ess", "-" + juce::String((int)deEssDb) + " dB", deEssDisplay);

    // Column divider
    g.setColour(AGI::border);
    g.drawVerticalLine(col2X + colW + gap / 2, (float)y0, (float)(y0 + 16 + rowH * 5));

    // === Column 3: Global ===
    int gy = y0;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(12.0f)));
    g.setColour(AGI::textMid);
    g.drawText("Global", col3X, gy, colW, 14, juce::Justification::centredLeft);
    gy += 16;

    drawRow(col3X, colW, gy, "Noise Floor",
            juce::String((int)noiseFloorDisplay) + " dB",
            juce::jlimit(0.0f, 1.0f, (noiseFloorDisplay + 70.0f) / 50.0f));
    drawRow(col3X, colW, gy, "Gate Thresh",
            juce::String((int)gateThreshDisplay) + " dB",
            juce::jlimit(0.0f, 1.0f, (gateThreshDisplay + 55.0f) / 35.0f));
    drawRow(col3X, colW, gy, "Gate Offset", "+7 dB", 0, true);
    drawRow(col3X, colW, gy, "Lo Cut",      "80 Hz", 0, true);
    drawRow(col3X, colW, gy, "Unvoiced Bands", "4", 0, true);
}

//==============================================================================
// GuitarVocoderEditor
//==============================================================================
GuitarVocoderEditor::GuitarVocoderEditor(GuitarVocoderProcessor& p)
    : AudioProcessorEditor(&p),
      proc(p),
      inputMeter(p),
      visualizer(p),
      bandSelector(p.getAPVTS()),
      advancedPanel(p)
{
    setLookAndFeel(&agiLnf);
    setSize(kWidth, computeWindowHeight());

    // --- Header ---
    addAndMakeVisible(inputMeter);

    monitorBtn.setClickingTogglesState(true);
    monitorBtn.setColour(juce::TextButton::buttonColourId, AGI::surfaceW);
    monitorBtn.setColour(juce::TextButton::buttonOnColourId, AGI::guitar);
    monitorBtn.setColour(juce::TextButton::textColourOffId, AGI::guitar.withAlpha(0.5f));
    monitorBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(monitorBtn);
    monitorAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.getAPVTS(), "monitorGuitar", monitorBtn);

    // Preset controls
    savePresetBtn.setColour(juce::TextButton::buttonColourId, AGI::surfaceW);
    savePresetBtn.setColour(juce::TextButton::textColourOffId, AGI::textDim);
    savePresetBtn.setVisible(false);  // Hidden for v1.0

    presetBox.setTextWhenNothingSelected("Presets");
    addAndMakeVisible(presetBox);
    rebuildPresetBox();

    presetBox.onChange = [this]() {
        int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
        {
            loadingPreset = true;
            proc.getPresetManager().loadPreset(idx);
            loadingPreset = false;
        }
    };
    savePresetBtn.onClick = [this]() {
        auto* aw = new juce::AlertWindow("Save Preset", "Enter a name for this preset:",
                                          juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor("name", "", "Name:");
        aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        auto safeThis = juce::Component::SafePointer<GuitarVocoderEditor>(this);
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [safeThis, aw](int result) {
                if (result == 1 && safeThis != nullptr)
                {
                    auto name = aw->getTextEditorContents("name").trim();
                    if (name.isNotEmpty())
                    {
                        safeThis->proc.getPresetManager().saveUserPreset(name);
                        safeThis->rebuildPresetBox();
                    }
                }
                delete aw;
            }), false);
    };

    {
        int idx = proc.getPresetManager().getCurrentIndex();
        if (idx >= 0)
            presetBox.setSelectedItemIndex(idx, juce::dontSendNotification);
    }

    // --- Visualizer + Diagnostics Bar + Band Selector ---
    addAndMakeVisible(visualizer);
    addAndMakeVisible(diagnosticsBar);
    visualizer.setDiagnosticsBar(&diagnosticsBar);
    addAndMakeVisible(bandSelector);

    // --- Slider setup helper ---
    auto setupSlider = [this](juce::Slider& slider, juce::Label& valLbl,
                              const juce::String& name,
                              juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& paramId,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setName(name);
        addAndMakeVisible(slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);

        bool isVoice = name.startsWith("voice_");
        valLbl.setColour(juce::Label::textColourId, isVoice ? AGI::voice : AGI::guitar);
        valLbl.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
        valLbl.setJustificationType(juce::Justification::centredRight);
        valLbl.setEditable(false, true, false);
        valLbl.setColour(juce::Label::backgroundWhenEditingColourId, AGI::surface);
        valLbl.setColour(juce::Label::outlineWhenEditingColourId, AGI::border);
        valLbl.setColour(juce::Label::textWhenEditingColourId, isVoice ? AGI::voice : AGI::guitar);
        valLbl.onTextChange = [&slider, &valLbl]() {
            juce::String text = valLbl.getText().trimCharactersAtEnd("%");
            float val = text.getFloatValue();
            val = juce::jlimit(0.0f, 100.0f, val) / 100.0f;
            slider.setValue((double)val, juce::sendNotificationSync);
        };
        addAndMakeVisible(valLbl);
    };

    // --- Vocoder sliders (pink) ---
    setupSlider(responseSlider, responseValLbl, "voice_response", p.getAPVTS(), "response", responseAtt);
    responseSlider.onValueChange = [this]() {
        responseValLbl.setText(juce::String((int)(responseSlider.getValue() * 100)) + "%", juce::dontSendNotification);
    };
    responseSlider.onValueChange();

    setupSlider(claritySlider, clarityValLbl, "voice_clarity", p.getAPVTS(), "clarity", clarityAtt);
    claritySlider.onValueChange = [this]() {
        clarityValLbl.setText(juce::String((int)(claritySlider.getValue() * 100)) + "%", juce::dontSendNotification);
    };
    claritySlider.onValueChange();

    // --- Enrichment sliders (cyan) ---
    setupSlider(toneSlider, toneValLbl, "bipolar_tone", p.getAPVTS(), "tone", toneAtt);
    toneSlider.setDoubleClickReturnValue(true, 0.5);
    toneSlider.onValueChange = [this]() {
        float v = (float)toneSlider.getValue();
        float bp = v * 2.0f - 1.0f;
        if (std::abs(bp) < 0.02f)
            toneValLbl.setText("FLAT", juce::dontSendNotification);
        else
            toneValLbl.setText(juce::String((int)(v * 100)) + "%", juce::dontSendNotification);
    };
    toneSlider.onDragEnd = [this]() {
        float v = (float)toneSlider.getValue();
        if (std::abs(v - 0.5) < 0.02)
            toneSlider.setValue(0.5, juce::sendNotificationSync);
    };
    toneSlider.onValueChange();
    toneValLbl.onTextChange = [this]() {
        juce::String text = toneValLbl.getText().trim().toUpperCase();
        if (text == "FLAT" || text == "CENTER" || text == "CTR")
            toneSlider.setValue(0.5, juce::sendNotificationSync);
        else
        {
            float val = text.trimCharactersAtEnd("%").getFloatValue();
            val = juce::jlimit(0.0f, 100.0f, val) / 100.0f;
            toneSlider.setValue((double)val, juce::sendNotificationSync);
        }
    };

    setupSlider(spreadSlider, spreadValLbl, "guitar_spread", p.getAPVTS(), "spread", spreadAtt);
    spreadSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    spreadSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
    spreadValLbl.setJustificationType(juce::Justification::centred);
    spreadSlider.onValueChange = [this]() {
        spreadValLbl.setText(juce::String((int)(spreadSlider.getValue() * 100)) + "%", juce::dontSendNotification);
    };
    spreadSlider.onValueChange();

    setupSlider(driveSlider, driveValLbl, "guitar_drive", p.getAPVTS(), "drive", driveAtt);
    driveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f, true);
    driveValLbl.setJustificationType(juce::Justification::centred);
    driveSlider.onValueChange = [this]() {
        driveValLbl.setText(juce::String((int)(driveSlider.getValue() * 100)) + "%", juce::dontSendNotification);
    };
    driveSlider.onValueChange();

    setupSlider(octaveSlider, octaveValLbl, "bipolar_octave", p.getAPVTS(), "octave", octaveAtt);
    octaveSlider.setDoubleClickReturnValue(true, 0.5);
    octaveSlider.onValueChange = [this]() {
        float v = (float)octaveSlider.getValue();
        float bp = v * 2.0f - 1.0f;
        if (std::abs(bp) < 0.02f)
            octaveValLbl.setText("BYPASS", juce::dontSendNotification);
        else
            octaveValLbl.setText(juce::String((int)(v * 100)) + "%", juce::dontSendNotification);
    };
    octaveSlider.onDragEnd = [this]() {
        float v = (float)octaveSlider.getValue();
        if (std::abs(v - 0.5) < 0.02)
            octaveSlider.setValue(0.5, juce::sendNotificationSync);
    };
    octaveSlider.onValueChange();
    octaveValLbl.onTextChange = [this]() {
        juce::String text = octaveValLbl.getText().trim().toUpperCase();
        if (text == "BYPASS" || text == "DRY" || text == "CENTER" || text == "CTR")
            octaveSlider.setValue(0.5, juce::sendNotificationSync);
        else
        {
            float val = text.trimCharactersAtEnd("%").getFloatValue();
            val = juce::jlimit(0.0f, 100.0f, val) / 100.0f;
            octaveSlider.setValue((double)val, juce::sendNotificationSync);
        }
    };

    setupSlider(crushSlider, crushValLbl, "guitar_crush", p.getAPVTS(), "crush", crushAtt);
    crushSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    crushSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f, true);
    crushValLbl.setJustificationType(juce::Justification::centred);
    crushSlider.onValueChange = [this]() {
        crushValLbl.setText(juce::String((int)(crushSlider.getValue() * 100)) + "%", juce::dontSendNotification);
    };
    crushSlider.onValueChange();

    setupSlider(unisonSlider, unisonValLbl, "guitar_unison", p.getAPVTS(), "unison", unisonAtt);
    unisonSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    unisonSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
    unisonValLbl.setJustificationType(juce::Justification::centred);
    unisonSlider.onValueChange = [this]() {
        unisonValLbl.setText(juce::String((int)(unisonSlider.getValue() * 100)) + "%", juce::dontSendNotification);
    };
    unisonSlider.onValueChange();

    // --- Dry/Wet footer ---
    dryWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    dryWetSlider.setName("mix_dryWet");
    addAndMakeVisible(dryWetSlider);
    dryWetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "dryWet", dryWetSlider);

    // --- Advanced panel ---
    addAndMakeVisible(advancedPanel);
    advancedPanel.onToggle = [this]() {
        setSize(kWidth, computeWindowHeight());
    };

    // Watch for parameter changes to show "Custom" preset name
    for (auto* id : {"response", "clarity", "numBands", "tone", "spread",
                     "drive", "octave", "crush", "unison", "dryWet"})
        proc.getAPVTS().addParameterListener(id, this);
}

void GuitarVocoderEditor::rebuildPresetBox()
{
    presetBox.clear(juce::dontSendNotification);
    auto names = proc.getPresetManager().getPresetNames();
    int numFactory = proc.getPresetManager().getNumFactoryPresets();

    for (int i = 0; i < names.size(); ++i)
    {
        if (i == numFactory && numFactory > 0)
            presetBox.addSeparator();
        presetBox.addItem(names[i], i + 1);
    }

    int idx = proc.getPresetManager().getCurrentIndex();
    if (idx >= 0)
        presetBox.setSelectedItemIndex(idx, juce::dontSendNotification);
}

GuitarVocoderEditor::~GuitarVocoderEditor()
{
    for (auto* id : {"response", "clarity", "numBands", "tone", "spread",
                     "drive", "octave", "crush", "unison", "dryWet"})
        proc.getAPVTS().removeParameterListener(id, this);
    setLookAndFeel(nullptr);
}

void GuitarVocoderEditor::parameterChanged(const juce::String&, float)
{
    if (loadingPreset) return;
    auto safeThis = juce::Component::SafePointer<GuitarVocoderEditor>(this);
    juce::MessageManager::callAsync([safeThis]() {
        if (safeThis != nullptr)
            safeThis->presetBox.setText("Custom", juce::dontSendNotification);
    });
}


int GuitarVocoderEditor::computeWindowHeight() const
{
    // Header + viz + diag bar + controls + dry/wet + advanced
    int controlsY = kHeaderH + 12 + kVizH + 6 + kDiagBarH + 8;
    int controlsH = 38 + 191;
    int bottomY = controlsY + controlsH + 8;
    int advancedY = bottomY + kBottomBarH;
    return advancedY + advancedPanel.getRequiredHeight();
}

void GuitarVocoderEditor::paint(juce::Graphics& g)
{
    int W = getWidth();
    g.fillAll(AGI::bg);

    // --- Header background ---
    g.setColour(AGI::bg);
    g.fillRect(0, 0, W, kHeaderH);
    g.setColour(AGI::border);
    g.drawLine(0.0f, (float)kHeaderH, (float)W, (float)kHeaderH, 1.0f);

    // Icon
    auto iconRect = juce::Rectangle<float>(18.0f, 8.0f, 32.0f, 32.0f);
    juce::ColourGradient iconGrad(AGI::voice, iconRect.getX(), iconRect.getY(),
                                   AGI::guitar, iconRect.getRight(), iconRect.getBottom(), false);
    g.setGradientFill(iconGrad);
    g.fillRoundedRectangle(iconRect, 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withName("AvenirNextCondensed-Bold").withHeight(17.0f)));
    g.drawText("V", iconRect.toNearestInt(), juce::Justification::centred);

    // Title
    g.setFont(juce::Font(juce::FontOptions().withName("AvenirNextCondensed-Bold").withHeight(17.0f)));
    juce::ColourGradient titleGrad(AGI::voice, 58.0f, 12.0f, AGI::guitar, 220.0f, 12.0f, false);
    g.setGradientFill(titleGrad);
    g.drawText("GUITAR VOCODER", 58, 8, 180, 20, juce::Justification::centredLeft);

    g.setColour(AGI::textDim);
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.drawText(juce::CharPointer_UTF8("VOICE \xe2\x86\x92 GUITAR SHAPING"), 58, 28, 180, 12, juce::Justification::centredLeft);

    // --- Controls section ---
    int controlsY = kHeaderH + 12 + kVizH + 6 + kDiagBarH + 8;
    int leftX = 22;
    int leftW = W / 2 - 22 - 12;
    int rightX = W / 2 + 12;

    // Vertical divider
    g.setColour(AGI::border);
    g.drawLine((float)(W / 2), (float)controlsY, (float)(W / 2), (float)(controlsY + 38 + 191), 1.0f);

    // Vocoder badge (10px top padding)
    int badgeY = controlsY + 10;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(12.0f)));
    auto vocBadge = juce::Rectangle<float>((float)leftX, (float)badgeY, 60.0f, 18.0f);
    g.setColour(AGI::voiceDim);
    g.fillRoundedRectangle(vocBadge, 4.0f);
    g.setColour(AGI::voice);
    g.drawText("VOCODER", vocBadge.toNearestInt(), juce::Justification::centred);
    g.setColour(AGI::border);
    g.drawLine(vocBadge.getRight() + 6.0f, (float)badgeY + 9.0f, (float)(W / 2 - 12), (float)badgeY + 9.0f, 1.0f);

    // Guitar badge
    auto gtrBadge = juce::Rectangle<float>((float)rightX, (float)badgeY, 60.0f, 18.0f);
    g.setColour(AGI::guitarDim);
    g.fillRoundedRectangle(gtrBadge, 4.0f);
    g.setColour(AGI::guitar);
    g.drawText("GUITAR", gtrBadge.toNearestInt(), juce::Justification::centred);
    g.setColour(AGI::border);
    g.drawLine(gtrBadge.getRight() + 6.0f, (float)badgeY + 9.0f, (float)(W - 22), (float)badgeY + 9.0f, 1.0f);

    // --- Left column static labels ---
    int lcY = controlsY + 38;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));

    // "BANDS" label
    g.setColour(AGI::textMid);
    g.drawText("BANDS", leftX, lcY + 4, 60, 16, juce::Justification::centredLeft);

    // Response
    int respY = lcY + 38;
    g.setColour(AGI::textMid);
    g.drawText("RESPONSE", leftX, respY, 100, 12, juce::Justification::centredLeft);

    // Range labels for response
    auto rb = responseSlider.getBounds();
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.setColour(AGI::textDim);
    g.drawText(juce::CharPointer_UTF8("SLOW \xc2\xb7 PAD"), rb.getX(), rb.getBottom() + 1, rb.getWidth() / 2, 10, juce::Justification::centredLeft);
    g.drawText(juce::CharPointer_UTF8("FAST \xc2\xb7 CRISP"), rb.getX() + rb.getWidth() / 2, rb.getBottom() + 1, rb.getWidth() / 2, 10, juce::Justification::centredRight);

    g.setColour(AGI::textDim.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
    g.drawText("Attack + release with smart split envelope", leftX, rb.getBottom() + 11, leftW, 10, juce::Justification::centredLeft);

    // Clarity
    int clarY = rb.getBottom() + 36;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
    g.setColour(AGI::textMid);
    g.drawText("CLARITY", leftX, clarY, 100, 12, juce::Justification::centredLeft);

    auto cb = claritySlider.getBounds();
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.setColour(AGI::textDim);
    g.drawText(juce::CharPointer_UTF8("SMOOTH \xc2\xb7 ROBOT"), cb.getX(), cb.getBottom() + 1, cb.getWidth() / 2, 10, juce::Justification::centredLeft);
    g.drawText(juce::CharPointer_UTF8("CLEAR \xc2\xb7 SPEECH"), cb.getX() + cb.getWidth() / 2, cb.getBottom() + 1, cb.getWidth() / 2, 10, juce::Justification::centredRight);

    g.setColour(AGI::textDim.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
    g.drawText("Noise blend, consonant detection, dynamics", leftX, cb.getBottom() + 11, leftW, 10, juce::Justification::centredLeft);

    // --- Right column static labels ---
    // Tone (bipolar slider)
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
    g.setColour(AGI::textMid);
    auto tb = toneSlider.getBounds();
    g.drawText("TONE", rightX, tb.getY() - 14, 100, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.setColour(AGI::textDim);
    g.drawText(juce::CharPointer_UTF8("\xe2\x97\x82 DARK"), tb.getX() - 42, tb.getY(), 40, tb.getHeight(), juce::Justification::centredRight);
    g.drawText(juce::CharPointer_UTF8("BRIGHT \xe2\x96\xb8"), tb.getRight() + 2, tb.getY(), 50, tb.getHeight(), juce::Justification::centredLeft);
    g.drawText("FLAT", tb.getX() + tb.getWidth() / 2 - 15, tb.getBottom() + 1, 30, 10, juce::Justification::centred);

    // Octave (bipolar slider)
    auto ob = octaveSlider.getBounds();
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
    g.setColour(AGI::textMid);
    g.drawText("OCTAVE", rightX, ob.getY() - 14, 100, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.setColour(AGI::textDim);
    g.drawText(juce::CharPointer_UTF8("\xe2\x97\x82 SUB"), ob.getX() - 42, ob.getY(), 40, ob.getHeight(), juce::Justification::centredRight);
    g.drawText(juce::CharPointer_UTF8("HIGH \xe2\x96\xb8"), ob.getRight() + 2, ob.getY(), 40, ob.getHeight(), juce::Justification::centredLeft);
    g.drawText("BYPASS", ob.getX() + ob.getWidth() / 2 - 20, ob.getBottom() + 1, 40, 10, juce::Justification::centred);

    // Knob row labels (name below value label)
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(11.0f)));
    g.setColour(AGI::textMid);

    auto paintKnobLabel = [&](const juce::Slider& slider, const char* name) {
        int labelY = slider.getBottom() + 16;
        g.drawText(name, slider.getX() - 8, labelY, slider.getWidth() + 16, 14,
                   juce::Justification::centred);
    };

    paintKnobLabel(spreadSlider, "SPREAD");
    paintKnobLabel(driveSlider, "DRIVE");
    paintKnobLabel(crushSlider, "CRUSH");
    paintKnobLabel(unisonSlider, "UNISON");

    // --- Bottom bar ---
    int controlsH = 38 + 191;
    int bottomY = controlsY + controlsH + 8;
    g.setColour(AGI::surface);
    g.fillRect(0, bottomY, W, kBottomBarH);
    g.setColour(AGI::border);
    g.drawLine(0.0f, (float)bottomY, (float)W, (float)bottomY, 1.0f);

    // Dry/Wet labels
    int sliderMargin = 22;
    g.setFont(juce::Font(juce::FontOptions().withName("Menlo").withHeight(13.0f)));
    g.setColour(AGI::textDim);
    g.drawText("DRY", sliderMargin, bottomY + 8, 30, 14, juce::Justification::centredLeft);
    g.setColour(AGI::purple);
    g.drawText("WET", W - sliderMargin - 30, bottomY + 8, 30, 14, juce::Justification::centredRight);

    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.setColour(AGI::textDim);
    g.drawText("Clean Voice", sliderMargin, bottomY + 32, 70, 10, juce::Justification::centredLeft);
    g.drawText("Full Vocoder", W - sliderMargin - 70, bottomY + 32, 70, 10, juce::Justification::centredRight);
}

void GuitarVocoderEditor::resized()
{
    int W = getWidth();

    // --- Header ---
    int presetX = 226;
    int presetY = 12;
    int presetH = 24;
    presetBox.setBounds(presetX, presetY, 164, presetH);
    savePresetBtn.setBounds(presetX + 168, presetY, 44, presetH);

    inputMeter.setBounds(W - 134, 4, 22, 40);
    monitorBtn.setBounds(W - 108, 10, 84, 28);

    // --- Visualizer ---
    visualizer.setBounds(22, kHeaderH + 12, W - 44, kVizH);

    // --- Diagnostics bar ---
    int diagBarY = kHeaderH + 12 + kVizH + 6;
    diagnosticsBar.setBounds(22, diagBarY, W - 44, kDiagBarH);

    // --- Controls area ---
    int controlsY = diagBarY + kDiagBarH + 8;
    int leftX = 22;
    int leftW = W / 2 - 22 - 12;
    int rightX = W / 2 + 12;
    int rightW = W - rightX - 22;

    // Left column: Vocoder (10px above + 10px below badge)
    int lcY = controlsY + 38;

    // Band selector (beside label)
    bandSelector.setBounds(leftX + 50, lcY, leftW - 50, 26);

    // Response slider
    int respY = lcY + 38;
    responseSlider.setBounds(leftX, respY + 14, leftW, 18);
    responseValLbl.setBounds(leftX + leftW - 50, respY, 50, 20);

    // Clarity slider
    auto rb = responseSlider.getBounds();
    int clarY = rb.getBottom() + 36;
    claritySlider.setBounds(leftX, clarY + 14, leftW, 18);
    clarityValLbl.setBounds(leftX + leftW - 50, clarY, 50, 20);

    // Right column: Guitar
    int rcY = controlsY + 38;
    int sliderInset = 44;

    // Tone (bipolar horizontal slider)
    int y = rcY;
    toneSlider.setBounds(rightX + sliderInset, y + 14, rightW - sliderInset * 2, 18);
    toneValLbl.setBounds(rightX + rightW - 50, y, 50, 20);

    // Octave (bipolar horizontal slider)
    y = toneSlider.getBounds().getBottom() + 18;
    octaveSlider.setBounds(rightX + sliderInset, y + 14, rightW - sliderInset * 2, 18);
    octaveValLbl.setBounds(rightX + rightW - 50, y, 50, 20);

    // Knob row: 4 rotary knobs evenly spaced
    int knobY = octaveSlider.getBounds().getBottom() + 24;
    int knobSize = 50;
    int knobSpacing = (rightW - 4 * knobSize) / 3;

    auto placeKnob = [&](juce::Slider& slider, juce::Label& valLbl, int idx) {
        int kx = rightX + idx * (knobSize + knobSpacing);
        slider.setBounds(kx, knobY, knobSize, knobSize);
        valLbl.setBounds(kx - 8, knobY + knobSize + 2, knobSize + 16, 20);
    };

    placeKnob(spreadSlider, spreadValLbl, 0);
    placeKnob(driveSlider, driveValLbl, 1);
    placeKnob(crushSlider, crushValLbl, 2);
    placeKnob(unisonSlider, unisonValLbl, 3);

    // --- Bottom bar ---
    int controlsH = 38 + 191;
    int bottomY = controlsY + controlsH + 8;
    int sliderMargin = 22;
    int sliderX = sliderMargin + 34;
    int sliderEnd = W - sliderMargin - 34;
    dryWetSlider.setBounds(sliderX, bottomY + 12, sliderEnd - sliderX, 16);

    // --- Advanced panel ---
    int advancedY = bottomY + kBottomBarH;
    advancedPanel.setBounds(0, advancedY, W, advancedPanel.getRequiredHeight());
}

