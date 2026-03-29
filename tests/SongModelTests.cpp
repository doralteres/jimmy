#include <gtest/gtest.h>
#include "SongModel.h"

// ── Chord CRUD ──────────────────────────────────────────────────────

TEST(SongModel, AddChordSorted)
{
    SongModel m;
    m.addChord({ "G", 5.0, Chord::Manual });
    m.addChord({ "C", 1.0, Chord::Manual });
    m.addChord({ "F", 3.0, Chord::Manual });

    auto chords = m.getChords();
    ASSERT_EQ(chords.size(), 3u);
    EXPECT_DOUBLE_EQ(chords[0].barPosition, 1.0);
    EXPECT_DOUBLE_EQ(chords[1].barPosition, 3.0);
    EXPECT_DOUBLE_EQ(chords[2].barPosition, 5.0);
}

TEST(SongModel, RemoveChord)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    m.addChord({ "G", 5.0, Chord::Manual });
    m.removeChord(0);
    auto chords = m.getChords();
    ASSERT_EQ(chords.size(), 1u);
    EXPECT_EQ(chords[0].name, juce::String("G"));
}

TEST(SongModel, RemoveChordOutOfRange)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    m.removeChord(-1);
    m.removeChord(5);
    EXPECT_EQ(m.getChords().size(), 1u); // unchanged
}

TEST(SongModel, GetChordAtBeforeFirst)
{
    SongModel m;
    m.addChord({ "C", 3.0, Chord::Manual });
    EXPECT_TRUE(m.getChordAt(1.0).isEmpty());
}

TEST(SongModel, GetChordAtExact)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    m.addChord({ "G", 5.0, Chord::Manual });
    EXPECT_EQ(m.getChordAt(1.0), juce::String("C"));
    EXPECT_EQ(m.getChordAt(5.0), juce::String("G"));
}

TEST(SongModel, GetChordAtBetween)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    m.addChord({ "G", 5.0, Chord::Manual });
    EXPECT_EQ(m.getChordAt(3.0), juce::String("C"));
}

TEST(SongModel, GetChordAtAfterLast)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    EXPECT_EQ(m.getChordAt(100.0), juce::String("C"));
}

TEST(SongModel, GetChordAtEmpty)
{
    SongModel m;
    EXPECT_TRUE(m.getChordAt(1.0).isEmpty());
}

// ── MIDI chords ─────────────────────────────────────────────────────

TEST(SongModel, SetMidiChordInsert)
{
    SongModel m;
    m.setMidiChordAt(2.0, "Am");
    auto chords = m.getChords();
    ASSERT_EQ(chords.size(), 1u);
    EXPECT_EQ(chords[0].name, juce::String("Am"));
    EXPECT_EQ(chords[0].source, Chord::Midi);
}

TEST(SongModel, SetMidiChordReplacesNearby)
{
    SongModel m;
    m.setMidiChordAt(2.0, "Am");
    m.setMidiChordAt(2.005, "C");  // within 0.01 tolerance
    auto chords = m.getChords();
    ASSERT_EQ(chords.size(), 1u);
    EXPECT_EQ(chords[0].name, juce::String("C"));
}

TEST(SongModel, ClearMidiChordsLeavesManual)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    m.setMidiChordAt(3.0, "Dm");
    m.clearMidiChords();
    auto chords = m.getChords();
    ASSERT_EQ(chords.size(), 1u);
    EXPECT_EQ(chords[0].name, juce::String("C"));
    EXPECT_EQ(chords[0].source, Chord::Manual);
}

// ── Section CRUD ────────────────────────────────────────────────────

TEST(SongModel, AddSectionSorted)
{
    SongModel m;
    m.addSection({ "Chorus", 10, 20 });
    m.addSection({ "Verse", 1, 9 });
    auto sections = m.getSections();
    ASSERT_EQ(sections.size(), 2u);
    EXPECT_EQ(sections[0].name, juce::String("Verse"));
    EXPECT_EQ(sections[1].name, juce::String("Chorus"));
}

TEST(SongModel, GetSectionAtInside)
{
    SongModel m;
    m.addSection({ "Verse", 1, 8 });
    EXPECT_EQ(m.getSectionAt(4.0), juce::String("Verse"));
}

TEST(SongModel, GetSectionAtBoundary)
{
    SongModel m;
    m.addSection({ "Verse", 1, 8 });
    EXPECT_EQ(m.getSectionAt(1.0), juce::String("Verse"));
    EXPECT_EQ(m.getSectionAt(8.0), juce::String("Verse"));
}

