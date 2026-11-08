#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class Q45MonitorEditor : public juce::AudioProcessorEditor
{
public:
    explicit Q45MonitorEditor (Q45MonitorProcessor&);
    ~Q45MonitorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Attachment is declared last so it is destroyed before the slider it points at.
    struct LabelledSlider
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addSlider (LabelledSlider&, const juce::String& paramId, const juce::String& text,
                    const juce::String& suffix);

    Q45MonitorProcessor& proc;

    juce::ToggleButton xfeedButton { "Crossfeed (virtual speakers)" };
    juce::ToggleButton eqButton { "Correction EQ" };
    std::unique_ptr<ButtonAttachment> xfeedAttachment, eqAttachment;

    LabelledSlider angle, bleedTrim, cutoff, preamp, trim;

    juce::Label pasteLabel;
    juce::TextEditor pasteBox;
    juce::TextButton applyButton { "Apply AutoEQ filters" };
    juce::TextButton starterButton { "Reset to Q45 starter" };
    juce::TextButton flatButton { "Flat EQ" };
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Q45MonitorEditor)
};
