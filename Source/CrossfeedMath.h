#pragma once
// Plain C++ (no JUCE): maps a virtual speaker angle to crossfeed delay and level.

#include <algorithm>
#include <cmath>

namespace q45
{
constexpr double kHeadRadiusM = 0.0875; // average head radius
constexpr double kSpeedOfSound = 343.0; // m/s
constexpr double kPi = 3.14159265358979323846;

// speakerAngleDeg is the TOTAL angle between the two speakers (90 = each speaker 45 deg off centre).
// Woodworth interaural time difference: (a / c) * (sin(phi) + phi), phi = half angle in radians.
inline double itdSeconds (double speakerAngleDeg)
{
    const double phi = (speakerAngleDeg * 0.5) * kPi / 180.0;
    return (kHeadRadiusM / kSpeedOfSound) * (std::sin (phi) + phi);
}

// Level of the opposite-channel bleed. Wider speakers bleed less:
// -3 dB at 30 deg, -5 dB at 90 deg, -6 dB at 120 deg.
inline double bleedDb (double speakerAngleDeg)
{
    const double a = std::clamp (speakerAngleDeg, 30.0, 120.0);
    return -3.0 - (a - 30.0) * (3.0 / 90.0);
}
} // namespace q45
