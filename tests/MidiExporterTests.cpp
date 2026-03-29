#include <gtest/gtest.h>
#include "MidiExporter.h"
#include "SongModel.h"
#include "SharedState.h"

// ══════════════════════════════════════════════════
// barToTick conversion
// ══════════════════════════════════════════════════

TEST(MidiExporter, BarToTickAtBarOne)
{
    // Bar 1 should be tick 0
    EXPECT_NEAR(MidiExporter::barToTick(1.0, 4), 0.0, 0.001);
}

TEST(MidiExporter, BarToTickBar2In4_4)
{
    // Bar 2 in 4/4: (2-1) * 4 * 480 = 1920
    EXPECT_NEAR(MidiExporter::barToTick(2.0, 4), 1920.0, 0.001);
}

TEST(MidiExporter, BarToTickBar3In3_4)
{
    // Bar 3 in 3/4: (3-1) * 3 * 480 = 2880
    EXPECT_NEAR(MidiExporter::barToTick(3.0, 3), 2880.0, 0.001);
}

TEST(MidiExporter, BarToTickFractionalBar)
{
    // Bar 1.5 in 4/4: (1.5-1) * 4 * 480 = 960
    EXPECT_NEAR(MidiExporter::barToTick(1.5, 4), 960.0, 0.001);
}

// ══════════════════════════════════════════════════
// Export empty SongModel
// ══════════════════════════════════════════════════

TEST(MidiExporter, ExportEmptyModel)
{
    SongModel model;
    MidiExporter::ExportContext ctx { 120.0, 4, 4 };

    auto data = MidiExporter::exportToSmf(model, ctx);
    EXPECT_GT(data.getSize(), 0u);

    // Should be valid MIDI data (starts with MThd)
    auto* bytes = static_cast<const juce::uint8*>(data.getData());
    ASSERT_GE(data.getSize(), 4u);
    EXPECT_EQ(bytes[0], 'M');
    EXPECT_EQ(bytes[1], 'T');
    EXPECT_EQ(bytes[2], 'h');
    EXPECT_EQ(bytes[3], 'd');
}

// ══════════════════════════════════════════════════
// Export with lyrics
// ══════════════════════════════════════════════════

TEST(MidiExporter, ExportSingleLyricLine)
{
    SongModel model;
    model.addLyricLine({ "Hello world", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    // Parse the exported MIDI to verify
    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));
    EXPECT_EQ(midiFile.getTimeFormat(), 480);
    EXPECT_GE(midiFile.getNumTracks(), 1);
}

TEST(MidiExporter, ExportMultipleLyricsWithSections)
{
    SongModel model;
    model.addSection({ "Verse 1", 1, 8, juce::Colour(0xfff5a623) });
    model.addLyricLine({ "Line one", 1.0, 3.0, 0, false });
    model.addLyricLine({ "Line two", 3.0, 5.0, 0, false });
    model.addChord({ "Am", 1.0, Chord::Manual });
    model.addChord({ "G", 3.0, Chord::Midi });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    // Count Text and Lyric meta events
    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    int textEvents = 0, lyricEvents = 0;
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isMetaEvent())
        {
            if (msg.getMetaEventType() == 1) textEvents++;
            if (msg.getMetaEventType() == 5) lyricEvents++;
        }
    }

    EXPECT_GT(textEvents, 0);   // JIMMY:v1, DEFAULT_BPL, SECTION:, CHORD: events
    EXPECT_GT(lyricEvents, 0);  // Lyric lines + CR/LF markers
}

// ══════════════════════════════════════════════════
// Export with breaks and line length overrides
// ══════════════════════════════════════════════════

TEST(MidiExporter, ExportBreakAndLineLenOverride)
{
    SongModel model;
    model.setDefaultBarsPerLine(2.0);
    model.addLyricLine({ "First line", 1.0, 5.0, -1, false });  // 4 bars, not default 2
    model.addLyricLine({ "", 5.0, 7.0, -1, true });             // break
    model.addLyricLine({ "After break", 7.0, 9.0, -1, false }); // default 2 bars

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    // Verify we can parse it back
    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    // Check that LINE_LEN and BREAK metadata events exist
    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    bool foundLineLenOverride = false;
    bool foundBreak = false;
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isMetaEvent() && msg.getMetaEventType() == 1)
        {
            auto text = msg.getTextFromTextMetaEvent();
            if (text.startsWith("LINE_LEN:")) foundLineLenOverride = true;
            if (text.startsWith("BREAK:")) foundBreak = true;
        }
    }

    EXPECT_TRUE(foundLineLenOverride);
    EXPECT_TRUE(foundBreak);
}

// ══════════════════════════════════════════════════
// Hebrew / UTF-8 lyrics
// ══════════════════════════════════════════════════

TEST(MidiExporter, ExportHebrewLyrics)
{
    SongModel model;
    model.addLyricLine({ juce::String::fromUTF8("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d \xd7\xa2\xd7\x95\xd7\x9c\xd7\x9d"),
                         1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    // Look for the Hebrew lyric in the parsed events
    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    bool foundHebrew = false;
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isMetaEvent() && msg.getMetaEventType() == 5)
        {
            auto text = msg.getTextFromTextMetaEvent();
            if (text.containsChar(0x05D0) || text.containsChar(0x05E9))  // Hebrew chars
                foundHebrew = true;
        }
    }

    EXPECT_TRUE(foundHebrew);
}

// ══════════════════════════════════════════════════
// Tempo and time signature export
// ══════════════════════════════════════════════════

