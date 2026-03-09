#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Computer Love theme — Zapp & Roger electro-funk aesthetic
namespace AGI
{
    inline const juce::Colour bg         {0xff08040E};
    inline const juce::Colour panel      {0xff0E0818};
    inline const juce::Colour surface    {0xff160E24};
    inline const juce::Colour surfaceW   {0xff201632};
    inline const juce::Colour border     {0xff302248};
    inline const juce::Colour borderHov  {0xff443060};
    inline const juce::Colour voice      {0xffFF2D8A};
    inline const juce::Colour voiceDim   {0x48FF2D8A};
    inline const juce::Colour guitar     {0xff00BFFF};
    inline const juce::Colour guitarDim  {0x3800BFFF};
    inline const juce::Colour purple     {0xffDA70D6};
    inline const juce::Colour purpleDim  {0x34DA70D6};
    inline const juce::Colour amber      {0xffFF8C00};
    inline const juce::Colour green      {0xff44DD88};
    inline const juce::Colour text       {0xffF0E8F8};
    inline const juce::Colour textMid    {0xffC0B0D4};
    inline const juce::Colour textDim    {0xffA898B8};
    inline const juce::Colour knobTrack  {0xff201632};
}

//==============================================================================
class DiagnosticsBarComponent;

class VisualizerComponent : public juce::Component, private juce::Timer
{
public:
    explicit VisualizerComponent(GuitarVocoderProcessor& p);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void setDiagnosticsBar(DiagnosticsBarComponent* bar) { diagBar = bar; }

private:
    GuitarVocoderProcessor& proc;
    Vocoder& vocoder;
    DiagnosticsBarComponent* diagBar = nullptr;

    std::array<float, Vocoder::kMaxBands> smoothVoice{};
    std::array<float, Vocoder::kMaxBands> smoothGuitar{};

    bool gateOpen = false;
    bool scActive = false;
    int matchPct = 0;
    float regionCoverage[3] = {0, 0, 0};

    // Stale detection
    float prevVoiceRaw[Vocoder::kMaxBands] = {};
    float prevGuitarRaw[Vocoder::kMaxBands] = {};
    int staleFrames = 0;
    static constexpr int kStaleThreshold = 3;

    void computeDiagnostics(int numBands);
    static float toDisplay(float linear);

    static constexpr float kSmoothing = 0.18f;
    static constexpr float kDisplayThreshold = 0.001f;
    static constexpr float kDbRange = 60.0f;
};

//==============================================================================
class DiagnosticsBarComponent : public juce::Component
{
public:
    DiagnosticsBarComponent();
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void updateState(bool gateOpen, bool scActive,
                     int matchPct, const float regionCoverage[3],
                     bool vClip, bool gClip);

private:
    bool gateOpen = false;
    bool scActive = false;
    int matchPct = 0;
    float regionCoverage[3] = {0, 0, 0};

    enum ChipId { cMatch = 0, cLows, cMids, cHighs, cClip, cCount };
    int chipState[cCount] = {};
    double chipLatchTime[cCount] = {};

    static constexpr double kLatchDecaySeconds = 15.0;
    double currentTime = 0.0;

    int hoveredChip = -1;

    int framesWithHiDeficit = 0;
    int framesWithLoDeficit = 0;
    int framesWithMidDeficit = 0;
    int framesWithGoodMatch = 0;
    bool voiceClipped = false;
    bool guitarClipped = false;
    int analysisFrameCount = 0;
    static constexpr int kAnalysisWindow = 60;

    void updateChips();
    juce::Rectangle<int> getChipBounds(int chipIndex) const;
    juce::String getChipLabel(int chipIndex) const;
    juce::String getChipFixText(int chipIndex) const;
    bool isChipGood(int chipIndex) const;
};

//==============================================================================
class InputMeter : public juce::Component, private juce::Timer
{
public:
    InputMeter(GuitarVocoderProcessor& p);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
private:
    GuitarVocoderProcessor& proc;
    float voiceLevel = 0.0f;
    float guitarLevel = 0.0f;
    bool voiceClipHeld = false;
    bool guitarClipHeld = false;
    int voiceClipTimer = 0;
    int guitarClipTimer = 0;
    static constexpr float kDecay = 0.88f;
    static constexpr int kClipHoldFrames = 30;
};

//==============================================================================
class AGILookAndFeel : public juce::LookAndFeel_V4
{
public:
    AGILookAndFeel();
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
};

//==============================================================================
class BandSelector : public juce::Component
{
public:
    BandSelector(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    void updateButtons();
    juce::TextButton btn8{"8"}, btn16{"16"}, btn24{"24"}, btn32{"32"};
    juce::ComboBox hiddenBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

//==============================================================================
class AdvancedPanel : public juce::Component, private juce::Timer
{
public:
    AdvancedPanel(GuitarVocoderProcessor& proc);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void timerCallback() override;
    bool isExpanded() const { return expanded; }
    int getRequiredHeight() const;
    std::function<void()> onToggle;
private:
    GuitarVocoderProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    bool expanded = false;
    float loAtk = 0, loRel = 0, hiAtk = 0, hiRel = 0;
    float noise = 0, uvSens = 0, dynamics = 0, hiBoost = 0;
    float deEssDisplay = 0.0f;
    float response = 0, clarity = 0;
    float noiseFloorDisplay = -60.0f;
    float gateThreshDisplay = -50.0f;

};

//==============================================================================
class GuitarVocoderEditor : public juce::AudioProcessorEditor,
                            public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit GuitarVocoderEditor(GuitarVocoderProcessor&);
    ~GuitarVocoderEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
    GuitarVocoderProcessor& proc;
    AGILookAndFeel agiLnf;

    // Header
    InputMeter inputMeter;
    juce::TextButton monitorBtn{"MONITOR"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monitorAtt;
    juce::TextButton savePresetBtn{"SAVE"};
    juce::ComboBox presetBox;
    void rebuildPresetBox();

    VisualizerComponent visualizer;
    DiagnosticsBarComponent diagnosticsBar;
    BandSelector bandSelector;

    // Vocoder sliders (voice pink)
    juce::Slider responseSlider, claritySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> responseAtt, clarityAtt;
    juce::Label responseValLbl, clarityValLbl;

    // Enrichment sliders (guitar cyan)
    juce::Slider toneSlider, spreadSlider, driveSlider, octaveSlider, crushSlider, unisonSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        toneAtt, spreadAtt, driveAtt, octaveAtt, crushAtt, unisonAtt;
    juce::Label toneValLbl, spreadValLbl, driveValLbl, octaveValLbl, crushValLbl, unisonValLbl;

    // Dry/Wet footer
    juce::Slider dryWetSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAtt;

    // Advanced panel
    AdvancedPanel advancedPanel;

    bool loadingPreset = false;
    uint32_t lastProcessBlockCounter = 0;

    int computeWindowHeight() const;

    static constexpr int kWidth = 850;
    static constexpr int kHeaderH = 48;
    static constexpr int kVizH = 160;
    static constexpr int kDiagBarH = 70;
    static constexpr int kBottomBarH = 46;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarVocoderEditor)
};
