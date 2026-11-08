#include "PluginProcessor.h"
#include "PluginEditor.h"

using Param = juce::AudioProcessorValueTreeState;

juce::String Q45MonitorProcessor::bandId (int band, const char* what)
{
    return "b" + juce::String (band) + what;
}

Param::ParameterLayout Q45MonitorProcessor::createLayout()
{
    Param::ParameterLayout layout;

    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    layout.add (std::make_unique<juce::AudioParameterBool> (pid ("xfeedOn"), "Crossfeed", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("angle"), "Speaker Angle", juce::NormalisableRange<float> (30.f, 120.f, 1.f), 90.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("bleedTrim"), "Bleed Trim", juce::NormalisableRange<float> (-3.f, 3.f, 0.1f), 0.f));

    juce::NormalisableRange<float> cutoffRange (300.f, 2000.f, 1.f);
    cutoffRange.setSkewForCentre (800.f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (pid ("cutoff"), "Head Shadow Cutoff", cutoffRange, 800.f));

    layout.add (std::make_unique<juce::AudioParameterBool> (pid ("eqOn"), "Correction EQ", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("preamp"), "Preamp", juce::NormalisableRange<float> (-18.f, 6.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("trim"), "Output Trim", juce::NormalisableRange<float> (-12.f, 6.f, 0.1f), 0.f));

    // Default band layout: band 1 is the Q45 starter low shelf, the rest are off.
    const float defaultFreqs[numBands] = { 120.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f };

    for (int i = 0; i < numBands; ++i)
    {
        const juce::String n (i + 1);
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            pid (bandId (i, "Type")), "Band " + n + " Type",
            juce::StringArray { "Off", "Peak", "Low Shelf", "High Shelf" }, i == 0 ? 2 : 0));

        juce::NormalisableRange<float> fr (20.f, 20000.f, 0.1f);
        fr.setSkewForCentre (1000.f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            pid (bandId (i, "Freq")), "Band " + n + " Freq", fr, defaultFreqs[i]));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            pid (bandId (i, "Gain")), "Band " + n + " Gain",
            juce::NormalisableRange<float> (-18.f, 18.f, 0.1f), i == 0 ? -3.f : 0.f));

        juce::NormalisableRange<float> qr (0.1f, 10.f, 0.01f);
        qr.setSkewForCentre (1.f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            pid (bandId (i, "Q")), "Band " + n + " Q", qr, 0.7f));
    }
    return layout;
}

Q45MonitorProcessor::Q45MonitorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    angleParam = apvts.getRawParameterValue ("angle");
    bleedTrimParam = apvts.getRawParameterValue ("bleedTrim");
    cutoffParam = apvts.getRawParameterValue ("cutoff");
    xfeedOnParam = apvts.getRawParameterValue ("xfeedOn");
    eqOnParam = apvts.getRawParameterValue ("eqOn");
    preampParam = apvts.getRawParameterValue ("preamp");
    trimParam = apvts.getRawParameterValue ("trim");

    for (int i = 0; i < numBands; ++i)
    {
        bType[(size_t) i] = apvts.getRawParameterValue (bandId (i, "Type"));
        bFreq[(size_t) i] = apvts.getRawParameterValue (bandId (i, "Freq"));
        bGain[(size_t) i] = apvts.getRawParameterValue (bandId (i, "Gain"));
        bQ[(size_t) i] = apvts.getRawParameterValue (bandId (i, "Q"));
    }
}

bool Q45MonitorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Q45MonitorProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 1 };
    const int maxDelay = (int) std::ceil (0.005 * sr) + 4;
    delayL.setMaximumDelayInSamples (maxDelay);
    delayR.setMaximumDelayInSamples (maxDelay);
    delayL.prepare (spec);
    delayR.prepare (spec);
    delayL.reset();
    delayR.reset();

    lpL.reset();
    lpR.reset();
    cutoffCache = -1.0f;

    for (auto& band : eq)
        for (auto& f : band)
            f.reset();
    for (auto& c : eqCache)
        c = {};

    const float angle = angleParam->load();
    delaySmooth.reset (sr, 0.05);
    delaySmooth.setCurrentAndTargetValue ((float) (q45::itdSeconds (angle) * sr));
    bleedSmooth.reset (sr, 0.05);
    bleedSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((float) q45::bleedDb (angle)));
    levelSmooth.reset (sr, 0.05);
    levelSmooth.setCurrentAndTargetValue (1.0f);
    wasXfeedOn = true;
}

void Q45MonitorProcessor::updateEqCoefficients()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        BandCache now;
        now.type = (int) bType[b]->load();
        now.freq = bFreq[b]->load();
        now.gain = bGain[b]->load();
        now.q = bQ[b]->load();

        if (now == eqCache[b])
            continue;
        eqCache[b] = now;

        const bool active = now.type != 0 && std::abs (now.gain) > 0.01f;
        eqActive[b] = active;

        if (! active)
        {
            for (auto& f : eq[b])
                f.reset();
            continue;
        }

        const float freq = juce::jlimit (20.0f, (float) (sampleRate * 0.49), now.freq);
        const float gain = juce::Decibels::decibelsToGain (now.gain);
        Coeffs::Ptr c;
        if (now.type == 1)       c = Coeffs::makePeakFilter (sampleRate, freq, now.q, gain);
        else if (now.type == 2)  c = Coeffs::makeLowShelf (sampleRate, freq, now.q, gain);
        else                     c = Coeffs::makeHighShelf (sampleRate, freq, now.q, gain);

        for (auto& f : eq[b])
            f.coefficients = c;
    }
}

