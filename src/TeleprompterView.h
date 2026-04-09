#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongModel.h"
#include "Theme.h"
#include "ChordParser.h"

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

    void setTransposeOffset(int offset)
    {
        if (transposeOffset != offset)
        {
            transposeOffset = offset;
            repaint();
        }
    }

    int getTransposeOffset() const { return transposeOffset; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(Theme::kBackground));

        // ── Transpose / Capo banner ──────────────────────────────────────────
        float bannerHeight = 0.0f;
        if (transposeOffset != 0)
        {
            bannerHeight = 30.0f;
            juce::String bannerText;
            if (transposeOffset < 0)
                bannerText = juce::String(juce::CharPointer_UTF8("\xf0\x9f\x8e\xb8 Capo on fret "))
                             + juce::String(-transposeOffset);
            else
                bannerText = "Transpose: +" + juce::String(transposeOffset);

            g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.15f));
            g.fillRect(0.0f, 0.0f, (float)getWidth(), bannerHeight);
            g.setColour(juce::Colour(Theme::kAccent));
            g.setFont(juce::Font(juce::FontOptions(13.0f)));
            g.drawText(bannerText,
                       juce::Rectangle<float>(12.0f, 0.0f, (float)getWidth() - 24.0f, bannerHeight),
                       juce::Justification::centredLeft);
        }

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

        float y = bannerHeight - currentScrollY;
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

            // Build word-wrapped rows for this lyric line
            auto lyricsFont  = juce::Font(juce::FontOptions(fontSize));
            auto chordFont   = juce::Font(juce::FontOptions(fontSize * 0.75f));
            bool rtl         = isRtlText(line.text);
            double lineSpan  = line.endBar - line.startBar;
            auto wrappedRows = buildWrappedRows(line.text, lyricsFont, contentWidth);
            int  numRows     = (int)wrappedRows.size();

            // Compute row xFraction boundaries proportional to each row's char count
            std::vector<float> rowBoundaries(static_cast<size_t>(numRows + 1), 0.0f);
            {
                int totalChars = 0;
                for (const auto& r : wrappedRows) totalChars += r.text.length();
                totalChars = juce::jmax(1, totalChars);
                float cum = 0.0f;
                for (int r = 0; r < numRows; ++r)
                {
                    rowBoundaries[static_cast<size_t>(r)] = cum;
                    cum += (float)wrappedRows[static_cast<size_t>(r)].text.length() / (float)totalChars;
                }
                rowBoundaries[static_cast<size_t>(numRows)] = 1.0f;
            }

            // Assign each chord to a wrapped row
            auto lineChords = getChordsForLine(line.startBar, line.endBar);
            struct PlacedChord { int row; float subFrac; Chord chord; };
            std::vector<PlacedChord> placedChords;
            for (const auto& lc : lineChords)
            {
                float xf = (lineSpan > 0.001)
                    ? static_cast<float>((lc.barPosition - line.startBar) / lineSpan) : 0.0f;
                xf = juce::jlimit(0.0f, 1.0f, xf);
                int r = numRows - 1;
                for (int ri = 0; ri < numRows; ++ri)
                    if (xf < rowBoundaries[static_cast<size_t>(ri + 1)]) { r = ri; break; }
                float span = rowBoundaries[static_cast<size_t>(r + 1)] - rowBoundaries[static_cast<size_t>(r)];
                float subFrac = (span > 0.0f) ? (xf - rowBoundaries[static_cast<size_t>(r)]) / span : 0.0f;
                placedChords.push_back({ r, juce::jlimit(0.0f, 1.0f, subFrac), lc });
            }

            // Highlight spans all wrapped rows for the current line
            if (i == currentLineIndex)
            {
                float totalH = (float)numRows * (chordHeight + lineHeight);
                g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.08f));
                g.fillRect(0.0f, y, (float)getWidth(), totalH);
            }

            // Draw each wrapped row: chords above, then lyric text below
            float rowY = y;
            for (int r = 0; r < numRows; ++r)
            {
                const auto& row = wrappedRows[static_cast<size_t>(r)];
                float rowTextWidth = juce::jlimit(1.0f, contentWidth, row.textWidth);

                // Draw chords for this row.
                // Chords are sorted by subFrac and drawn with adaptive font scaling:
                // if a chord would overlap its neighbour at the default size, the font
                // is reduced proportionally so every chord stays visible.
                if (rowY + chordHeight > 0 && rowY < viewHeight)
                {
                    // Collect chords for this row, sorted by subFrac (ascending).
                    // For LTR this is left→right; for RTL subFrac=0 is rightmost so
                    // ascending subFrac is also the natural draw order (right→left).
                    std::vector<const PlacedChord*> rowChords;
                    for (const auto& pc : placedChords)
                        if (pc.row == r) rowChords.push_back(&pc);
                    std::sort(rowChords.begin(), rowChords.end(),
                              [](const PlacedChord* a, const PlacedChord* b)
                              { return a->subFrac < b->subFrac; });

                    const float minChordFontSize = juce::jmax(8.0f, fontSize * 0.3f);
                    // lastBarrier: for LTR = right edge of prev chord;
                    //              for RTL = left  edge of prev chord.
                    float lastBarrier = -1.0f;

                    for (int ci = 0; ci < (int)rowChords.size(); ++ci)
                    {
                        const auto& pc = *rowChords[static_cast<size_t>(ci)];

                        // Measures the chord label width at a given font size (with padding).
                        auto measureChordW = [&pc, this](float fs) -> float
                        {
                            juce::GlyphArrangement ga;
                            ga.addLineOfText(juce::Font(juce::FontOptions(fs)),
                                             ChordParser::transposeChordName(pc.chord.name, transposeOffset),
                                             0, 0);
                            return ga.getBoundingBox(0, -1, true).getWidth() + 8.0f;
                        };

                        if (rtl)
                        {
                            // idealRightEdge: where this chord's right side should sit.
                            float idealRightEdge = margin + contentWidth - pc.subFrac * rowTextWidth;
                            // Clamp so we don't draw on top of the previously drawn chord.
                            float actualRightEdge = (lastBarrier >= 0.0f)
                                ? juce::jmin(idealRightEdge, lastBarrier)
                                : idealRightEdge;

                            // Space available up to the next chord's ideal right edge.
                            float nextIdealRightEdge = (ci + 1 < (int)rowChords.size())
                                ? (margin + contentWidth - rowChords[static_cast<size_t>(ci + 1)]->subFrac * rowTextWidth)
                                : margin;
                            float availableWidth = actualRightEdge - nextIdealRightEdge;
                            if (availableWidth <= 0.0f) continue;

                            float usedFontSize = fontSize * 0.75f;
                            float chordW = measureChordW(usedFontSize);
                            if (chordW > availableWidth)
                            {
                                usedFontSize = juce::jmax(minChordFontSize,
                                                          usedFontSize * availableWidth / chordW);
                                chordW = measureChordW(usedFontSize);
                            }

                            float chordX = actualRightEdge - chordW;
                            bool isActive = (pc.chord.barPosition <= currentBarPos);
                            g.setFont(juce::Font(juce::FontOptions(usedFontSize)));
                            g.setColour((isActive ? juce::Colour(Theme::kChordColour)
                                                  : juce::Colour(Theme::kChordDimColour))
                                        .withAlpha(isActive ? alpha : alpha * 0.85f));
                            g.drawText(ChordParser::transposeChordName(pc.chord.name, transposeOffset),
                                       juce::Rectangle<float>(chordX, rowY, chordW, chordHeight),
                                       juce::Justification::centredRight);
                            lastBarrier = chordX;
                        }
                        else
                        {
                            float idealX = margin + pc.subFrac * rowTextWidth;
                            // Never draw over the previous chord.
                            float startX = (lastBarrier >= 0.0f)
                                ? juce::jmax(idealX, lastBarrier)
                                : idealX;

                            // Space available up to the next chord's ideal start.
                            float nextIdealX = (ci + 1 < (int)rowChords.size())
                                ? (margin + rowChords[static_cast<size_t>(ci + 1)]->subFrac * rowTextWidth)
                                : (margin + contentWidth);
                            float availableWidth = nextIdealX - startX;
                            if (availableWidth <= 0.0f) continue;

                            float usedFontSize = fontSize * 0.75f;
                            float chordW = measureChordW(usedFontSize);
                            if (chordW > availableWidth)
                            {
                                usedFontSize = juce::jmax(minChordFontSize,
                                                          usedFontSize * availableWidth / chordW);
                                chordW = measureChordW(usedFontSize);
                            }

                            bool isActive = (pc.chord.barPosition <= currentBarPos);
                            g.setFont(juce::Font(juce::FontOptions(usedFontSize)));
                            g.setColour((isActive ? juce::Colour(Theme::kChordColour)
                                                  : juce::Colour(Theme::kChordDimColour))
                                        .withAlpha(isActive ? alpha : alpha * 0.85f));
                            g.drawText(ChordParser::transposeChordName(pc.chord.name, transposeOffset),
                                       juce::Rectangle<float>(startX, rowY, chordW, chordHeight),
                                       juce::Justification::centredLeft);
                            lastBarrier = startX + chordW;
                        }
                    }
                }
                rowY += chordHeight;

                // Draw lyric text for this row
                if (rowY + lineHeight > 0 && rowY < viewHeight)
                {
                    g.setColour(juce::Colour(Theme::kTextPrimary).withAlpha(alpha));
                    g.setFont(lyricsFont);
                    auto tb = juce::Rectangle<float>(margin, rowY, contentWidth, lineHeight);
                    if (rtl)
                        g.drawText(row.text, tb, juce::Justification::centredRight);
                    else
                        g.drawText(row.text, tb, juce::Justification::centredLeft);
                }
                rowY += lineHeight;
            }
            y = rowY + lineSpacing;
        }
    }

