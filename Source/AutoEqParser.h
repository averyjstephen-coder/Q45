#pragma once
// Plain C++ (no JUCE): parses AutoEQ / EqualizerAPO "ParametricEQ.txt" text.
//
//   Preamp: -6.1 dB
//   Filter 1: ON PK Fc 105 Hz Gain 2.5 dB Q 0.70
//   Filter 2: ON LSC Fc 105 Hz Gain 5.0 dB Q 0.70
//   Filter 3: ON HSC Fc 10000 Hz Gain -2.0 dB Q 0.70

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace q45
{
// Order matches the plugin's per-band "Type" choice parameter.
enum class BandType { Off = 0, Peak = 1, LowShelf = 2, HighShelf = 3 };

struct Band
{
    BandType type = BandType::Off;
    double freq = 1000.0;
    double gainDb = 0.0;
    double q = 0.707;
};

struct ParsedEq
{
    double preampDb = 0.0;
    bool hasPreamp = false;
    std::vector<Band> bands;
    int skippedUnsupported = 0; // filter types this plugin does not have (LP, HP, notch...)
    int skippedOverflow = 0;    // more filters than the plugin has bands
};

namespace detail
{
inline std::string upper (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(),
                    [] (unsigned char c) { return (char) std::toupper (c); });
    return s;
}

inline bool toNumber (const std::string& s, double& out)
{
    char* end = nullptr;
    out = std::strtod (s.c_str(), &end);
    return end != s.c_str();
}

inline bool isKnownType (const std::string& t)
{
    static const char* known[] = { "PK", "PEQ", "MODAL", "LS", "LSC", "HS", "HSC",
                                   "LP", "LPQ", "HP", "HPQ", "BP", "NO", "AP" };
    for (auto* k : known)
        if (t == k)
            return true;
    return false;
}
} // namespace detail

inline ParsedEq parseAutoEq (const std::string& text, int maxBands = 8)
{
    using namespace detail;
    ParsedEq out;
    std::istringstream lines (text);
    std::string line;

    while (std::getline (lines, line))
    {
        std::vector<std::string> tok;
        {
            std::istringstream words (line);
            std::string w;
            while (words >> w)
                tok.push_back (w);
        }
        if (tok.empty())
            continue;

        const auto first = upper (tok[0]);

        if (first == "PREAMP:" || first == "PREAMP")
        {
            for (size_t k = 1; k < tok.size(); ++k)
            {
                double v = 0.0;
                if (toNumber (tok[k], v))
                {
                    out.preampDb = std::clamp (v, -18.0, 6.0);
                    out.hasPreamp = true;
                    break;
                }
            }
            continue;
        }

        if (first.rfind ("FILTER", 0) != 0)
            continue; // ignore anything that is not a Filter line

        bool on = true;
        std::string type;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        double fc = nan, gain = 0.0, q = nan;

        for (size_t k = 1; k < tok.size(); ++k)
        {
            const auto up = upper (tok[k]);
            if (up == "ON")                           on = true;
            else if (up == "OFF")                     on = false;
            else if (up == "FC" && k + 1 < tok.size()) toNumber (tok[k + 1], fc);
            else if (up == "GAIN" && k + 1 < tok.size()) toNumber (tok[k + 1], gain);
            else if (up == "Q" && k + 1 < tok.size())  toNumber (tok[k + 1], q);
            else if (type.empty() && isKnownType (up)) type = up;
        }

        if (! on)
            continue;

        Band b;
        if (type == "PK" || type == "PEQ" || type == "MODAL")  b.type = BandType::Peak;
        else if (type == "LS" || type == "LSC")                b.type = BandType::LowShelf;
        else if (type == "HS" || type == "HSC")                b.type = BandType::HighShelf;
        else                                                   b.type = BandType::Off;

        if (b.type == BandType::Off || std::isnan (fc))
        {
            ++out.skippedUnsupported;
            continue;
        }

        if ((int) out.bands.size() >= maxBands)
        {
            ++out.skippedOverflow;
            continue;
        }

        b.freq = std::clamp (fc, 20.0, 20000.0);
        b.gainDb = std::clamp (gain, -18.0, 18.0);
        b.q = std::isnan (q) ? 0.707 : std::clamp (q, 0.1, 10.0);
        out.bands.push_back (b);
    }
    return out;
}
} // namespace q45
