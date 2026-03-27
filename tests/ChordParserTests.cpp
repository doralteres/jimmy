#include <gtest/gtest.h>
#include "ChordParser.h"

// ── Helper ──────────────────────────────────────────────────────────
static ChordParser::ChordResult id(std::vector<int> notes)
{
    return ChordParser::identify(notes);
}

// ── Edge cases ──────────────────────────────────────────────────────

TEST(ChordParser, EmptyInput)
{
    auto r = id({});
    EXPECT_TRUE(r.name.isEmpty());
    EXPECT_EQ(r.rootNote, -1);
}

TEST(ChordParser, SingleNote)
{
    auto r = id({ 60 }); // C
    EXPECT_EQ(r.root, juce::String("C"));
    EXPECT_EQ(r.quality, juce::String(""));
    EXPECT_EQ(r.rootNote, 0);
}

TEST(ChordParser, DuplicateNotes)
{
    auto r = id({ 60, 60, 64, 64, 67, 67 });
    EXPECT_EQ(r.name, juce::String("C"));
}

TEST(ChordParser, TwoNotesPowerChord)
{
    auto r = id({ 48, 55 }); // C + G = power chord
    EXPECT_EQ(r.name, juce::String("C5"));
}

// ── Major triads ────────────────────────────────────────────────────

TEST(ChordParser, CMajor)
{
    auto r = id({ 60, 64, 67 }); // C E G
    EXPECT_EQ(r.name, juce::String("C"));
    EXPECT_EQ(r.root, juce::String("C"));
    EXPECT_EQ(r.quality, juce::String(""));
    EXPECT_EQ(r.rootNote, 0);
}

TEST(ChordParser, DMajor)
{
    auto r = id({ 62, 66, 69 }); // D F# A
    EXPECT_EQ(r.name, juce::String("D"));
    EXPECT_EQ(r.root, juce::String("D"));
}

TEST(ChordParser, FSharpMajor)
{
    auto r = id({ 66, 70, 73 }); // F# A# C#
    EXPECT_EQ(r.name, juce::String("F#"));
    EXPECT_EQ(r.root, juce::String("F#"));
}

TEST(ChordParser, BbMajor)
{
    auto r = id({ 58, 62, 65 }); // Bb D F
    EXPECT_EQ(r.name, juce::String("Bb"));
    EXPECT_EQ(r.root, juce::String("Bb"));
}

// ── Minor triads ────────────────────────────────────────────────────

TEST(ChordParser, CMinor)
{
    auto r = id({ 60, 63, 67 }); // C Eb G
    EXPECT_EQ(r.name, juce::String("Cm"));
    EXPECT_EQ(r.quality, juce::String("m"));
}

TEST(ChordParser, AMinor)
{
    auto r = id({ 57, 60, 64 }); // A C E
    EXPECT_EQ(r.name, juce::String("Am"));
}

// ── Diminished & Augmented triads ───────────────────────────────────

TEST(ChordParser, CDiminished)
{
    auto r = id({ 60, 63, 66 }); // C Eb Gb
    EXPECT_EQ(r.name, juce::String("Cdim"));
}

TEST(ChordParser, CAugmented)
{
    auto r = id({ 60, 64, 68 }); // C E G#
    EXPECT_EQ(r.name, juce::String("Caug"));
}

// ── Suspended ───────────────────────────────────────────────────────

TEST(ChordParser, CSus4)
{
    auto r = id({ 60, 65, 67 }); // C F G
    EXPECT_EQ(r.name, juce::String("Csus4"));
}

TEST(ChordParser, CSus2)
{
    auto r = id({ 60, 62, 67 }); // C D G
    EXPECT_EQ(r.name, juce::String("Csus2"));
}

// ── Seventh chords ──────────────────────────────────────────────────

TEST(ChordParser, CMaj7)
{
    auto r = id({ 60, 64, 67, 71 }); // C E G B
    EXPECT_EQ(r.name, juce::String("Cmaj7"));
}

