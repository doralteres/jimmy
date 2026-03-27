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
