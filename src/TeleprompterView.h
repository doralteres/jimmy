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

            // Skip break entries — they are timeline spacers, not displayed
            if (line.isBreak)
                continue;

            // Check if we need to draw a section header
            int sectionIdx = line.sectionIndex;
            if (sectionIdx >= 0 && sectionIdx < (int)currentSections.size() && sectionIdx != lastSectionDrawn)
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

                // Measure the actual rendered width of the lyric text so 100% maps
                // to the end of the text, not the full content box.
                juce::GlyphArrangement lineGlyphs;
                lineGlyphs.addLineOfText(juce::Font(juce::FontOptions(fontSize)),
                                         line.text, 0.0f, 0.0f);
                float lineTextWidth = lineGlyphs.getBoundingBox(0, -1, true).getWidth();
                lineTextWidth = juce::jlimit(1.0f, contentWidth, lineTextWidth);

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
                        // RTL: rightmost = start of line, leftward = later in bar.
                        // 100% maps to the left edge of the rendered text.
                        float rightEdge = margin + contentWidth - xFraction * lineTextWidth;
                        chordX = rightEdge - chordW;

                        // Skip if overlapping previous chord (previous is to the right)
                        if (lastChordRight >= 0.0f && chordX + chordW > lastChordRight)
                            continue;
                    }
                    else
                    {
                        // LTR: 100% maps to the right edge of the rendered text.
                        chordX = margin + xFraction * lineTextWidth;

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
            if (currentLyrics[static_cast<size_t>(i)].isBreak)
                continue;
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
            const auto& loopLine = currentLyrics[static_cast<size_t>(i)];
            if (loopLine.isBreak)
                continue;

            int sectionIdx = loopLine.sectionIndex;
            if (sectionIdx >= 0 && sectionIdx < (int)currentSections.size() && sectionIdx != lastSectionDrawn)
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

    static bool isRtlText(const juce::String& text) { return Theme::isRtlText(text); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TeleprompterView)
};
