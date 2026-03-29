#include <gtest/gtest.h>
#include "SharedState.h"

// ═══════════════════════════════════════════════════════════════════
// ChordEvent
// ═══════════════════════════════════════════════════════════════════

TEST(ChordEvent, SetGetNameRoundTrip)
{
    ChordEvent e;
    e.setName("Cmaj7");
    EXPECT_EQ(e.getName(), juce::String("Cmaj7"));
}

TEST(ChordEvent, EmptyName)
{
    ChordEvent e;
    e.setName("");
    EXPECT_TRUE(e.getName().isEmpty());
}

TEST(ChordEvent, NameExactly31Chars)
{
    ChordEvent e;
    juce::String s31(std::string(31, 'A').c_str());
    e.setName(s31);
    EXPECT_EQ(e.getName().length(), 31);
}

TEST(ChordEvent, NameTruncatedAt31)
{
    ChordEvent e;
    juce::String s40(std::string(40, 'B').c_str());
    e.setName(s40);
    EXPECT_EQ(e.getName().length(), 31);
}

TEST(ChordEvent, BarPositionStored)
{
    ChordEvent e;
    e.barPosition = 42.5;
    EXPECT_DOUBLE_EQ(e.barPosition, 42.5);
}

// ═══════════════════════════════════════════════════════════════════
// ChordEventFifo
// ═══════════════════════════════════════════════════════════════════

TEST(ChordEventFifo, PushPopSingleItem)
{
    ChordEventFifo fifo;
    EXPECT_TRUE(fifo.push(1.0, "C"));

    ChordEvent out;
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_EQ(out.getName(), juce::String("C"));
    EXPECT_DOUBLE_EQ(out.barPosition, 1.0);
}

TEST(ChordEventFifo, PopFromEmptyReturnsFalse)
{
    ChordEventFifo fifo;
    ChordEvent out;
    EXPECT_FALSE(fifo.pop(out));
}

TEST(ChordEventFifo, FifoOrdering)
{
    ChordEventFifo fifo;
    fifo.push(1.0, "C");
    fifo.push(2.0, "D");
    fifo.push(3.0, "E");

    ChordEvent out;
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_EQ(out.getName(), juce::String("C"));
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_EQ(out.getName(), juce::String("D"));
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_EQ(out.getName(), juce::String("E"));
    EXPECT_FALSE(fifo.pop(out));
}

TEST(ChordEventFifo, FullBufferReturnsFalse)
{
    ChordEventFifo fifo;
    // Capacity is 256, but SPSC ring buffer with one sentinel → 255 usable
    int pushed = 0;
    for (int i = 0; i < 300; ++i)
    {
        if (fifo.push(static_cast<double>(i), "X"))
            ++pushed;
    }
    EXPECT_EQ(pushed, ChordEventFifo::kCapacity - 1);
}

TEST(ChordEventFifo, PushAfterDrain)
{
    ChordEventFifo fifo;
    fifo.push(1.0, "C");
    ChordEvent out;
    fifo.pop(out);

    EXPECT_TRUE(fifo.push(2.0, "D"));
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_EQ(out.getName(), juce::String("D"));
}

// ═══════════════════════════════════════════════════════════════════
// MidiNoteState
// ═══════════════════════════════════════════════════════════════════

TEST(MidiNoteState, NoteOnAndIsNoteOn)
{
    MidiNoteState state;
    state.clear();
    state.noteOn(60);
    EXPECT_TRUE(state.isNoteOn(60));
    EXPECT_FALSE(state.isNoteOn(61));
}

TEST(MidiNoteState, NoteOff)
{
    MidiNoteState state;
    state.clear();
    state.noteOn(60);
    state.noteOff(60);
    EXPECT_FALSE(state.isNoteOn(60));
}

TEST(MidiNoteState, GetHeldNotes)
{
    MidiNoteState state;
    state.clear();
    state.noteOn(60);
    state.noteOn(64);
    state.noteOn(67);

    auto held = state.getHeldNotes();
    ASSERT_EQ(held.size(), 3u);
    EXPECT_EQ(held[0], 60);
    EXPECT_EQ(held[1], 64);
    EXPECT_EQ(held[2], 67);
}

TEST(MidiNoteState, OutOfRangeNoteDoesNotCrash)
{
    MidiNoteState state;
    state.clear();
    state.noteOn(-1);
    state.noteOn(128);
    state.noteOff(-1);
    state.noteOff(128);
    EXPECT_FALSE(state.isNoteOn(-1));
    EXPECT_FALSE(state.isNoteOn(128));
}

TEST(MidiNoteState, BoundaryNotes)
{
    MidiNoteState state;
    state.clear();
    state.noteOn(0);
    state.noteOn(127);
    EXPECT_TRUE(state.isNoteOn(0));
    EXPECT_TRUE(state.isNoteOn(127));
}

TEST(MidiNoteState, ClearResetsAll)
{
    MidiNoteState state;
    state.noteOn(60);
    state.noteOn(72);
    state.clear();
    EXPECT_FALSE(state.isNoteOn(60));
    EXPECT_FALSE(state.isNoteOn(72));
    EXPECT_TRUE(state.getHeldNotes().empty());
}