TEST(SongModel, GetSectionAtOutside)
{
    SongModel m;
    m.addSection({ "Verse", 5, 10 });
    EXPECT_TRUE(m.getSectionAt(2.0).isEmpty());
    EXPECT_TRUE(m.getSectionAt(15.0).isEmpty());
}

TEST(SongModel, UpdateSection)
{
    SongModel m;
    m.addSection({ "Verse", 1, 8 });
    m.updateSection(0, { "Chorus", 1, 16 });
    auto sections = m.getSections();
    ASSERT_EQ(sections.size(), 1u);
    EXPECT_EQ(sections[0].name, juce::String("Chorus"));
    EXPECT_EQ(sections[0].endBar, 16);
}

TEST(SongModel, RemoveSection)
{
    SongModel m;
    m.addSection({ "Verse", 1, 8 });
    m.addSection({ "Chorus", 9, 16 });
    m.removeSection(0);
    auto sections = m.getSections();
    ASSERT_EQ(sections.size(), 1u);
    EXPECT_EQ(sections[0].name, juce::String("Chorus"));
}

// ── Lyrics CRUD ─────────────────────────────────────────────────────

TEST(SongModel, AddLyricLineSorted)
{
    SongModel m;
    m.addLyricLine({ "second", 5.0, 8.0 });
    m.addLyricLine({ "first", 1.0, 4.0 });
    auto lyrics = m.getLyrics();
    ASSERT_EQ(lyrics.size(), 2u);
    EXPECT_EQ(lyrics[0].text, juce::String("first"));
    EXPECT_EQ(lyrics[1].text, juce::String("second"));
}

TEST(SongModel, UpdateLyricLine)
{
    SongModel m;
    m.addLyricLine({ "original", 1.0, 4.0 });
    m.updateLyricLine(0, { "updated", 1.0, 4.0 });
    EXPECT_EQ(m.getLyrics()[0].text, juce::String("updated"));
}

TEST(SongModel, RemoveLyricLine)
{
    SongModel m;
    m.addLyricLine({ "first", 1.0, 4.0 });
    m.addLyricLine({ "second", 5.0, 8.0 });
    m.removeLyricLine(0);
    auto lyrics = m.getLyrics();
    ASSERT_EQ(lyrics.size(), 1u);
    EXPECT_EQ(lyrics[0].text, juce::String("second"));
}

TEST(SongModel, SetLyricsBulk)
{
    SongModel m;
    m.addLyricLine({ "old", 1.0, 2.0 });
    std::vector<LyricLine> newLines = {
        { "new1", 1.0, 3.0 },
        { "new2", 3.0, 5.0 },
    };
    m.setLyrics(newLines);
    auto lyrics = m.getLyrics();
    ASSERT_EQ(lyrics.size(), 2u);
    EXPECT_EQ(lyrics[0].text, juce::String("new1"));
}

// ── Settings ────────────────────────────────────────────────────────

TEST(SongModel, DefaultBarsPerLineRoundTrip)
{
    SongModel m;
    EXPECT_DOUBLE_EQ(m.getDefaultBarsPerLine(), 2.0);
    m.setDefaultBarsPerLine(4.0);
    EXPECT_DOUBLE_EQ(m.getDefaultBarsPerLine(), 4.0);
}

TEST(SongModel, DefaultBarsPerLineRejectsZero)
{
    SongModel m;
    m.setDefaultBarsPerLine(0.0);
    EXPECT_DOUBLE_EQ(m.getDefaultBarsPerLine(), 1.0);
}

TEST(SongModel, DefaultBarsPerLineRejectsNegative)
{
    SongModel m;
    m.setDefaultBarsPerLine(-5.0);
    EXPECT_DOUBLE_EQ(m.getDefaultBarsPerLine(), 1.0);
}

// ── XML round-trip ──────────────────────────────────────────────────

