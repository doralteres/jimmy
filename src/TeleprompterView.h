#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongModel.h"
#include "Theme.h"

// Auto-scrolling teleprompter view for live performance.
// Displays lyrics with chords above, synced to the DAW timeline.
class TeleprompterView : public juce::Component
{
public:
    TeleprompterView() = default;

    void setSongData(const std::vector<LyricLine>& lyrics,
                     const std::vector<Chord>& chords,
                     const std::vector<Section>& sections)
    {
        currentLyrics   = lyrics;
        currentChords   = chords;
        currentSections = sections;
        repaint();
    }

    void setPosition(double barPosition)
    {
        if (std::abs(currentBarPos - barPosition) > 0.001)
        {
            currentBarPos = barPosition;
            updateScrollTarget();
            repaint();
        }
    }

    void setFontSize(float size)
    {
        fontSize = juce::jlimit(14.0f, 72.0f, size);
        repaint();
    }

    float getFontSize() const { return fontSize; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(Theme::kBackground));

        if (currentLyrics.empty())
        {
            g.setColour(juce::Colour(Theme::kTextFaded));
            g.setFont(juce::Font(juce::FontOptions(fontSize)));
            g.drawText("No lyrics — switch to Edit mode to add lyrics",
                       getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Smooth scroll interpolation
        float scrollDelta = targetScrollY - currentScrollY;
        currentScrollY += scrollDelta * 0.15f;  // smooth ease

        float y = -currentScrollY;
        float lineHeight = fontSize * 1.4f;
        float chordHeight = fontSize * 0.8f;
        float sectionHeight = fontSize * 1.1f;
        float lineSpacing = lineHeight * 0.4f;
        int viewHeight = getHeight();

        int currentLineIndex = findCurrentLine();

        // Track which section was last drawn
        int lastSectionDrawn = -1;

        for (int i = 0; i < (int)currentLyrics.size(); ++i)
        {
            const auto& line = currentLyrics[static_cast<size_t>(i)];

            // Check if we need to draw a section header
            int sectionIdx = findSectionForBar(line.startBar);
            if (sectionIdx >= 0 && sectionIdx != lastSectionDrawn)
            {
                lastSectionDrawn = sectionIdx;
                const auto& sec = currentSections[static_cast<size_t>(sectionIdx)];

                if (y + sectionHeight > 0 && y < viewHeight)
                {
                    auto sectionColour = juce::Colour(
                        Theme::kSectionColours[sectionIdx % Theme::kNumSectionColours]);

                    g.setColour(sectionColour.withAlpha(0.15f));
                    g.fillRect(0.0f, y, (float)getWidth(), sectionHeight);

                    g.setColour(sectionColour);
                    g.setFont(juce::Font(juce::FontOptions(fontSize * 0.65f)));

                    auto sectionBounds = juce::Rectangle<float>(20.0f, y, (float)getWidth() - 40.0f, sectionHeight);
                    g.drawText(sec.name.toUpperCase(), sectionBounds, juce::Justification::centredLeft);
                }
                y += sectionHeight + 4.0f;
            }

            // Determine opacity based on proximity to current line
            float alpha;
            if (i == currentLineIndex)
                alpha = 1.0f;
            else if (std::abs(i - currentLineIndex) <= 2)
                alpha = 0.7f;
            else
                alpha = 0.4f;

            // Draw chord above the lyric line
            auto chordStr = getChordForBar(line.startBar);
            if (chordStr.isNotEmpty() && y + chordHeight > 0 && y < viewHeight)
            {
                g.setColour(juce::Colour(Theme::kChordColour).withAlpha(alpha));
                g.setFont(juce::Font(juce::FontOptions(fontSize * 0.75f)));

                auto chordBounds = juce::Rectangle<float>(20.0f, y, (float)getWidth() - 40.0f, chordHeight);

                if (isRtlText(line.text))
                    g.drawText(chordStr, chordBounds, juce::Justification::centredRight);
                else
                    g.drawText(chordStr, chordBounds, juce::Justification::centredLeft);
            }
            y += chordHeight;

            // Draw lyric line
            if (y + lineHeight > 0 && y < viewHeight)
            {
                g.setColour(juce::Colour(Theme::kTextPrimary).withAlpha(alpha));
                g.setFont(juce::Font(juce::FontOptions(fontSize)));

                // Highlight current line
                if (i == currentLineIndex)
                {
                    g.setColour(juce::Colour(Theme::kTextPrimary));
                    // Subtle highlight bar
                    g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.08f));
                    g.fillRect(0.0f, y, (float)getWidth(), lineHeight);
                    g.setColour(juce::Colour(Theme::kTextPrimary));
                }

                auto textBounds = juce::Rectangle<float>(20.0f, y, (float)getWidth() - 40.0f, lineHeight);

                if (isRtlText(line.text))
                    g.drawText(line.text, textBounds, juce::Justification::centredRight);
                else
                    g.drawText(line.text, textBounds, juce::Justification::centredLeft);
            }
            y += lineHeight + lineSpacing;
        }
    }

private:
    double currentBarPos = 1.0;
    float  fontSize = 28.0f;
    float  currentScrollY = 0.0f;
    float  targetScrollY  = 0.0f;

    std::vector<LyricLine> currentLyrics;
    std::vector<Chord>     currentChords;
    std::vector<Section>   currentSections;

    int findCurrentLine() const
    {
        int best = 0;
        for (int i = 0; i < (int)currentLyrics.size(); ++i)
        {
            if (currentBarPos >= currentLyrics[static_cast<size_t>(i)].startBar)
                best = i;
            else
                break;
        }
        return best;
    }

    void updateScrollTarget()
    {
        int currentLine = findCurrentLine();

        float lineHeight = fontSize * 1.4f;
        float chordHeight = fontSize * 0.8f;
        float lineSpacing = lineHeight * 0.4f;
        float sectionHeight = fontSize * 1.1f;
        float centerY = getHeight() * 0.35f;

        // Calculate Y position of the current line
        float y = 0.0f;
        int lastSectionDrawn = -1;
        for (int i = 0; i <= currentLine && i < (int)currentLyrics.size(); ++i)
        {
            int sectionIdx = findSectionForBar(currentLyrics[static_cast<size_t>(i)].startBar);
            if (sectionIdx >= 0 && sectionIdx != lastSectionDrawn)
            {
                lastSectionDrawn = sectionIdx;
                y += sectionHeight + 4.0f;
            }
            if (i < currentLine)
                y += chordHeight + lineHeight + lineSpacing;
        }

        targetScrollY = y - centerY;
        if (targetScrollY < 0.0f)
            targetScrollY = 0.0f;
    }

    juce::String getChordForBar(double barPos) const
    {
        juce::String result;
        for (const auto& c : currentChords)
        {
            if (c.barPosition <= barPos)
                result = c.name;
            else
                break;
        }
        return result;
    }

    int findSectionForBar(double barPos) const
    {
        for (int i = 0; i < (int)currentSections.size(); ++i)
        {
            const auto& s = currentSections[static_cast<size_t>(i)];
            if (barPos >= s.startBar && barPos <= s.endBar)
                return i;
        }
        return -1;
    }

    // Simple RTL detection: check if text starts with Hebrew/Arabic Unicode range
    static bool isRtlText(const juce::String& text)
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TeleprompterView)
};
