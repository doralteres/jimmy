#include <gtest/gtest.h>
#include "MidiImporter.h"
#include "MidiExporter.h"
#include "SongModel.h"

// Helper: export a SongModel to a temp file and return the File.
static juce::File exportToTempFile(const SongModel& model, const MidiExporter::ExportContext& ctx)
{
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("jimmy_importer_test.mid");
    MidiExporter::writeToFile(model, ctx, tempFile);
    return tempFile;
}

// ══════════════════════════════════════════════════
// Format detection
// ══════════════════════════════════════════════════

TEST(MidiImporter, DetectFormatJimmyFull)
{
    SongModel model;
    model.addLyricLine({ "Test", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    EXPECT_EQ(MidiImporter::detectFormat(file), MidiImporter::ImportFormat::JimmyFull);
    file.deleteFile();
}

TEST(MidiImporter, DetectFormatNonExistentFile)
{
    juce::File bogus("/tmp/nonexistent_jimmy_test_12345.mid");
    EXPECT_EQ(MidiImporter::detectFormat(bogus), MidiImporter::ImportFormat::Unknown);
}

// ══════════════════════════════════════════════════
// Full import: basic round-trip
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripSingleLyricLine)
{
    SongModel model;
    model.setDefaultBarsPerLine(2.0);
    model.addLyricLine({ "Hello world", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    EXPECT_NEAR(result.defaultBarsPerLine, 2.0, 0.01);
    ASSERT_EQ(result.lyrics.size(), 1u);
    EXPECT_EQ(result.lyrics[0].text, "Hello world");
    EXPECT_NEAR(result.lyrics[0].startBar, 1.0, 0.1);

    file.deleteFile();
}

TEST(MidiImporter, RoundTripMultipleLyricsAndChords)
{
    SongModel model;
    model.setDefaultBarsPerLine(2.0);
    model.addSection({ "Verse", 1, 8, juce::Colour(0xfff5a623) });
    model.addLyricLine({ "Line one", 1.0, 3.0, 0, false });
    model.addLyricLine({ "Line two", 3.0, 5.0, 0, false });
    model.addChord({ "Am", 1.0, Chord::Manual });
    model.addChord({ "G", 3.0, Chord::Midi });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);

    // Lyrics
    ASSERT_EQ(result.lyrics.size(), 2u);
    EXPECT_EQ(result.lyrics[0].text, "Line one");
    EXPECT_EQ(result.lyrics[1].text, "Line two");

    // Chords
    ASSERT_EQ(result.chords.size(), 2u);
    EXPECT_EQ(result.chords[0].name, "Am");
    EXPECT_EQ(result.chords[0].source, Chord::Manual);
    EXPECT_EQ(result.chords[1].name, "G");
    EXPECT_EQ(result.chords[1].source, Chord::Midi);

    // Sections
    ASSERT_EQ(result.sections.size(), 1u);
    EXPECT_EQ(result.sections[0].name, "Verse");

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Round-trip with line length overrides and breaks
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripLineLenOverride)
{
    SongModel model;
    model.setDefaultBarsPerLine(2.0);
    model.addLyricLine({ "Long line", 1.0, 5.0, -1, false }); // 4 bars (not default 2)
    model.addLyricLine({ "Normal", 5.0, 7.0, -1, false });    // default 2 bars

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lyrics.size(), 2u);

    // First line should have 4-bar length override
    double firstLen = result.lyrics[0].endBar - result.lyrics[0].startBar;
    EXPECT_NEAR(firstLen, 4.0, 0.1);

    // Second line should have default 2-bar length
    double secondLen = result.lyrics[1].endBar - result.lyrics[1].startBar;
    EXPECT_NEAR(secondLen, 2.0, 0.1);

    file.deleteFile();
}

TEST(MidiImporter, RoundTripBreaks)
{
    SongModel model;
    model.setDefaultBarsPerLine(2.0);
    model.addLyricLine({ "Before break", 1.0, 3.0, -1, false });
    model.addLyricLine({ "", 3.0, 5.0, -1, true });   // break
    model.addLyricLine({ "After break", 5.0, 7.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lyrics.size(), 3u);

    EXPECT_FALSE(result.lyrics[0].isBreak);
    EXPECT_EQ(result.lyrics[0].text, "Before break");

    EXPECT_TRUE(result.lyrics[1].isBreak);

    EXPECT_FALSE(result.lyrics[2].isBreak);
    EXPECT_EQ(result.lyrics[2].text, "After break");

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Hebrew / RTL round-trip
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripHebrewLyrics)
{
    SongModel model;
    juce::String hebrew = juce::String::fromUTF8("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d \xd7\xa2\xd7\x95\xd7\x9c\xd7\x9d");
    model.addLyricLine({ hebrew, 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lyrics.size(), 1u);
    EXPECT_EQ(result.lyrics[0].text, hebrew);

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Tempo and time signature round-trip
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripTempoAndTimeSig)
{
    SongModel model;
    model.addLyricLine({ "Test", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 140.0, 3, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    EXPECT_NEAR(result.bpm, 140.0, 1.0);
    EXPECT_EQ(result.timeSigNum, 3);
    EXPECT_EQ(result.timeSigDen, 4);

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Section attributes round-trip
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripSectionAttributes)
{
    SongModel model;
    juce::Colour sectionColour(0xff9b6dff);
    model.addSection({ "Chorus", 3, 10, sectionColour });
    model.addLyricLine({ "Chorus line", 3.0, 5.0, 0, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.sections.size(), 1u);
    EXPECT_EQ(result.sections[0].name, "Chorus");
    EXPECT_EQ(result.sections[0].startBar, 3);
    EXPECT_EQ(result.sections[0].endBar, 10);
    EXPECT_EQ(result.sections[0].colour, sectionColour);

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Error handling
// ══════════════════════════════════════════════════

TEST(MidiImporter, ImportNonExistentFile)
{
    juce::File bogus("/tmp/does_not_exist_jimmy_42.mid");
    auto result = MidiImporter::importFull(bogus);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.isNotEmpty());
}

TEST(MidiImporter, ImportChordsOnlyFallback)
{
    // A Jimmy-exported file should be detected as JimmyFull, not ChordOnly
    SongModel model;
    model.addLyricLine({ "Test", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    EXPECT_EQ(MidiImporter::detectFormat(file), MidiImporter::ImportFormat::JimmyFull);

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Multi-section round-trip
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripMultipleSections)
{
    SongModel model;
    model.addSection({ "Verse", 1, 8, juce::Colour(0xfff5a623) });
    model.addSection({ "Chorus", 9, 16, juce::Colour(0xff4caf50) });
    model.addLyricLine({ "Verse line 1", 1.0, 3.0, 0, false });
    model.addLyricLine({ "Verse line 2", 3.0, 5.0, 0, false });
    model.addLyricLine({ "Chorus line 1", 9.0, 11.0, 1, false });
    model.addChord({ "Am", 1.0, Chord::Midi });
    model.addChord({ "C", 9.0, Chord::Manual });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.lyrics.size(), 3u);
    EXPECT_EQ(result.sections.size(), 2u);
    EXPECT_EQ(result.chords.size(), 2u);

    EXPECT_EQ(result.sections[0].name, "Verse");
    EXPECT_EQ(result.sections[1].name, "Chorus");
    EXPECT_EQ(result.chords[0].name, "Am");
    EXPECT_EQ(result.chords[1].name, "C");

    file.deleteFile();
}

// ══════════════════════════════════════════════════
// Default BPL round-trip
// ══════════════════════════════════════════════════

TEST(MidiImporter, RoundTripDefaultBarsPerLine)
{
    SongModel model;
    model.setDefaultBarsPerLine(3.5);
    model.addLyricLine({ "Test", 1.0, 4.5, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto file = exportToTempFile(model, ctx);

    auto result = MidiImporter::importFull(file);
    ASSERT_TRUE(result.success);
    EXPECT_NEAR(result.defaultBarsPerLine, 3.5, 0.01);

    file.deleteFile();
}