void Q45MonitorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const int numSamples = buffer.getNumSamples();
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);

    // --- Crossfeed (virtual speakers) ---
    const bool xfeedOn = xfeedOnParam->load() > 0.5f;

    if (xfeedOn)
    {
        const float angle = angleParam->load();
        const float bleedDb = juce::jlimit (-12.0f, 0.0f, (float) q45::bleedDb (angle) + bleedTrimParam->load());
        delaySmooth.setTargetValue ((float) (q45::itdSeconds (angle) * sampleRate));
        bleedSmooth.setTargetValue (juce::Decibels::decibelsToGain (bleedDb));

        const float cutoff = cutoffParam->load();
        if (cutoff != cutoffCache)
        {
            cutoffCache = cutoff;
            lpL.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sampleRate, cutoff);
            lpR.coefficients = lpL.coefficients;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = l[i], inR = r[i];
            const float d = delaySmooth.getNextValue();
            const float g = bleedSmooth.getNextValue();

            delayL.pushSample (0, inL);
            delayR.pushSample (0, inR);
            const float dl = delayL.popSample (0, d);
            const float dr = delayR.popSample (0, d);

            // Right channel reaches the left ear (delayed, head-shadowed), and vice versa.
            const float toL = lpR.processSample (dr) * g;
            const float toR = lpL.processSample (dl) * g;

            // Keep centre-panned low end at unity so the monitor level stays comparable.
            const float norm = 1.0f / (1.0f + g);
            l[i] = (inL + toL) * norm;
            r[i] = (inR + toR) * norm;
        }
    }
    else if (wasXfeedOn)
    {
        delayL.reset();
        delayR.reset();
        lpL.reset();
        lpR.reset();
    }
    wasXfeedOn = xfeedOn;

    // --- Correction EQ + preamp + trim ---
    const bool eqOn = eqOnParam->load() > 0.5f;
    if (eqOn)
        updateEqCoefficients();

    levelSmooth.setTargetValue (juce::Decibels::decibelsToGain (
        (eqOn ? preampParam->load() : 0.0f) + trimParam->load()));

    float* chans[2] = { l, r };
    for (int i = 0; i < numSamples; ++i)
    {
        const float level = levelSmooth.getNextValue();
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float x = chans[ch][i];
            if (eqOn)
                for (size_t b = 0; b < (size_t) numBands; ++b)
                    if (eqActive[b])
                        x = eq[b][ch].processSample (x);
            chans[ch][i] = x * level;
        }
    }
}

// ---------------------------------------------------------------- presets / import

void Q45MonitorProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void Q45MonitorProcessor::setBand (int band, int type, float freq, float gainDb, float q)
{
    setParam (bandId (band, "Type"), (float) type);
    setParam (bandId (band, "Freq"), freq);
    setParam (bandId (band, "Gain"), gainDb);
    setParam (bandId (band, "Q"), q);
}

void Q45MonitorProcessor::flatEq()
{
    for (int i = 0; i < numBands; ++i)
        setBand (i, 0, i == 0 ? 120.f : 1000.f, 0.f, 0.7f);
    setParam ("preamp", 0.f);
}

// ROUGH placeholder: RTINGS lists the Space Q45's bass as about 3 dB emphasised, so the
// starter is a gentle low-shelf cut. Replace it with real AutoEQ filters (paste + Apply).
void Q45MonitorProcessor::loadQ45Starter()
{
    flatEq();
    setBand (0, 2, 120.f, -3.f, 0.7f);
}

juce::String Q45MonitorProcessor::applyAutoEq (const std::string& text)
{
    const auto parsed = q45::parseAutoEq (text, numBands);

    if (parsed.bands.empty() && ! parsed.hasPreamp)
        return "No filters found. Paste the contents of the AutoEQ ParametricEQ.txt, "
               "lines like: Filter 1: ON PK Fc 105 Hz Gain 2.5 dB Q 0.70";

    for (int i = 0; i < numBands; ++i)
    {
        if (i < (int) parsed.bands.size())
        {
            const auto& b = parsed.bands[(size_t) i];
            setBand (i, (int) b.type, (float) b.freq, (float) b.gainDb, (float) b.q);
        }
        else
        {
            setBand (i, 0, 1000.f, 0.f, 0.7f);
        }
    }
    setParam ("preamp", parsed.hasPreamp ? (float) parsed.preampDb : 0.f);

    auto msg = "Applied " + juce::String ((int) parsed.bands.size()) + " filter(s), preamp "
             + juce::String (parsed.hasPreamp ? parsed.preampDb : 0.0, 1) + " dB.";
    if (parsed.skippedOverflow > 0)
        msg << " " << parsed.skippedOverflow << " extra filter(s) did not fit in " << numBands << " bands.";
    if (parsed.skippedUnsupported > 0)
        msg << " " << parsed.skippedUnsupported << " unsupported filter(s) skipped (only PK / LS / HS).";
    return msg;
}

// ---------------------------------------------------------------- state / editor

void Q45MonitorProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void Q45MonitorProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Q45MonitorProcessor::createEditor()
{
    return new Q45MonitorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Q45MonitorProcessor();
}