TEST(SongModel, XmlRoundTrip)
{
    SongModel original;
    original.setDefaultBarsPerLine(3.0);
    original.addChord({ "C", 1.0, Chord::Manual });
    original.addChord({ "Am", 5.0, Chord::Midi });
    original.addSection({ "Verse", 1, 8, juce::Colour(0xfff5a623) });
    original.addSection({ "Chorus", 9, 16, juce::Colour(0xff9b6dff) });
    original.addLyricLine({ "Hello world", 1.0, 3.0, 0, false });
    original.addLyricLine({ "", 3.0, 5.0, 0, true }); // break line

    std::unique_ptr<juce::XmlElement> xml(original.toXml());
    ASSERT_NE(xml, nullptr);
    EXPECT_TRUE(xml->hasTagName("JimmySong"));

    SongModel restored;
    restored.fromXml(xml.get());

    EXPECT_DOUBLE_EQ(restored.getDefaultBarsPerLine(), 3.0);

    auto chords = restored.getChords();
    ASSERT_EQ(chords.size(), 2u);
    EXPECT_EQ(chords[0].name, juce::String("C"));
    EXPECT_DOUBLE_EQ(chords[0].barPosition, 1.0);
    EXPECT_EQ(chords[0].source, Chord::Manual);
    EXPECT_EQ(chords[1].name, juce::String("Am"));
    EXPECT_EQ(chords[1].source, Chord::Midi);

    auto sections = restored.getSections();
    ASSERT_EQ(sections.size(), 2u);
    EXPECT_EQ(sections[0].name, juce::String("Verse"));
    EXPECT_EQ(sections[0].startBar, 1);
    EXPECT_EQ(sections[0].endBar, 8);
    EXPECT_EQ(sections[1].name, juce::String("Chorus"));

    auto lyrics = restored.getLyrics();
    ASSERT_EQ(lyrics.size(), 2u);
    EXPECT_EQ(lyrics[0].text, juce::String("Hello world"));
    EXPECT_DOUBLE_EQ(lyrics[0].startBar, 1.0);
    EXPECT_EQ(lyrics[0].sectionIndex, 0);
    EXPECT_FALSE(lyrics[0].isBreak);
    EXPECT_TRUE(lyrics[1].isBreak);
}

TEST(SongModel, XmlFromNullPtr)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    m.fromXml(nullptr);
    // Should not crash, data unchanged
    EXPECT_EQ(m.getChords().size(), 1u);
}

TEST(SongModel, XmlFromWrongTag)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    juce::XmlElement wrongTag("WrongTag");
    m.fromXml(&wrongTag);
    // Should not crash, data unchanged
    EXPECT_EQ(m.getChords().size(), 1u);
}

TEST(SongModel, XmlFromEmptyElement)
{
    SongModel m;
    m.addChord({ "C", 1.0, Chord::Manual });
    juce::XmlElement empty("JimmySong");
    m.fromXml(&empty);
    // Should clear data since the XML is valid but has no children
    EXPECT_TRUE(m.getChords().empty());
    EXPECT_TRUE(m.getSections().empty());
    EXPECT_TRUE(m.getLyrics().empty());
}

TEST(SongModel, XmlHebrewLyricsRoundTrip)
{
    SongModel m;
    m.addLyricLine({ juce::String::fromUTF8("שלום עולם"), 1.0, 3.0 });

    std::unique_ptr<juce::XmlElement> xml(m.toXml());
    SongModel restored;
    restored.fromXml(xml.get());

    auto lyrics = restored.getLyrics();
    ASSERT_EQ(lyrics.size(), 1u);
    EXPECT_EQ(lyrics[0].text, juce::String::fromUTF8("שלום עולם"));
}

// ── LiveSourceMode ──────────────────────────────────────────────────

TEST(SongModel, LiveSourceModeDefault)
{
    SongModel m;
    EXPECT_EQ(m.getLiveSourceMode(), LiveSourceMode::LiveInput);
}

TEST(SongModel, LiveSourceModeSetGet)
{
    SongModel m;
    m.setLiveSourceMode(LiveSourceMode::FromEditor);
    EXPECT_EQ(m.getLiveSourceMode(), LiveSourceMode::FromEditor);
    m.setLiveSourceMode(LiveSourceMode::LiveInput);
    EXPECT_EQ(m.getLiveSourceMode(), LiveSourceMode::LiveInput);
}

TEST(SongModel, LiveSourceModeXmlRoundTrip)
{
    SongModel m;
    m.setLiveSourceMode(LiveSourceMode::FromEditor);
    std::unique_ptr<juce::XmlElement> xml(m.toXml());

    SongModel restored;
    restored.fromXml(xml.get());
    EXPECT_EQ(restored.getLiveSourceMode(), LiveSourceMode::FromEditor);
}

TEST(SongModel, LiveSourceModeXmlDefaultsToLiveInput)
{
    // An XML without liveSource attribute should default to LiveInput
    juce::XmlElement root("JimmySong");
    SongModel m;
    m.setLiveSourceMode(LiveSourceMode::FromEditor);
    m.fromXml(&root);
    EXPECT_EQ(m.getLiveSourceMode(), LiveSourceMode::LiveInput);
}

