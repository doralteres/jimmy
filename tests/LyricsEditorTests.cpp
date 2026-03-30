#include <gtest/gtest.h>
#include <juce_core/juce_core.h>

// Forward-declare the static helpers we need.
// LyricsEditor.h requires juce_gui_basics for the component,
// so we test only the public static parsing helpers.
#include "LyricsEditor.h"

// ═══════════════════════════════════════════════════════════════════
// isSectionMarker
// ═══════════════════════════════════════════════════════════════════

TEST(LyricsEditor, SectionMarker_Verse)
{
    juce::String name;
    EXPECT_TRUE(LyricsEditor::isSectionMarker("[Verse 1]", name));
    EXPECT_EQ(name, juce::String("Verse 1"));
}

TEST(LyricsEditor, SectionMarker_Chorus)
{
    juce::String name;
    EXPECT_TRUE(LyricsEditor::isSectionMarker("[Chorus]", name));
    EXPECT_EQ(name, juce::String("Chorus"));
}

TEST(LyricsEditor, SectionMarker_WithSpaces)
{
    juce::String name;
    EXPECT_TRUE(LyricsEditor::isSectionMarker("  [Bridge]  ", name));
    EXPECT_EQ(name, juce::String("Bridge"));
}

TEST(LyricsEditor, SectionMarker_BreakIsNotSection)
{
    juce::String name;
    EXPECT_FALSE(LyricsEditor::isSectionMarker("[break: 3]", name));
}

TEST(LyricsEditor, SectionMarker_LengthIsNotSection)
{
    juce::String name;
    EXPECT_FALSE(LyricsEditor::isSectionMarker("[length: 4]", name));
}

TEST(LyricsEditor, SectionMarker_EmptyBrackets)
{
    juce::String name;
    EXPECT_FALSE(LyricsEditor::isSectionMarker("[]", name));
}

TEST(LyricsEditor, SectionMarker_PlainText)
{
    juce::String name;
    EXPECT_FALSE(LyricsEditor::isSectionMarker("not a section", name));
}

// ═══════════════════════════════════════════════════════════════════
// isBreakDirective
// ═══════════════════════════════════════════════════════════════════

TEST(LyricsEditor, Break_IntegerBars)
{
    double bars = 0.0;
    EXPECT_TRUE(LyricsEditor::isBreakDirective("[break: 2]", bars));
    EXPECT_DOUBLE_EQ(bars, 2.0);
}

TEST(LyricsEditor, Break_DecimalBars)
{
    double bars = 0.0;
    EXPECT_TRUE(LyricsEditor::isBreakDirective("[break: 2.5]", bars));
    EXPECT_DOUBLE_EQ(bars, 2.5);
}

TEST(LyricsEditor, Break_CaseInsensitive)
{
    double bars = 0.0;
    EXPECT_TRUE(LyricsEditor::isBreakDirective("[Break: 3]", bars));
    EXPECT_DOUBLE_EQ(bars, 3.0);
}

TEST(LyricsEditor, Break_ZeroBarsReturnsFalse)
{
    double bars = 0.0;
    EXPECT_FALSE(LyricsEditor::isBreakDirective("[break: 0]", bars));
}

TEST(LyricsEditor, Break_NegativeReturnsFalse)
{
    double bars = 0.0;
    EXPECT_FALSE(LyricsEditor::isBreakDirective("[break: -1]", bars));
}

// ═══════════════════════════════════════════════════════════════════
// extractLengthDirective
// ═══════════════════════════════════════════════════════════════════

TEST(LyricsEditor, Length_AtEnd)
{
    double len = 0.0;
    auto text = LyricsEditor::extractLengthDirective("Some lyrics [length: 4]", len);
    EXPECT_EQ(text, juce::String("Some lyrics"));
    EXPECT_DOUBLE_EQ(len, 4.0);
}

TEST(LyricsEditor, Length_DecimalValue)
{
    double len = 0.0;
    auto text = LyricsEditor::extractLengthDirective("Line text [length: 2.5]", len);
    EXPECT_EQ(text, juce::String("Line text"));
    EXPECT_DOUBLE_EQ(len, 2.5);
}

TEST(LyricsEditor, Length_NoDirective)
{
    double len = 0.0;
    auto text = LyricsEditor::extractLengthDirective("Regular text", len);
    EXPECT_EQ(text, juce::String("Regular text"));
    EXPECT_DOUBLE_EQ(len, 0.0);
}

TEST(LyricsEditor, Length_OnlyDirective)
{
    double len = 0.0;
    auto text = LyricsEditor::extractLengthDirective("[length: 3]", len);
    EXPECT_DOUBLE_EQ(len, 3.0);
    EXPECT_TRUE(text.isEmpty());
}

// ═══════════════════════════════════════════════════════════════════
// Round-trip: format text with [length: N] → re-parse → lengths preserved
// These tests simulate what rebuildBulkText() + parseBulkText() do,
// using only the public static helpers.
// ═══════════════════════════════════════════════════════════════════

// Helper: format a lyric line with optional [length: N] tag, same logic as rebuildBulkText
static juce::String formatLine(const juce::String& text, double lineLen, double defaultBarsPerLine)
{
    juce::String result = text;
    if (std::abs(lineLen - defaultBarsPerLine) > 0.01)
        result += " [length: " + juce::String(lineLen, 1) + "]";
    return result;
}

