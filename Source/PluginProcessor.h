#pragma once
#include <JuceHeader.h>
#include "DSP/Vocoder.h"
#include "DSP/GuitarEnrichment.h"
#include "DSP/ClarityEngine.h"
#include "DSP/VoiceGate.h"
#include "MacroMapping.h"
#include "PresetManager.h"

class GuitarVocoderProcessor : public juce::AudioProcessor
{
public:
    GuitarVocoderProcessor();
    ~GuitarVocoderProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    Vocoder& getVocoder() { return vocoder; }
    GuitarEnrichment& getEnrichment() { return enrichment; }
    ClarityEngine& getClarity() { return clarity; }
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    PresetManager& getPresetManager() { return presetManager; }

    // Input level meters (read by UI at 30fps)
    std::atomic<float> voicePeakLevel{0.0f};
    std::atomic<float> guitarPeakLevel{0.0f};
    std::atomic<bool> voiceClip{false};
    std::atomic<bool> guitarClip{false};
    std::atomic<float> voiceGateGain{0.0f};
    std::atomic<bool> sideChainActive{false};
    std::atomic<uint32_t> processBlockCounter{0};
    std::atomic<float> noiseFloorDb{-60.0f};
    std::atomic<float> gateThresholdDb{-50.0f};
    std::atomic<int> gateTransitionsPerSecond{0};

    void updateMacros();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateDSPFromParams();

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;
    Vocoder vocoder;
    GuitarEnrichment enrichment;
    ClarityEngine clarity;
    VoiceGate voiceGate;

    float lastResponse = -1.0f;
    float lastClarity = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarVocoderProcessor)
};
