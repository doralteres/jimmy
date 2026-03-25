#pragma once

// Dark theme constants for Jimmy
namespace Theme
{
    // Background
    inline constexpr uint32_t kBackground    = 0xff1a1a1a;
    inline constexpr uint32_t kPanelBg       = 0xff252525;
    inline constexpr uint32_t kSectionHeader = 0xff2d2d2d;

    // Text
    inline constexpr uint32_t kTextPrimary   = 0xffffffff;
    inline constexpr uint32_t kTextDim       = 0xffaaaaaa;
    inline constexpr uint32_t kTextFaded     = 0xff666666;

    // Accents
    inline constexpr uint32_t kAccent        = 0xff00bcd4;
    inline constexpr uint32_t kChordColour   = 0xff00e5ff;

    // Status
    inline constexpr uint32_t kPlayingGreen  = 0xff4caf50;
    inline constexpr uint32_t kRecordingRed  = 0xfff44336;

    // Section colours (cycle through these)
    inline constexpr uint32_t kSectionColours[] = {
        0xff00bcd4,  // cyan
        0xff7c4dff,  // purple
        0xffff9800,  // orange
        0xff4caf50,  // green
        0xffe91e63,  // pink
        0xff03a9f4,  // blue
    };
    inline constexpr int kNumSectionColours = 6;
}
