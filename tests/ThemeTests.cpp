#include <gtest/gtest.h>
#include <juce_core/juce_core.h>
#include "Theme.h"

TEST(Theme, RtlHebrewText)
{
    EXPECT_TRUE(Theme::isRtlText(juce::String::fromUTF8("שלום")));
}

TEST(Theme, RtlArabicText)
{
    EXPECT_TRUE(Theme::isRtlText(juce::String::fromUTF8("مرحبا")));
}

TEST(Theme, LtrEnglishText)
{
    EXPECT_FALSE(Theme::isRtlText("Hello"));
}

TEST(Theme, LtrEmptyString)
{
    EXPECT_FALSE(Theme::isRtlText(""));
}

TEST(Theme, LtrSpacesOnly)
{
    EXPECT_FALSE(Theme::isRtlText("   "));
}

TEST(Theme, LtrMixedFirstCharLatin)
{
    // First non-space printable character is Latin → LTR
    EXPECT_FALSE(Theme::isRtlText(juce::String::fromUTF8("hello שלום")));
}

TEST(Theme, RtlLeadingSpace)
{
    // Leading spaces then Hebrew
    EXPECT_TRUE(Theme::isRtlText(juce::String::fromUTF8("  שלום")));
}
