#include "PluginEditor.h"

Q45MonitorEditor::Q45MonitorEditor (Q45MonitorProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (560, 520);

    addAndMakeVisible (xfeedButton);
    addAndMakeVisible (eqButton);
    xfeedAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "xfeedOn", xfeedButton);
    eqAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "eqOn", eqButton);

    addSlider (angle, "angle", "Speaker angle", " deg");
    addSlider (bleedTrim, "bleedTrim", "Bleed trim", " dB");
    addSlider (cutoff, "cutoff", "Head shadow", " Hz");
    addSlider (preamp, "preamp", "Preamp", " dB");
    addSlider (trim, "trim", "Output trim", " dB");

    pasteLabel.setText ("Paste AutoEQ ParametricEQ.txt (up to 8 PK / LS / HS filters):",
                        juce::dontSendNotification);
    pasteLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (pasteLabel);

    pasteBox.setMultiLine (true, false);
    pasteBox.setReturnKeyStartsNewLine (true);
    pasteBox.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    pasteBox.setTextToShowWhenEmpty ("Preamp: -6.1 dB\nFilter 1: ON PK Fc 105 Hz Gain 2.5 dB Q 0.70\n...",
                                     juce::Colours::grey);
    addAndMakeVisible (pasteBox);

    addAndMakeVisible (applyButton);
    addAndMakeVisible (starterButton);
    addAndMakeVisible (flatButton);

    applyButton.onClick = [this]
    {
        status.setText (proc.applyAutoEq (pasteBox.getText().toStdString()), juce::dontSendNotification);
    };
    starterButton.onClick = [this]
    {
        proc.loadQ45Starter();
        status.setText ("Q45 starter loaded (rough: -3 dB low shelf at 120 Hz).", juce::dontSendNotification);
    };
    flatButton.onClick = [this]
    {
        proc.flatEq();
        status.setText ("Correction EQ set to flat.", juce::dontSendNotification);
    };

    status.setText ("Default: 90 deg crossfeed + rough Q45 starter shelf. "
                    "Paste real AutoEQ filters above to replace the starter.",
                    juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    status.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (status);
}

void Q45MonitorEditor::addSlider (LabelledSlider& s, const juce::String& paramId,
                                  const juce::String& text, const juce::String& suffix)
{
    s.label.setText (text, juce::dontSendNotification);
    s.label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (s.label);

    s.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    s.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 22);
    s.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (s.slider);

    s.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, s.slider);
}

void Q45MonitorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15181d));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Q45 Monitor", 20, 12, 300, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Headphone correction + speaker-angle crossfeed. Monitoring only: bypass when bouncing.",
                20, 42, 520, 18, juce::Justification::centredLeft);
}

void Q45MonitorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (52);

    auto toggles = area.removeFromTop (28);
    xfeedButton.setBounds (toggles.removeFromLeft (260));
    eqButton.setBounds (toggles);
    area.removeFromTop (6);

    auto row = [&] (LabelledSlider& s)
    {
        auto r = area.removeFromTop (28);
        s.label.setBounds (r.removeFromLeft (120));
        s.slider.setBounds (r);
    };
    row (angle);
    row (bleedTrim);
    row (cutoff);
    row (preamp);
    row (trim);

    area.removeFromTop (10);
    pasteLabel.setBounds (area.removeFromTop (22));

    auto buttons = area.removeFromBottom (30);
    applyButton.setBounds (buttons.removeFromLeft (170));
    buttons.removeFromLeft (8);
    starterButton.setBounds (buttons.removeFromLeft (170));
    buttons.removeFromLeft (8);
    flatButton.setBounds (buttons.removeFromLeft (90));
    area.removeFromBottom (6);

    status.setBounds (area.removeFromBottom (40));
    area.removeFromBottom (6);
    pasteBox.setBounds (area);
}