// ── Clip operations ─────────────────────────────────────────────────

static SongClip makeClip(double startBar, double endBar)
{
    SongClip c;
    c.absoluteStartBar   = startBar;
    c.absoluteEndBar     = endBar;
    c.defaultBarsPerLine = 2.0;
    c.bpm                = 120.0;
    c.timeSigNum         = 4;
    c.timeSigDen         = 4;
    return c;
}

TEST(SongModel, AddClipSorted)
{
    SongModel m;
    m.addClip(makeClip(10.0, 20.0));
    m.addClip(makeClip(1.0, 9.0));
    auto clips = m.getClips();
    ASSERT_EQ(clips.size(), 2u);
    EXPECT_DOUBLE_EQ(clips[0].absoluteStartBar, 1.0);
    EXPECT_DOUBLE_EQ(clips[1].absoluteStartBar, 10.0);
}

TEST(SongModel, RemoveClip)
{
    SongModel m;
    m.addClip(makeClip(1.0, 9.0));
    m.addClip(makeClip(10.0, 20.0));
    m.removeClip(0);
    auto clips = m.getClips();
    ASSERT_EQ(clips.size(), 1u);
    EXPECT_DOUBLE_EQ(clips[0].absoluteStartBar, 10.0);
}

TEST(SongModel, ClearClips)
{
    SongModel m;
    m.addClip(makeClip(1.0, 9.0));
    m.clearClips();
    EXPECT_TRUE(m.getClips().empty());
}

TEST(SongModel, GetActiveClipInRange)
{
    SongModel m;
    m.addClip(makeClip(1.0, 10.0));
    m.addClip(makeClip(15.0, 25.0));

    EXPECT_EQ(m.getActiveClipIndex(5.0), 0);
    EXPECT_EQ(m.getActiveClipIndex(20.0), 1);
}

TEST(SongModel, GetActiveClipGap)
{
    SongModel m;
    m.addClip(makeClip(1.0, 10.0));
    m.addClip(makeClip(15.0, 25.0));

    // In the gap between clips
    EXPECT_EQ(m.getActiveClipIndex(12.0), -1);
}

TEST(SongModel, GetActiveClipBoundary)
{
    SongModel m;
    m.addClip(makeClip(1.0, 10.0));

    // Start boundary is inclusive
    EXPECT_EQ(m.getActiveClipIndex(1.0), 0);
    // End boundary is exclusive
    EXPECT_EQ(m.getActiveClipIndex(10.0), -1);
}

TEST(SongModel, GetActiveClipEmpty)
{
    SongModel m;
    EXPECT_EQ(m.getActiveClipIndex(5.0), -1);
}

TEST(SongModel, ClipXmlRoundTrip)
{
    SongModel m;
    SongClip clip;
    clip.absoluteStartBar = 5.0;
    clip.absoluteEndBar = 15.0;
    clip.defaultBarsPerLine = 3.0;
    clip.bpm = 140.0;
    clip.timeSigNum = 3;
    clip.timeSigDen = 4;
    clip.lyrics.push_back({ "Clip lyric", 1.0, 4.0, -1, false });
    clip.sections.push_back({ "Clip Section", 1, 8, juce::Colour(0xff4caf50) });
    clip.chords.push_back({ "Dm", 1.0, Chord::Manual });
    m.addClip(clip);

    std::unique_ptr<juce::XmlElement> xml(m.toXml());
    SongModel restored;
    restored.fromXml(xml.get());

    auto clips = restored.getClips();
    ASSERT_EQ(clips.size(), 1u);
    EXPECT_DOUBLE_EQ(clips[0].absoluteStartBar, 5.0);
    EXPECT_DOUBLE_EQ(clips[0].absoluteEndBar, 15.0);
    EXPECT_DOUBLE_EQ(clips[0].defaultBarsPerLine, 3.0);
    EXPECT_NEAR(clips[0].bpm, 140.0, 0.1);
    EXPECT_EQ(clips[0].timeSigNum, 3);
    EXPECT_EQ(clips[0].timeSigDen, 4);
    ASSERT_EQ(clips[0].lyrics.size(), 1u);
    EXPECT_EQ(clips[0].lyrics[0].text, juce::String("Clip lyric"));
    ASSERT_EQ(clips[0].sections.size(), 1u);
    EXPECT_EQ(clips[0].sections[0].name, juce::String("Clip Section"));
    ASSERT_EQ(clips[0].chords.size(), 1u);
    EXPECT_EQ(clips[0].chords[0].name, juce::String("Dm"));
}

