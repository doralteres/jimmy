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
        float contentWidth = (float)getWidth() - 40.0f;
        float margin = 20.0f;

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

                    auto sectionBounds = juce::Rectangle<float>(margin, y, contentWidth, sectionHeight);
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

            // Draw all chords for this lyric line at proportional X positions
            if (y + chordHeight > 0 && y < viewHeight)
            {
                auto lineChords = getChordsForLine(line.startBar, line.endBar);
                bool rtl = isRtlText(line.text);
                double lineSpan = line.endBar - line.startBar;

                g.setFont(juce::Font(juce::FontOptions(fontSize * 0.75f)));

                float lastChordRight = -1.0f;

                for (const auto& lc : lineChords)
                {
                    // Proportional position within the line's bar range
                    float xFraction = (lineSpan > 0.001)
                        ? static_cast<float>((lc.barPosition - line.startBar) / lineSpan)
                        : 0.0f;
                    xFraction = juce::jlimit(0.0f, 1.0f, xFraction);

                    // Measure chord text width
                    juce::GlyphArrangement glyphs;
                    glyphs.addLineOfText(g.getCurrentFont(), lc.name, 0.0f, 0.0f);
                    float textW = glyphs.getBoundingBox(0, -1, true).getWidth();
                    float chordW = textW + 8.0f;

                    float chordX;
                    if (rtl)
                    {
                        // RTL: rightmost = start of line, leftward = later in bar
                        float rightEdge = margin + contentWidth - xFraction * contentWidth;
                        chordX = rightEdge - chordW;

                        // Skip if overlapping previous chord (previous is to the right)
                        if (lastChordRight >= 0.0f && chordX + chordW > lastChordRight)
                            continue;
                    }
                    else
                    {
                        chordX = margin + xFraction * contentWidth;

                        // Skip if overlapping previous chord
                        if (chordX < lastChordRight)
                            continue;
                    }

                    // Active chord (currently playing) is brighter
                    bool isActive = (lc.barPosition <= currentBarPos);

                    if (isActive)
                        g.setColour(juce::Colour(Theme::kChordColour).withAlpha(alpha));
                    else
                        g.setColour(juce::Colour(Theme::kChordDimColour).withAlpha(alpha * 0.85f));

                    auto chordBounds = juce::Rectangle<float>(chordX, y, chordW, chordHeight);
                    if (rtl)
                    {
                        g.drawText(lc.name, chordBounds, juce::Justification::centredRight);
                        lastChordRight = chordX;
                    }
                    else
                    {
                        g.drawText(lc.name, chordBounds, juce::Justification::centredLeft);
                        lastChordRight = chordX + chordW;
                    }
                }
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
                    g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.08f));
                    g.fillRect(0.0f, y, (float)getWidth(), lineHeight);
                    g.setColour(juce::Colour(Theme::kTextPrimary));
                }

                auto textBounds = juce::Rectangle<float>(margin, y, contentWidth, lineHeight);

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

    // Returns all chords relevant to a lyric line's bar range.
    // Includes the carry-over chord (last chord before lineStart) plus
    // all chords within [lineStart, lineEnd).
    std::vector<Chord> getChordsForLine(double lineStart, double lineEnd) const
    {
        std::vector<Chord> result;

        // Find carry-over chord: last chord before this line starts
        const Chord* carryOver = nullptr;
        for (const auto& c : currentChords)
        {
            if (c.barPosition < lineStart)
                carryOver = &c;
            else
                break;
        }

        if (carryOver != nullptr)
            result.push_back({ carryOver->name, lineStart, carryOver->source });

        // Collect all chords within the line's bar range
        for (const auto& c : currentChords)
        {
            if (c.barPosition >= lineStart && c.barPosition < lineEnd)
            {
                // Avoid duplicate if carry-over chord is at exactly lineStart
                if (!result.empty() && std::abs(result.back().barPosition - c.barPosition) < 0.01
                    && result.back().name == c.name)
                    continue;
                result.push_back(c);
            }
            else if (c.barPosition >= lineEnd)
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
