# Q45 Monitor

A monitoring-only plugin (AU / VST3 / Standalone) for mixing on Soundcore Space Q45 headphones:

1. **Crossfeed / virtual speakers.** Each channel bleeds into the opposite ear, delayed and low-passed,
   to mimic speakers. Default speaker angle is **90 deg** (each speaker 45 deg off centre).
   Delay is the Woodworth head model (about 0.38 ms at 90 deg, 0.26 ms at 60 deg); bleed level goes from -3 dB (30 deg) to -6 dB (120 deg), -5 dB at 90 deg.
2. **8-band correction EQ** (Peak / Low Shelf / High Shelf) with a **paste-in AutoEQ importer**.

## Important: the Q45 default is a rough placeholder

The default correction is a single -3 dB low shelf at 120 Hz, based only on RTINGS describing the Q45's bass as
about 3 dB emphasised. It is **not** a measured correction. For a real one:

1. Find the Soundcore Space Q45 in the AutoEQ project (github.com/jaakkopasanen/AutoEq or an AutoEQ web app),
   target "Harman over-ear 2018", and get the **ParametricEQ.txt**. (I could not confirm it is in the database.)
2. Paste its contents into the plugin and press **Apply AutoEQ filters**. Preamp is applied too.
3. Only PK / LS / HS filters are supported, up to 8. If AutoEQ gives more, re-run it with 8 filters or fewer.

Measure with the headphones wired (not Bluetooth/LDAC), ANC off, and the Soundcore app EQ flat.

## Build (macOS, for Logic)

Prereqs: Xcode command line tools, CMake 3.22+, internet (JUCE 8.0.4 is fetched automatically).

    cd q45-monitor
    cmake -B build -G Xcode
    cmake --build build --config Release

`COPY_PLUGIN_AFTER_BUILD` installs the AU to `~/Library/Audio/Plug-Ins/Components`. Then:

- Open Logic, let it rescan plugins (or run `killall -9 AudioComponentRegistrar`).
- Insert **Q45 Monitor** on the **Stereo Out** as the last plugin (Audio FX > Personal).
- If Logic rejects it, run `auval -v aufx Q45m Pers`.
- **Bypass it when you bounce.**

## Tests

`tests/test_core.cpp` covers the AutoEQ parser and crossfeed maths (no JUCE needed):

    g++ -std=c++17 -ISource tests/test_core.cpp -o core_tests && ./core_tests

## Status

The parser and crossfeed maths are unit-tested. The JUCE plugin code (DSP, UI) has **not been compiled or
listened to**; expect to fix small build errors on first build.