private:
    double currentBarPos = 1.0;
    float  fontSize = 28.0f;
    int    transposeOffset = 0;
    float  currentScrollY = 0.0f;
    float  targetScrollY  = 0.0f;

    std::vector<LyricLine> currentLyrics;
    std::vector<Chord>     currentChords;
    std::vector<Section>   currentSections;

    struct WrappedRow
    {
        juce::String text;
        float textWidth = 0.0f;
    };

    // Splits text into word-wrapped rows that each fit within maxWidth.
    std::vector<WrappedRow> buildWrappedRows(const juce::String& text,
                                              const juce::Font& font,
                                              float maxWidth) const
    {
        auto measureW = [&font](const juce::String& s) -> float
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText(font, s, 0.0f, 0.0f);
            return ga.getBoundingBox(0, -1, true).getWidth();
        };

        if (text.isEmpty())
            return { { text, 0.0f } };

        std::vector<WrappedRow> rows;
        juce::StringArray words;
        words.addTokens(text, " ", "\"");

        juce::String current;
        for (int wi = 0; wi < words.size(); ++wi)
        {
            juce::String candidate = current.isEmpty() ? words[wi]
                                                       : (current + " " + words[wi]);
            if (measureW(candidate) <= maxWidth || current.isEmpty())
                current = candidate;
            else
            {
                rows.push_back({ current, measureW(current) });
                current = words[wi];
            }
        }
        if (!current.isEmpty())
            rows.push_back({ current, measureW(current) });
        if (rows.empty())
            rows.push_back({ text, measureW(text) });
        return rows;
    }

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
            {
                int numRows = (int)buildWrappedRows(
                    loopLine.text,
                    juce::Font(juce::FontOptions(fontSize)),
                    juce::jmax(1.0f, (float)getWidth() - 40.0f)).size();
                y += (float)numRows * (chordHeight + lineHeight) + lineSpacing;
            }
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