// ═══════════════════════════════════════════════════════════════════
// TransportState
// ═══════════════════════════════════════════════════════════════════

TEST(TransportState, DefaultValues)
{
    TransportState ts;
    EXPECT_DOUBLE_EQ(ts.bpm.load(), 120.0);
    EXPECT_EQ(ts.timeSigNum.load(), 4);
    EXPECT_EQ(ts.timeSigDen.load(), 4);
    EXPECT_FALSE(ts.isPlaying.load());
    EXPECT_FALSE(ts.isRecording.load());
    EXPECT_EQ(ts.lastProgramChange.load(), -1);
}

TEST(TransportState, WriteReadAtomic)
{
    TransportState ts;
    ts.bpm.store(140.0);
    ts.timeSigNum.store(3);
    ts.timeSigDen.store(8);
    ts.isPlaying.store(true);

    EXPECT_DOUBLE_EQ(ts.bpm.load(), 140.0);
    EXPECT_EQ(ts.timeSigNum.load(), 3);
    EXPECT_EQ(ts.timeSigDen.load(), 8);
    EXPECT_TRUE(ts.isPlaying.load());
}

// ═══════════════════════════════════════════════════════════════════
// JimmySysEx protocol helpers
// ═══════════════════════════════════════════════════════════════════

TEST(JimmySysEx, IsJimmySysEx)
{
    juce::uint8 valid[] = { 0x7D, 0x4A, 0x4D, 0x01 };
    EXPECT_TRUE(JimmySysEx::isJimmySysEx(valid, 4));

    juce::uint8 tooShort[] = { 0x7D, 0x4A };
    EXPECT_FALSE(JimmySysEx::isJimmySysEx(tooShort, 2));

    juce::uint8 wrongMfr[] = { 0x7E, 0x4A, 0x4D, 0x01 };
    EXPECT_FALSE(JimmySysEx::isJimmySysEx(wrongMfr, 4));

    juce::uint8 wrongSub[] = { 0x7D, 0x4B, 0x4D, 0x01 };
    EXPECT_FALSE(JimmySysEx::isJimmySysEx(wrongSub, 4));
}

TEST(JimmySysEx, RelBarEncoding)
{
    EXPECT_EQ(JimmySysEx::decodeRelBar(0, 0), 0);
    EXPECT_EQ(JimmySysEx::decodeRelBar(0, 16), 16);

    // Encode then decode round-trip
    int original = 1234;
    int encoded = JimmySysEx::encodeRelBar(original);
    juce::uint8 hi = static_cast<juce::uint8>((encoded >> 7) & 0x7F);
    juce::uint8 lo = static_cast<juce::uint8>(encoded & 0x7F);
    EXPECT_EQ(JimmySysEx::decodeRelBar(hi, lo), original);

    // Max value clamp
    EXPECT_EQ(JimmySysEx::encodeRelBar(0x7FFF), 0x3FFF);
}

// ═══════════════════════════════════════════════════════════════════
// SongDataFifo
// ═══════════════════════════════════════════════════════════════════

TEST(SongDataFifo, PushPopRoundTrip)
{
    SongDataFifo fifo;
    juce::uint8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    EXPECT_TRUE(fifo.push(data, 4, 5.0, false));

    SongDataEvent out;
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_EQ(out.size, 4);
    EXPECT_NEAR(out.transportBar, 5.0, 0.001);
    EXPECT_FALSE(out.isSongEnd);
    EXPECT_EQ(out.data[0], 0x01);
    EXPECT_EQ(out.data[3], 0x04);
}

TEST(SongDataFifo, PopFromEmptyReturnsFalse)
{
    SongDataFifo fifo;
    SongDataEvent out;
    EXPECT_FALSE(fifo.pop(out));
}

TEST(SongDataFifo, SongEndEvent)
{
    SongDataFifo fifo;
    EXPECT_TRUE(fifo.push(nullptr, 0, 0.0, true));

    SongDataEvent out;
    EXPECT_TRUE(fifo.pop(out));
    EXPECT_TRUE(out.isSongEnd);
    EXPECT_EQ(out.size, 0);
}

TEST(SongDataFifo, RejectOversizedPayload)
{
    SongDataFifo fifo;
    // Payload larger than kMaxPayload should be rejected
    EXPECT_FALSE(fifo.push(nullptr, SongDataEvent::kMaxPayload + 1, 1.0, false));
}

TEST(SongDataFifo, FullBufferReturnsFalse)
{
    SongDataFifo fifo;
    juce::uint8 data[] = { 0xFF };
    // Capacity is 4, usable is 3 (SPSC ring buffer)
    int pushed = 0;
    for (int i = 0; i < 10; ++i)
    {
        if (fifo.push(data, 1, static_cast<double>(i), false))
            ++pushed;
    }
    EXPECT_EQ(pushed, SongDataFifo::kCapacity - 1);
}
