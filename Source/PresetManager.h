#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    int getNumPresets() const { return (int)presets.size(); }
    juce::String getPresetName(int index) const;
    juce::StringArray getPresetNames() const;
    int getCurrentIndex() const { return currentIndex; }
    int getNumFactoryPresets() const { return numFactory; }
    bool isFactory(int index) const { return index >= 0 && index < numFactory; }

    void loadPreset(int index);
    void loadNext();
    void loadPrevious();

    void saveUserPreset(const juce::String& name);
    void refreshUserPresets();

    std::atomic<bool> presetJustLoaded{false};

    // State save/recall
    juce::String getCurrentPresetName() const;
    void setCurrentPresetByName(const juce::String& name);

private:
    void initFactoryPresets();
    void scanUserPresets();
    void setParam(const juce::String& id, float rawValue);
    juce::File getUserPresetDir() const;

    using ParamList = std::vector<std::pair<juce::String, float>>;

    struct FactoryData {
        const char* name;
        ParamList params;
    };

    struct Preset {
        juce::String name;
        bool isFactory;
        juce::File file;
    };

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<FactoryData> factoryData;
    std::vector<Preset> presets;
    int currentIndex = -1;
    int numFactory = 0;
};