TEST(LyricsEditor, RoundTrip_CustomLengthPreserved)
{
    double defaultBpl = 2.0;
    juce::String original = "Hello world";
    double customLen = 4.0;

    // Format with [length: N] tag (simulates rebuildBulkText)
    auto formatted = formatLine(original, customLen, defaultBpl);
    EXPECT_EQ(formatted, juce::String("Hello world [length: 4.0]"));

    // Re-parse (simulates parseBulkText)
    double parsedLen = 0.0;
    auto parsedText = LyricsEditor::extractLengthDirective(formatted, parsedLen);
    double barsForLine = (parsedLen > 0.0) ? parsedLen : defaultBpl;

    EXPECT_EQ(parsedText, original);
    EXPECT_NEAR(barsForLine, customLen, 0.01);
}

TEST(LyricsEditor, RoundTrip_DefaultLengthNoTag)
{
    double defaultBpl = 2.0;
    juce::String original = "Hello world";

    // Default length → no tag emitted
    auto formatted = formatLine(original, defaultBpl, defaultBpl);
    EXPECT_EQ(formatted, original);

    // Re-parse: no directive → falls back to default
    double parsedLen = 0.0;
    auto parsedText = LyricsEditor::extractLengthDirective(formatted, parsedLen);
    double barsForLine = (parsedLen > 0.0) ? parsedLen : defaultBpl;

    EXPECT_EQ(parsedText, original);
    EXPECT_NEAR(barsForLine, defaultBpl, 0.01);
}

TEST(LyricsEditor, RoundTrip_BreakDirectiveSurvives)
{
    // Break formatted text
    double breakLen = 3.0;
    juce::String breakText = "[break: " + juce::String(breakLen, 1) + "]";

    // Parse it back
    double parsedBars = 0.0;
    EXPECT_TRUE(LyricsEditor::isBreakDirective(breakText, parsedBars));
    EXPECT_NEAR(parsedBars, breakLen, 0.01);
}

TEST(LyricsEditor, RoundTrip_SectionMarkerSurvives)
{
    juce::String sectionText = "[Verse 1]";
    juce::String name;
    EXPECT_TRUE(LyricsEditor::isSectionMarker(sectionText, name));
    EXPECT_EQ(name, juce::String("Verse 1"));
}

TEST(LyricsEditor, RoundTrip_DecimalLength)
{
    double defaultBpl = 2.0;
    juce::String original = "Singing along";
    double customLen = 3.5;

    auto formatted = formatLine(original, customLen, defaultBpl);
    EXPECT_EQ(formatted, juce::String("Singing along [length: 3.5]"));

    double parsedLen = 0.0;
    auto parsedText = LyricsEditor::extractLengthDirective(formatted, parsedLen);
    EXPECT_EQ(parsedText, original);
    EXPECT_NEAR(parsedLen, customLen, 0.01);
}

TEST(LyricsEditor, RoundTrip_MultiLineTextPreservesLengths)
{
    // Simulate a full song text round-trip with mixed lengths
    double defaultBpl = 2.0;

    struct TestLine { juce::String text; double len; bool isBreak; };
    std::vector<TestLine> lines = {
        { "First line",  4.0, false },
        { "Second line", 2.0, false },  // default length
        { "",            3.0, true  },  // break
        { "Third line",  1.5, false },
    };

    // Format all lines (simulates rebuildBulkText)
    juce::String fullText;
    for (const auto& l : lines)
    {
        if (fullText.isNotEmpty()) fullText += "\n";
        if (l.isBreak)
            fullText += "[break: " + juce::String(l.len, 1) + "]";
        else
            fullText += formatLine(l.text, l.len, defaultBpl);
    }

    // Re-parse all lines (simulates parseBulkText)
    auto parsedLines = juce::StringArray::fromLines(fullText);
    int parsedIdx = 0;
    for (const auto& lineText : parsedLines)
    {
        auto trimmed = lineText.trim();
        if (trimmed.isEmpty()) continue;

        juce::String sectionName;
        if (LyricsEditor::isSectionMarker(trimmed, sectionName)) continue;

        double breakBars = 0.0;
        if (LyricsEditor::isBreakDirective(trimmed, breakBars))
        {
            // Find matching break in original
            while (parsedIdx < (int)lines.size() && !lines[static_cast<size_t>(parsedIdx)].isBreak)
                parsedIdx++;
            ASSERT_LT(parsedIdx, (int)lines.size());
            EXPECT_NEAR(breakBars, lines[static_cast<size_t>(parsedIdx)].len, 0.01);
            parsedIdx++;
            continue;
        }

        double lineLength = 0.0;
        auto lyricText = LyricsEditor::extractLengthDirective(trimmed, lineLength);
        double barsForLine = (lineLength > 0.0) ? lineLength : defaultBpl;

        // Find matching non-break line
        while (parsedIdx < (int)lines.size() && lines[static_cast<size_t>(parsedIdx)].isBreak)
            parsedIdx++;
        ASSERT_LT(parsedIdx, (int)lines.size());
        EXPECT_EQ(lyricText, lines[static_cast<size_t>(parsedIdx)].text);
        EXPECT_NEAR(barsForLine, lines[static_cast<size_t>(parsedIdx)].len, 0.01);
        parsedIdx++;
    }
}

TEST(LyricsEditor, RoundTrip_HebrewTextWithLength)
{
    double defaultBpl = 2.0;
    juce::String hebrew = juce::String::fromUTF8("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d \xd7\xa2\xd7\x95\xd7\x9c\xd7\x9d");
    double customLen = 3.0;

    auto formatted = formatLine(hebrew, customLen, defaultBpl);

    double parsedLen = 0.0;
    auto parsedText = LyricsEditor::extractLengthDirective(formatted, parsedLen);
    EXPECT_EQ(parsedText, hebrew);
    EXPECT_NEAR(parsedLen, customLen, 0.01);
}
