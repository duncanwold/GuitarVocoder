#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    initFactoryPresets();
    scanUserPresets();
    loadPreset(0);  // Apply Talk Box defaults on first launch
}

void PresetManager::initFactoryPresets()
{
    // numBands is a choice index: 0=8, 1=16, 2=24, 3=32.
    // All other values are raw APVTS floats (0.0–1.0).
    // User-facing params: numBands, response, clarity, tone, spread, drive, octave, crush, unison, dryWet

    // --- CLASSIC VOCODER ---
    factoryData.push_back({"Talk Box", {
        {"numBands", 1}, {"response", 0.50f}, {"clarity", 0.65f},
        {"tone", 0.50f}, {"spread", 0.00f}, {"drive", 0.15f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 1.00f}
    }});
    factoryData.push_back({"Robot", {
        {"numBands", 3}, {"response", 0.80f}, {"clarity", 0.40f},
        {"tone", 0.45f}, {"spread", 0.30f}, {"drive", 0.00f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 1.00f}
    }});
    factoryData.push_back({"Choir Pad", {
        {"numBands", 2}, {"response", 0.15f}, {"clarity", 0.25f},
        {"tone", 0.55f}, {"spread", 0.40f}, {"drive", 0.00f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.65f}, {"dryWet", 0.90f}
    }});
    factoryData.push_back({"Radio DJ", {
        {"numBands", 2}, {"response", 0.70f}, {"clarity", 0.85f},
        {"tone", 0.65f}, {"spread", 0.25f}, {"drive", 0.10f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 0.90f}
    }});

    // --- BASS & LOW END ---
    factoryData.push_back({"Vocoder Bass", {
        {"numBands", 0}, {"response", 0.60f}, {"clarity", 0.35f},
        {"tone", 0.30f}, {"spread", 0.00f}, {"drive", 0.50f},
        {"octave", 0.15f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 1.00f}
    }});
    factoryData.push_back({"Sub Growl", {
        {"numBands", 0}, {"response", 0.90f}, {"clarity", 0.50f},
        {"tone", 0.25f}, {"spread", 0.00f}, {"drive", 0.75f},
        {"octave", 0.10f}, {"crush", 0.15f}, {"unison", 0.00f}, {"dryWet", 1.00f}
    }});

    // --- FUNK & RHYTHM ---
    factoryData.push_back({"Business Funk", {
        {"numBands", 1}, {"response", 0.85f}, {"clarity", 0.75f},
        {"tone", 0.60f}, {"spread", 0.15f}, {"drive", 0.35f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 0.85f}
    }});
    factoryData.push_back({"Zapp Bounce", {
        {"numBands", 1}, {"response", 0.75f}, {"clarity", 0.80f},
        {"tone", 0.70f}, {"spread", 0.20f}, {"drive", 0.20f},
        {"octave", 0.55f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 0.90f}
    }});
    factoryData.push_back({"Clavinet Chop", {
        {"numBands", 1}, {"response", 0.95f}, {"clarity", 0.70f},
        {"tone", 0.55f}, {"spread", 0.10f}, {"drive", 0.25f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.00f}, {"dryWet", 0.80f}
    }});

    // --- TEXTURE & AMBIENT ---
    factoryData.push_back({"Glass Cathedral", {
        {"numBands", 2}, {"response", 0.10f}, {"clarity", 0.20f},
        {"tone", 0.60f}, {"spread", 0.50f}, {"drive", 0.00f},
        {"octave", 0.75f}, {"crush", 0.00f}, {"unison", 0.80f}, {"dryWet", 0.95f}
    }});
    factoryData.push_back({"Frozen Vowel", {
        {"numBands", 2}, {"response", 0.05f}, {"clarity", 0.15f},
        {"tone", 0.50f}, {"spread", 0.60f}, {"drive", 0.00f},
        {"octave", 0.50f}, {"crush", 0.00f}, {"unison", 0.50f}, {"dryWet", 0.95f}
    }});
    factoryData.push_back({"Whisper Circuit", {
        {"numBands", 1}, {"response", 0.40f}, {"clarity", 0.90f},
        {"tone", 0.50f}, {"spread", 0.00f}, {"drive", 0.00f},
        {"octave", 0.50f}, {"crush", 0.35f}, {"unison", 0.00f}, {"dryWet", 0.75f}
    }});

    // --- AGGRESSIVE & WEIRD ---
    factoryData.push_back({"Broken Radio", {
        {"numBands", 0}, {"response", 0.95f}, {"clarity", 0.80f},
        {"tone", 0.50f}, {"spread", 0.00f}, {"drive", 0.85f},
        {"octave", 0.50f}, {"crush", 0.55f}, {"unison", 0.00f}, {"dryWet", 1.00f}
    }});
    factoryData.push_back({"Demon Voice", {
        {"numBands", 0}, {"response", 0.70f}, {"clarity", 0.20f},
        {"tone", 0.35f}, {"spread", 0.00f}, {"drive", 0.80f},
        {"octave", 0.08f}, {"crush", 0.20f}, {"unison", 0.00f}, {"dryWet", 1.00f}
    }});
    factoryData.push_back({"Swarm", {
        {"numBands", 2}, {"response", 0.30f}, {"clarity", 0.45f},
        {"tone", 0.55f}, {"spread", 0.70f}, {"drive", 0.20f},
        {"octave", 0.70f}, {"crush", 0.10f}, {"unison", 0.90f}, {"dryWet", 0.95f}
    }});

    numFactory = (int)factoryData.size();
}