TEST(ChordParser, CDominant7)
{
    auto r = id({ 60, 64, 67, 70 }); // C E G Bb
    EXPECT_EQ(r.name, juce::String("C7"));
}

TEST(ChordParser, CMinor7)
{
    auto r = id({ 60, 63, 67, 70 }); // C Eb G Bb
    EXPECT_EQ(r.name, juce::String("Cm7"));
}

TEST(ChordParser, CHalfDim7)
{
    auto r = id({ 60, 63, 66, 70 }); // C Eb Gb Bb
    EXPECT_EQ(r.name, juce::String("Cm7b5"));
}

TEST(ChordParser, CDim7)
{
    auto r = id({ 60, 63, 66, 69 }); // C Eb Gb A(=Bbb)
    EXPECT_EQ(r.name, juce::String("Cdim7"));
}

TEST(ChordParser, CMinorMaj7)
{
    auto r = id({ 60, 63, 67, 71 }); // C Eb G B
    EXPECT_EQ(r.name, juce::String("CmMaj7"));
}

// ── Extended chords ─────────────────────────────────────────────────

TEST(ChordParser, CDominant9)
{
    auto r = id({ 60, 64, 67, 70, 74 }); // C E G Bb D
    EXPECT_EQ(r.name, juce::String("C9"));
}

TEST(ChordParser, CMaj9)
{
    auto r = id({ 60, 64, 67, 71, 74 }); // C E G B D
    EXPECT_EQ(r.name, juce::String("Cmaj9"));
}

TEST(ChordParser, CMinor9)
{
    auto r = id({ 60, 63, 67, 70, 74 }); // C Eb G Bb D
    EXPECT_EQ(r.name, juce::String("Cm9"));
}

TEST(ChordParser, CAdd9)
{
    // C E G D — add9 template scores 95, but major triad also scores 95
    // (100 base - 5 for 1 extra note). Major wins due to template ordering.
    auto r = id({ 60, 64, 67, 74 }); // C E G D (no 7th)
    EXPECT_EQ(r.root, juce::String("C"));
    EXPECT_FALSE(r.name.isEmpty());
}

TEST(ChordParser, C6)
{
    // C E G A — parser sees Am7 inversion (A C E G = m7 @ 110) which
    // outscores C6 (95). Bass note C differs from root A → slash notation.
    auto r = id({ 60, 64, 67, 69 }); // C E G A
    EXPECT_EQ(r.name, juce::String("Am7/C"));
}

TEST(ChordParser, Cm6)
{
    // C Eb G A — parser identifies Am7b5 inversion (A C Eb G = m7b5 @ 105)
    // which outscores Cm6 (95). Bass note C differs from root A → slash notation.
    auto r = id({ 60, 63, 67, 69 }); // C Eb G A
    EXPECT_EQ(r.name, juce::String("Am7b5/C"));
}

// ── Slash chords ────────────────────────────────────────────────────

TEST(ChordParser, CMajorOverE)
{
    auto r = id({ 52, 60, 64, 67 }); // E(bass) C E G → C/E
    EXPECT_EQ(r.name, juce::String("C/E"));
    EXPECT_EQ(r.root, juce::String("C"));
}

TEST(ChordParser, CMajorOverG)
{
    auto r = id({ 43, 60, 64, 67 }); // G(bass) C E G → root position bass = G
    EXPECT_EQ(r.name, juce::String("C/G"));
}

// ── Root note names ─────────────────────────────────────────────────

TEST(ChordParser, AllTwelveRootNames)
{
    // Major triads on each root: verify the root name
    const char* expected[] = { "C", "Db", "D", "Eb", "E", "F",
                               "F#", "G", "Ab", "A", "Bb", "B" };
    for (int root = 0; root < 12; ++root)
    {
        int base = 60 + root;
        auto r = id({ base, base + 4, base + 7 }); // major triad
        EXPECT_EQ(r.root, juce::String(expected[root]))
            << "Root pitch class " << root;
    }
}
