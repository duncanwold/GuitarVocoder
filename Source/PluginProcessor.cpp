#include "PluginProcessor.h"
#include "PluginEditor.h"

static const juce::StringArray bandChoices{"8", "16", "24", "32"};

juce::AudioProcessorValueTreeState::ParameterLayout GuitarVocoderProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // === User-facing parameters (9 controls) ===
    params.push_back(std::make_unique<juce::AudioParameterChoice>("numBands", "Bands", bandChoices, 1)); // default index 1 = "16"
    params.push_back(std::make_unique<juce::AudioParameterFloat>("response", "Response",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("clarity", "Clarity",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tone", "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("octave", "Octave",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("crush", "Crush",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("unison", "Unison",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("spread", "Spread",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dryWet", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.85f));


    // === System parameters ===
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("monitorGuitar", "Monitor Guitar", false));

    // === Internal parameters (driven by macros, registered for automation/Advanced panel) ===
    params.push_back(std::make_unique<juce::AudioParameterFloat>("loAttack", "Lo Attack",
        juce::NormalisableRange<float>(0.5f, 50.0f, 0.0f, 0.5f), 7.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("loRelease", "Lo Release",
        juce::NormalisableRange<float>(15.0f, 400.0f, 0.0f, 0.5f), 75.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hiAttack", "Hi Attack",
        juce::NormalisableRange<float>(0.5f, 20.0f, 0.0f, 0.5f), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hiRelease", "Hi Release",
        juce::NormalisableRange<float>(5.0f, 120.0f, 0.0f, 0.5f), 25.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noise", "Noise Blend",
        juce::NormalisableRange<float>(0.0f, 0.45f), 0.22f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("unvoicedSens", "UV Sensitivity",
        juce::NormalisableRange<float>(0.1f, 0.9f), 0.62f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("voiceDynamics", "Voice Dynamics",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.78f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("voiceHighBoost", "Hi Boost",
        juce::NormalisableRange<float>(0.0f, 10.0f), 4.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("deEss", "De-Ess",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));

    return { params.begin(), params.end() };
}

GuitarVocoderProcessor::GuitarVocoderProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Voice (Main)", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Guitar (Sidechain)", juce::AudioChannelSet::mono(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      presetManager(apvts)
{
}

GuitarVocoderProcessor::~GuitarVocoderProcessor() {}

const juce::String GuitarVocoderProcessor::getName() const { return JucePlugin_Name; }
bool GuitarVocoderProcessor::acceptsMidi() const { return false; }
bool GuitarVocoderProcessor::producesMidi() const { return false; }
bool GuitarVocoderProcessor::isMidiEffect() const { return false; }
double GuitarVocoderProcessor::getTailLengthSeconds() const { return 0.0; }
int GuitarVocoderProcessor::getNumPrograms() { return 1; }
int GuitarVocoderProcessor::getCurrentProgram() { return 0; }
void GuitarVocoderProcessor::setCurrentProgram(int) {}
const juce::String GuitarVocoderProcessor::getProgramName(int) { return {}; }
void GuitarVocoderProcessor::changeProgramName(int, const juce::String&) {}

void GuitarVocoderProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    vocoder.prepare(sampleRate, samplesPerBlock);
    enrichment.prepare(sampleRate, samplesPerBlock);
    clarity.prepare(sampleRate, samplesPerBlock);
    voiceGate.prepare(sampleRate);
}

void GuitarVocoderProcessor::releaseResources()
{
    vocoder.reset();
    enrichment.reset();
    clarity.reset();
    voiceGate.reset();
}

bool GuitarVocoderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainIn  = layouts.getMainInputChannelSet();
    auto mainOut = layouts.getMainOutputChannelSet();

    // Main I/O: accept mono or stereo, but input and output must match
    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    // Sidechain (bus 1) must be mono or disabled
    if (layouts.inputBuses.size() > 1)
    {
        auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono())
            return false;
    }

    return true;
}

void GuitarVocoderProcessor::updateMacros()
{
    float response = *apvts.getRawParameterValue("response");
    float clar     = *apvts.getRawParameterValue("clarity");

    // Skip if neither macro has changed (avoids 9x setValueNotifyingHost per block)
    if (std::abs(response - lastResponse) < 1e-6f && std::abs(clar - lastClarity) < 1e-6f)
        return;
    lastResponse = response;
    lastClarity = clar;

    // Response macro → four timing params
    apvts.getParameter("loAttack")->setValueNotifyingHost(
        apvts.getParameter("loAttack")->convertTo0to1(MacroMapping::responseLoAtk(response)));
    apvts.getParameter("loRelease")->setValueNotifyingHost(
        apvts.getParameter("loRelease")->convertTo0to1(MacroMapping::responseLoRel(response)));
    apvts.getParameter("hiAttack")->setValueNotifyingHost(
        apvts.getParameter("hiAttack")->convertTo0to1(MacroMapping::responseHiAtk(response)));
    apvts.getParameter("hiRelease")->setValueNotifyingHost(
        apvts.getParameter("hiRelease")->convertTo0to1(MacroMapping::responseHiRel(response)));

    // Clarity macro → four voice params
    apvts.getParameter("noise")->setValueNotifyingHost(
        apvts.getParameter("noise")->convertTo0to1(MacroMapping::clarityNoise(clar)));
    apvts.getParameter("unvoicedSens")->setValueNotifyingHost(
        apvts.getParameter("unvoicedSens")->convertTo0to1(MacroMapping::clarityUvSens(clar)));
    apvts.getParameter("voiceDynamics")->setValueNotifyingHost(
        apvts.getParameter("voiceDynamics")->convertTo0to1(MacroMapping::clarityDynamics(clar)));
    apvts.getParameter("voiceHighBoost")->setValueNotifyingHost(
        apvts.getParameter("voiceHighBoost")->convertTo0to1(MacroMapping::clarityHiBoost(clar)));
    apvts.getParameter("deEss")->setValueNotifyingHost(
        apvts.getParameter("deEss")->convertTo0to1(MacroMapping::clarityDeEss(clar)));
}

void GuitarVocoderProcessor::updateDSPFromParams()
{
    // Run macro mapping first (Response/Clarity → internal params)
    updateMacros();

    // --- Vocoder ---
    static const int bandValues[] = { 8, 16, 24, 32 };
    int bandIdx = ((juce::AudioParameterChoice*)apvts.getParameter("numBands"))->getIndex();
    vocoder.setNumBands(bandValues[bandIdx]);
    // Timing from internal macro-driven params
    float loAtk = *apvts.getRawParameterValue("loAttack");
    float loRel = *apvts.getRawParameterValue("loRelease");
    float hiAtk = *apvts.getRawParameterValue("hiAttack");
    float hiRel = *apvts.getRawParameterValue("hiRelease");

    vocoder.setAttack(loAtk);
    vocoder.setRelease(loRel);
    vocoder.setHiAttack(hiAtk);
    vocoder.setHiRelease(hiRel);
    vocoder.applyEnvelopeSettings();

    vocoder.setNoiseAmount(*apvts.getRawParameterValue("noise"));
    vocoder.setDryWet(*apvts.getRawParameterValue("dryWet"));
    vocoder.setVoiceDynamics(*apvts.getRawParameterValue("voiceDynamics"));

    // De-esser
    vocoder.setDeEssAmount(*apvts.getRawParameterValue("deEss"));

    // --- Clarity ---
    clarity.setLowCutFreq(80.0f);  // hardcoded
    clarity.setHighBoost(*apvts.getRawParameterValue("voiceHighBoost"));
    clarity.setSensitivity(*apvts.getRawParameterValue("unvoicedSens"));

    // --- Voice gate ---
    voiceGate.setRelease(loRel);  // gate release tracks lo release
    // Hardcoded: +7dB above noise floor (more permissive than the old +10dB default)
    voiceGate.setOffsetDb(7.0f);

    // --- Enrichment ---
    enrichment.setToneAmount(*apvts.getRawParameterValue("tone"));
    enrichment.setDriveAmount(*apvts.getRawParameterValue("drive"));
    enrichment.setOctaveBipolar(*apvts.getRawParameterValue("octave"));
    enrichment.setCrushAmount(*apvts.getRawParameterValue("crush"));
    enrichment.setUnisonAmount(*apvts.getRawParameterValue("unison"));
    enrichment.setSpreadAmount(*apvts.getRawParameterValue("spread"));
}

void GuitarVocoderProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    updateDSPFromParams();

    // Re-trigger onset ramp on preset load to prevent click from parameter jumps
    if (presetManager.presetJustLoaded.exchange(false, std::memory_order_relaxed))
        vocoder.retriggerOnsetRamp();

    bool bypassed = apvts.getRawParameterValue("bypass")->load() >= 0.5f;
    bool monitor  = apvts.getRawParameterValue("monitorGuitar")->load() >= 0.5f;

    auto numSamples = buffer.getNumSamples();
    auto mainIO = getBusBuffer(buffer, true, 0);

    // Voice from channel 0 of main input
    auto* voiceData = mainIO.getReadPointer(0);

    // Guitar from channel 0 of sidechain
    const float* guitarData = nullptr;
    bool scIsActive = false;
    auto sidechain = getBusBuffer(buffer, true, 1);
    if (sidechain.getNumChannels() > 0)
    {
        const float* scData = sidechain.getReadPointer(0);

        // Logic feeds main input into sidechain when no source is assigned.
        // Detect this by checking if the pointers are identical (same buffer)
        // or if the first N samples match exactly (copied buffer).
        if (scData != voiceData)
        {
            bool identical = true;
            int checkSamples = juce::jmin(numSamples, 32);
            for (int i = 0; i < checkSamples; ++i)
            {
                if (scData[i] != voiceData[i])
                {
                    identical = false;
                    break;
                }
            }

            if (!identical)
            {
                guitarData = scData;
                scIsActive = true;
            }
        }
        // If pointers match or content is identical, guitarData stays nullptr
    }

    // Mono processing into channel 0
    auto* outputData = mainIO.getWritePointer(0);

    // Input level metering (pre-processing)
    float vPeak = 0.0f, gPeak = 0.0f;
    bool vClip = false, gClip = false;

    bool prevGateState = voiceGate.getGateGain() > 0.5f;
    int transitions = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        float voice = voiceData[i];
        float guitar = (guitarData != nullptr) ? guitarData[i] : 0.0f;

        // Track peaks for meters
        float va = std::abs(voice);
        float ga = std::abs(guitar);
        if (va > vPeak) vPeak = va;
        if (ga > gPeak) gPeak = ga;
        if (va >= 1.0f) vClip = true;
        if (ga >= 1.0f) gClip = true;

        if (bypassed)
        {
            outputData[i] = voice;
            continue;
        }

        // Voice gate (auto-calibrating)
        float gatedVoice = voiceGate.process(voice);

        bool currentGateState = voiceGate.getGateGain() > 0.5f;
        if (currentGateState != prevGateState) transitions++;
        prevGateState = currentGateState;

        float enrichedGuitar = enrichment.process(guitar);

        float preEQVoice = clarity.processVoice(gatedVoice);

        if (scIsActive)
        {
            float vocoded = vocoder.process(gatedVoice, preEQVoice, enrichedGuitar,
                                            clarity.getUnvoicedAmount());
            outputData[i] = monitor ? enrichedGuitar : vocoded;
        }
        else
        {
            vocoder.process(gatedVoice, preEQVoice, 0.0f, 0.0f);
            outputData[i] = voice;
        }
    }

    // Update atomic meters
    voicePeakLevel.store(vPeak, std::memory_order_relaxed);
    guitarPeakLevel.store(gPeak, std::memory_order_relaxed);
    if (vClip) voiceClip.store(true, std::memory_order_relaxed);
    if (gClip) guitarClip.store(true, std::memory_order_relaxed);
    voiceGateGain.store(voiceGate.getGateGain(), std::memory_order_relaxed);
    sideChainActive.store(scIsActive, std::memory_order_relaxed);
    processBlockCounter.fetch_add(1, std::memory_order_relaxed);
    noiseFloorDb.store(voiceGate.getNoiseFloorDb(), std::memory_order_relaxed);
    gateThresholdDb.store(voiceGate.getActiveThresholdDb(), std::memory_order_relaxed);

    // Normalize transitions to per-second rate
    float transPerSec = (numSamples > 0)
        ? (float)transitions * (float)getSampleRate() / (float)numSamples
        : 0.0f;
    gateTransitionsPerSecond.store((int)transPerSec, std::memory_order_relaxed);

    // Copy mono result to all remaining output channels (stereo tracks)
    for (int ch = 1; ch < mainIO.getNumChannels(); ++ch)
        mainIO.copyFrom(ch, 0, mainIO, 0, 0, numSamples);
}

juce::AudioProcessorEditor* GuitarVocoderProcessor::createEditor()
{
    return new GuitarVocoderEditor(*this);
}

bool GuitarVocoderProcessor::hasEditor() const { return true; }

void GuitarVocoderProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GuitarVocoderProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        auto presetName = tree.getProperty("currentPreset", "").toString();
        apvts.replaceState(tree);
        presetManager.setCurrentPresetByName(presetName);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarVocoderProcessor();
}
