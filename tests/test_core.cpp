#include <cmath>
#include <cstdio>
#include <string>

#include "AutoEqParser.h"
#include "CrossfeedMath.h"

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (! (cond)) { std::printf ("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } \
    } while (0)

static bool near (double a, double b, double tol) { return std::fabs (a - b) <= tol; }

int main()
{
    // Crossfeed maths
    CHECK (near (q45::itdSeconds (90.0) * 1000.0, 0.381, 0.005));
    CHECK (near (q45::itdSeconds (60.0) * 1000.0, 0.261, 0.005));
    CHECK (near (q45::bleedDb (90.0), -5.0, 1e-9));
    CHECK (near (q45::bleedDb (30.0), -3.0, 1e-9));
    CHECK (near (q45::bleedDb (200.0), -6.0, 1e-9)); // clamped

    // Parser: typical AutoEQ output
    const std::string txt =
        "Preamp: -6.1 dB\r\n"
        "Filter 1: ON PK Fc 105 Hz Gain 2.5 dB Q 0.70\r\n"
        "Filter 2: ON LSC Fc 105 Hz Gain 5.0 dB Q 0.70\n"
        "Filter 3: ON HSC Fc 10000 Hz Gain -2.0 dB Q 0.70\n"
        "Filter 4: OFF PK Fc 3000 Hz Gain 4.0 dB Q 2.0\n"
        "Filter 5: ON LPQ Fc 18000 Hz Q 0.7\n"
        "GraphicEQ: 20 0; 100 1\n";
    auto p = q45::parseAutoEq (txt, 8);
    CHECK (p.hasPreamp && near (p.preampDb, -6.1, 1e-9));
    CHECK (p.bands.size() == 3);
    CHECK (p.skippedUnsupported == 1); // the LPQ; the OFF filter is just ignored
    CHECK (p.bands[0].type == q45::BandType::Peak && near (p.bands[0].freq, 105, 1e-9)
           && near (p.bands[0].gainDb, 2.5, 1e-9) && near (p.bands[0].q, 0.70, 1e-9));
    CHECK (p.bands[1].type == q45::BandType::LowShelf && near (p.bands[1].gainDb, 5.0, 1e-9));
    CHECK (p.bands[2].type == q45::BandType::HighShelf && near (p.bands[2].gainDb, -2.0, 1e-9));

    // Overflow past the available bands
    std::string many;
    for (int i = 1; i <= 10; ++i)
        many += "Filter " + std::to_string (i) + ": ON PK Fc " + std::to_string (100 * i) + " Hz Gain 1 dB Q 1\n";
    auto o = q45::parseAutoEq (many, 8);
    CHECK (o.bands.size() == 8 && o.skippedOverflow == 2);

    // Garbage in, nothing out
    auto g = q45::parseAutoEq ("hello world\n", 8);
    CHECK (g.bands.empty() && ! g.hasPreamp);

    if (failures == 0)
        std::printf ("All core tests passed.\n");
    return failures == 0 ? 0 : 1;
}
