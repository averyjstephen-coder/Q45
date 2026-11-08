#pragma once

#include <array>
#include <atomic>
#include <string>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "AutoEqParser.h"
#include "CrossfeedMath.h"

class Q45MonitorProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 8;

    Q45MonitorProcessor();
    ~Q45MonitorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Q45 Monitor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Called from the editor (message thread).
    juce::String applyAutoEq (const std::string& text);
    void loadQ45Starter();
    void flatEq();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String bandId (int band, const char* what);
    void setParam (const juce::String& id, float realValue);
    void setBand (int band, int type, float freq, float gainDb, float q);
    void updateEqCoefficients();

    // Raw parameter pointers (read on the audio thread)
    std::atomic<float>* angleParam = nullptr;
    std::atomic<float>* bleedTrimParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* xfeedOnParam = nullptr;
    std::atomic<float>* eqOnParam = nullptr;
    std::atomic<float>* preampParam = nullptr;
    std::atomic<float>* trimParam = nullptr;
    std::array<std::atomic<float>*, numBands> bType {}, bFreq {}, bGain {}, bQ {};

    // Crossfeed
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;
    Delay delayL { 512 }, delayR { 512 };
    juce::dsp::IIR::Filter<float> lpL, lpR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmooth, bleedSmooth, levelSmooth;
    float cutoffCache = -1.0f;
    bool wasXfeedOn = true;

    // Correction EQ
    struct BandCache
    {
        int type = -1;
        float freq = 0.f, gain = 0.f, q = 0.f;
        bool operator== (const BandCache& o) const
        {
            return type == o.type && freq == o.freq && gain == o.gain && q == o.q;
        }
    };
    std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, numBands> eq;
    std::array<BandCache, numBands> eqCache;
    std::array<bool, numBands> eqActive {};

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Q45MonitorProcessor)
};