// ── clearAllChords ──────────────────────────────────────────────────

TEST(SongModel, ClearAllChordsRemovesBothSources)
{
    SongModel m;
    m.addChord({ "Am", 1.0, Chord::Manual });
    m.addChord({ "G", 3.0, Chord::Midi });
    m.addChord({ "Dm", 5.0, Chord::Manual });
    EXPECT_EQ(m.getChords().size(), 3u);

    m.clearAllChords();
    EXPECT_TRUE(m.getChords().empty());
}

TEST(SongModel, ClearAllChordsOnEmptyModel)
{
    SongModel m;
    m.clearAllChords();  // should not crash
    EXPECT_TRUE(m.getChords().empty());
}

// ── songToXml / songFromXml ─────────────────────────────────────────

TEST(SongModel, SongToXmlRoundTrip)
{
    SongModel m;
    m.setDefaultBarsPerLine(3.0);
    m.addSection({ "Verse", 1, 8, juce::Colour(0xfff5a623) });
    m.addLyricLine({ "Hello world", 1.0, 4.0, 0, false });
    m.addLyricLine({ "", 4.0, 6.0, -1, true });  // break
    m.addLyricLine({ "Second line", 6.0, 9.0, 0, false });
    m.addChord({ "Am", 1.0, Chord::Manual });
    m.addChord({ "G", 6.0, Chord::Midi });

    auto xmlString = m.songToXml();
    EXPECT_FALSE(xmlString.isEmpty());

    auto result = SongModel::songFromXml(xmlString);
    EXPECT_NEAR(result.defaultBarsPerLine, 3.0, 0.01);

    ASSERT_EQ(result.lyrics.size(), 3u);
    EXPECT_EQ(result.lyrics[0].text, "Hello world");
    EXPECT_NEAR(result.lyrics[0].startBar, 1.0, 0.01);
    EXPECT_TRUE(result.lyrics[1].isBreak);
    EXPECT_EQ(result.lyrics[2].text, "Second line");

    ASSERT_EQ(result.sections.size(), 1u);
    EXPECT_EQ(result.sections[0].name, "Verse");
    EXPECT_EQ(result.sections[0].startBar, 1);
    EXPECT_EQ(result.sections[0].endBar, 8);

    ASSERT_EQ(result.chords.size(), 2u);
    EXPECT_EQ(result.chords[0].name, "Am");
    EXPECT_EQ(result.chords[0].source, Chord::Manual);
    EXPECT_EQ(result.chords[1].name, "G");
    EXPECT_EQ(result.chords[1].source, Chord::Midi);
}

TEST(SongModel, SongFromXmlInvalidReturnsEmpty)
{
    auto result = SongModel::songFromXml("not xml at all");
    EXPECT_TRUE(result.lyrics.empty());
    EXPECT_TRUE(result.chords.empty());
    EXPECT_TRUE(result.sections.empty());
}

TEST(SongModel, SongFromXmlWrongTagReturnsEmpty)
{
    auto result = SongModel::songFromXml("<WrongTag/>");
    EXPECT_TRUE(result.lyrics.empty());
}

TEST(SongModel, LoadSongDataReplacesExistingData)
{
    SongModel m;
    m.addLyricLine({ "Old line", 1.0, 3.0, -1, false });
    m.addChord({ "C", 1.0, Chord::Manual });

    SongModel::SongData newData;
    newData.lyrics.push_back({ "New line", 5.0, 7.0, -1, false });
    newData.chords.push_back({ "Dm", 5.0, Chord::Midi });
    newData.defaultBarsPerLine = 4.0;

    m.loadSongData(newData);

    auto lyrics = m.getLyrics();
    ASSERT_EQ(lyrics.size(), 1u);
    EXPECT_EQ(lyrics[0].text, "New line");

    auto chords = m.getChords();
    ASSERT_EQ(chords.size(), 1u);
    EXPECT_EQ(chords[0].name, "Dm");

    EXPECT_NEAR(m.getDefaultBarsPerLine(), 4.0, 0.01);
}

TEST(SongModel, SongToXmlHebrewRoundTrip)
{
    SongModel m;
    juce::String hebrew = juce::String::fromUTF8("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d");
    m.addLyricLine({ hebrew, 1.0, 3.0, -1, false });

    auto xmlString = m.songToXml();
    auto result = SongModel::songFromXml(xmlString);

    ASSERT_EQ(result.lyrics.size(), 1u);
    EXPECT_EQ(result.lyrics[0].text, hebrew);
}