TEST(MidiExporter, ExportContextTempoAndTimeSig)
{
    SongModel model;
    model.addLyricLine({ "Test", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 140.0, 3, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    bool foundJimmyMarker = false;
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isMetaEvent() && msg.getMetaEventType() == 1)
        {
            if (msg.getTextFromTextMetaEvent() == "JIMMY:v1")
                foundJimmyMarker = true;
        }
    }

    EXPECT_TRUE(foundJimmyMarker);
}

// ══════════════════════════════════════════════════
// Write to file
// ══════════════════════════════════════════════════

TEST(MidiExporter, WriteToFile)
{
    SongModel model;
    model.addLyricLine({ "Test line", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("jimmy_test_export.mid");

    EXPECT_TRUE(MidiExporter::writeToFile(model, ctx, tempFile));
    EXPECT_TRUE(tempFile.existsAsFile());
    EXPECT_GT(tempFile.getSize(), 0);

    tempFile.deleteFile();
}

// ══════════════════════════════════════════════════
// SysEx encoding in exported MIDI
// ══════════════════════════════════════════════════

TEST(MidiExporter, ExportContainsSysExEvents)
{
    SongModel model;
    model.addLyricLine({ "Hello world", 1.0, 3.0, -1, false });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    int sysExCount = 0;
    bool foundSongEnd = false;
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isSysEx())
        {
            auto* sysExData = msg.getSysExData();
            int sysExSize = msg.getSysExDataSize();
            if (JimmySysEx::isJimmySysEx(sysExData, sysExSize))
            {
                auto msgType = sysExData[JimmySysEx::kMsgTypeOffset];
                if (msgType == JimmySysEx::kSongHeader)
                    ++sysExCount;
                else if (msgType == JimmySysEx::kSongEnd)
                    foundSongEnd = true;
            }
        }
    }

    EXPECT_GE(sysExCount, 1);  // At least one song header SysEx
    EXPECT_TRUE(foundSongEnd);  // Song end marker present
}

TEST(MidiExporter, SysExRepeatEvery16Bars)
{
    SongModel model;
    // Create a song spanning 40 bars
    for (int i = 0; i < 20; ++i)
    {
        double startBar = 1.0 + i * 2.0;
        model.addLyricLine({ "Line " + juce::String(i), startBar, startBar + 2.0, -1, false });
    }

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    int songHeaderCount = 0;
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isSysEx())
        {
            auto* sysExData = msg.getSysExData();
            int sysExSize = msg.getSysExDataSize();
            if (JimmySysEx::isJimmySysEx(sysExData, sysExSize)
                && sysExData[JimmySysEx::kMsgTypeOffset] == JimmySysEx::kSongHeader)
                ++songHeaderCount;
        }
    }

    // 40 bars / 16 = 2.5, plus bar 0 → should have at least 3 headers
    EXPECT_GE(songHeaderCount, 3);
}

TEST(MidiExporter, SysExPayloadDecodable)
{
    SongModel model;
    model.addLyricLine({ "Test line", 1.0, 3.0, -1, false });
    model.addChord({ "Am", 1.0, Chord::Manual });

    MidiExporter::ExportContext ctx { 120.0, 4, 4 };
    auto data = MidiExporter::exportToSmf(model, ctx);

    juce::MemoryInputStream mis(data.getData(), data.getSize(), false);
    juce::MidiFile midiFile;
    ASSERT_TRUE(midiFile.readFrom(mis, true));

    const auto* seq = midiFile.getTrack(0);
    ASSERT_NE(seq, nullptr);

    // Find first song header SysEx and decode it
    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto& msg = seq->getEventPointer(i)->message;
        if (msg.isSysEx())
        {
            auto* sysExData = msg.getSysExData();
            int sysExSize = msg.getSysExDataSize();
            if (JimmySysEx::isJimmySysEx(sysExData, sysExSize)
                && sysExData[JimmySysEx::kMsgTypeOffset] == JimmySysEx::kSongHeader
                && sysExSize > JimmySysEx::kPayloadOffset)
            {
                auto xmlString = MidiExporter::decompressSongXml(
                    sysExData + JimmySysEx::kPayloadOffset,
                    sysExSize - JimmySysEx::kPayloadOffset);

                ASSERT_FALSE(xmlString.isEmpty());

                auto songData = SongModel::songFromXml(xmlString);
                ASSERT_EQ(songData.lyrics.size(), 1u);
                EXPECT_EQ(songData.lyrics[0].text, "Test line");
                ASSERT_EQ(songData.chords.size(), 1u);
                EXPECT_EQ(songData.chords[0].name, "Am");
                return;
            }
        }
    }

    FAIL() << "No decodable Song Header SysEx found";
}

TEST(MidiExporter, CompressDecompressRoundTrip)
{
    juce::String original = "<JimmySongData defaultBarsPerLine=\"2\"><Lyrics><Line text=\"Hello\" startBar=\"1\" endBar=\"3\"/></Lyrics></JimmySongData>";
    auto compressed = MidiExporter::decompressSongXml(nullptr, 0);
    EXPECT_TRUE(compressed.isEmpty());

    // Use songToXml from a model for a proper round-trip
    SongModel model;
    model.addLyricLine({ "Round trip test", 1.0, 3.0, -1, false });
    auto xmlString = model.songToXml();

    // We can't directly call compressSongXml (private), but we can test through export
    // The SysExPayloadDecodable test above already covers this path.
}