void PresetManager::scanUserPresets()
{
    presets.clear();
    for (auto& fd : factoryData)
        presets.push_back({fd.name, true, {}});

    auto dir = getUserPresetDir();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& f : files)
            presets.push_back({f.getFileNameWithoutExtension(), false, f});
    }
}

juce::File PresetManager::getUserPresetDir() const
{
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Audio/Presets/YachtPockets/Guitar Vocoder");
}

juce::String PresetManager::getPresetName(int index) const
{
    if (index >= 0 && index < (int)presets.size())
        return presets[(size_t)index].name;
    return {};
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : presets)
        names.add(p.name);
    return names;
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (currentIndex >= 0 && currentIndex < (int)presets.size())
        return presets[(size_t)currentIndex].name;
    return {};
}

void PresetManager::setCurrentPresetByName(const juce::String& name)
{
    if (name.isEmpty()) { currentIndex = -1; return; }
    for (int i = 0; i < (int)presets.size(); ++i)
    {
        if (presets[(size_t)i].name == name)
        {
            currentIndex = i;
            return;
        }
    }
    currentIndex = -1;
}

void PresetManager::setParam(const juce::String& id, float rawValue)
{
    if (auto* param = apvts.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(rawValue));
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= (int)presets.size()) return;
    currentIndex = index;

    if (presets[(size_t)index].isFactory)
    {
        auto& params = factoryData[(size_t)index].params;
        for (auto& [id, value] : params)
            setParam(id, value);
    }
    else
    {
        auto& file = presets[(size_t)index].file;
        if (auto xml = juce::XmlDocument::parse(file))
        {
            if (xml->hasTagName(apvts.state.getType()))
                apvts.replaceState(juce::ValueTree::fromXml(*xml));
        }
    }

    presetJustLoaded.store(true, std::memory_order_relaxed);
}

void PresetManager::loadNext()
{
    if (presets.empty()) return;
    int next = (currentIndex + 1) % (int)presets.size();
    loadPreset(next);
}

void PresetManager::loadPrevious()
{
    if (presets.empty()) return;
    int prev = currentIndex <= 0 ? (int)presets.size() - 1 : currentIndex - 1;
    loadPreset(prev);
}

void PresetManager::saveUserPreset(const juce::String& name)
{
    auto dir = getUserPresetDir();
    dir.createDirectory();

    auto file = dir.getChildFile(name + ".xml");
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        xml->writeTo(file);

    refreshUserPresets();

    for (int i = 0; i < (int)presets.size(); ++i)
    {
        if (presets[(size_t)i].name == name)
        {
            currentIndex = i;
            return;
        }
    }
}

void PresetManager::refreshUserPresets()
{
    scanUserPresets();
}
