#pragma once

// Dark theme constants for Jimmy — refined dark palette with warm amber accent
namespace Theme
{
    // Background layers (near-black with slight blue hue)
    inline constexpr uint32_t kBackground    = 0xff0e0e12;  // true background
    inline constexpr uint32_t kPanelBg       = 0xff161620;  // transport bar / header panels
    inline constexpr uint32_t kSurface       = 0xff1a1a22;  // elevated cards / input fields
    inline constexpr uint32_t kSurfaceLight  = 0xff242430;  // alt rows / hover states
    inline constexpr uint32_t kSectionHeader = 0xff1e1e28;  // section header rows
    inline constexpr uint32_t kBorder        = 0xff2a2a38;  // subtle dividers

    // Text
    inline constexpr uint32_t kTextPrimary   = 0xffe8e8f0;  // slightly warm white
    inline constexpr uint32_t kTextSecondary = 0xff8888a0;  // labels, hints (~55% on dark)
    inline constexpr uint32_t kTextDim       = 0xff8888a0;  // alias for secondary
    inline constexpr uint32_t kTextFaded     = 0xff4a4a5a;  // disabled / placeholder

    // Accent — warm amber for high visibility on stage
    inline constexpr uint32_t kAccent        = 0xfff5a623;
    inline constexpr uint32_t kAccentDim     = 0xffc4851a;  // reduced intensity accent
    inline constexpr uint32_t kChordColour   = 0xffffd074;  // warm gold chord
    inline constexpr uint32_t kChordDimColour = 0xffa08040; // dimmed upcoming chord

    // Status
    inline constexpr uint32_t kPlayingGreen  = 0xff4caf50;
    inline constexpr uint32_t kRecordingRed  = 0xffe05555;  // slightly softer red

    // Semantic
    inline constexpr uint32_t kSuccess       = 0xff4caf50;
    inline constexpr uint32_t kDanger        = 0xffc0544e;  // desaturated red for destructive

    // Section colours (cycle through these — complement amber on deep dark)
    inline constexpr uint32_t kSectionColours[] = {
        0xfff5a623,  // amber (accent)
        0xff9b6dff,  // violet
        0xff4caf50,  // green
        0xff00b8d4,  // teal
        0xffe06090,  // rose
        0xff5ca0ff,  // sky blue
    };
    inline constexpr int kNumSectionColours = 6;

    // Simple RTL detection: check if text starts with Hebrew/Arabic Unicode range
    inline bool isRtlText(const juce::String& text)
    {
        for (auto c : text)
        {
            if (c >= 0x0590 && c <= 0x08FF)  // Hebrew, Arabic, and related blocks
                return true;
            if (c > 0x7F)
                return false;  // other non-ASCII, assume LTR
            if (c > ' ')
                return false;  // ASCII letter, LTR
        }
        return false;
    }
}
