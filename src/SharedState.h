#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <array>

// Lock-free shared state for processor -> editor communication.
// Written on the audio thread, read on the UI thread.
struct TransportState
{
    std::atomic<double>  bpm           { 120.0 };
    std::atomic<int>     timeSigNum    { 4 };
    std::atomic<int>     timeSigDen    { 4 };
    std::atomic<double>  ppqPosition   { 0.0 };
    std::atomic<double>  barStartPpq   { 0.0 };
    std::atomic<int64_t> barCount      { 0 };
    std::atomic<bool>    isPlaying     { false };
    std::atomic<bool>    isRecording   { false };
    std::atomic<int>     lastProgramChange { -1 };
    std::atomic<bool>    programChangeFlag { false };

    // Live source mode: 0 = LiveInput (default), 1 = FromEditor
    // UI thread writes, audio thread reads
    std::atomic<int>     liveSourceMode { 0 };
};

// Stores currently held MIDI notes (lock-free via atomic flags).
// Each bit in the array represents whether a note is currently on.
struct MidiNoteState
{
    std::array<std::atomic<bool>, 128> held {};

    void clear()
    {
        for (auto& note : held)
            note.store(false, std::memory_order_relaxed);
    }

    void noteOn(int noteNumber)
    {
        if (noteNumber >= 0 && noteNumber < 128)
            held[static_cast<size_t>(noteNumber)].store(true, std::memory_order_relaxed);
    }

    void noteOff(int noteNumber)
    {
        if (noteNumber >= 0 && noteNumber < 128)
            held[static_cast<size_t>(noteNumber)].store(false, std::memory_order_relaxed);
    }

    bool isNoteOn(int noteNumber) const
    {
        if (noteNumber >= 0 && noteNumber < 128)
            return held[static_cast<size_t>(noteNumber)].load(std::memory_order_relaxed);
        return false;
    }

    // Returns a snapshot of all held notes (UI thread use)
    std::vector<int> getHeldNotes() const
    {
        std::vector<int> notes;
        for (int i = 0; i < 128; ++i)
            if (held[static_cast<size_t>(i)].load(std::memory_order_relaxed))
                notes.push_back(i);
        return notes;
    }
};

// Lock-free SPSC FIFO for chord events (audio thread -> UI thread).
// Audio thread pushes detected chords; UI thread drains them into SongModel.
struct ChordEvent
{
    double barPosition = 0.0;
    char   name[32] {};          // fixed-size to avoid allocations

    void setName(const juce::String& s)
    {
        auto utf8 = s.toRawUTF8();
        auto len = std::min(s.getNumBytesAsUTF8(), (size_t)31);
        std::memcpy(name, utf8, len);
        name[len] = '\0';
    }

    juce::String getName() const { return juce::String::fromUTF8(name); }
};

struct ChordEventFifo
{
    static constexpr int kCapacity = 256;

    bool push(double barPos, const juce::String& chordName)
    {
        auto w = writePos.load(std::memory_order_relaxed);
        auto r = readPos.load(std::memory_order_acquire);

        if (((w + 1) % kCapacity) == r)
            return false;  // full

        buffer[static_cast<size_t>(w)].barPosition = barPos;
        buffer[static_cast<size_t>(w)].setName(chordName);
        writePos.store((w + 1) % kCapacity, std::memory_order_release);
        return true;
    }

    bool pop(ChordEvent& out)
    {
        auto r = readPos.load(std::memory_order_relaxed);
        auto w = writePos.load(std::memory_order_acquire);

        if (r == w)
            return false;  // empty

        out = buffer[static_cast<size_t>(r)];
        readPos.store((r + 1) % kCapacity, std::memory_order_release);
        return true;
    }

private:
    std::array<ChordEvent, kCapacity> buffer {};
    std::atomic<int> readPos  { 0 };
    std::atomic<int> writePos { 0 };
};

// ── Jimmy SysEx protocol ──────────────────────────────────────────
// Manufacturer ID 0x7D (non-commercial) + sub-ID "JM" (0x4A, 0x4D).
// Used in exported MIDI files so DAWs forward song data to the plugin.
namespace JimmySysEx
{
    static constexpr juce::uint8 kManufacturer = 0x7D;
    static constexpr juce::uint8 kSubId1       = 0x4A;  // 'J'
    static constexpr juce::uint8 kSubId2       = 0x4D;  // 'M'
    static constexpr int         kHeaderSize   = 3;      // manufacturer + sub-IDs

    // Message types (byte following header)
    static constexpr juce::uint8 kSongHeader   = 0x01;
    static constexpr juce::uint8 kSongEnd      = 0x02;

    // Payload layout for kSongHeader:
    //   [0x7D 0x4A 0x4D] [0x01] [relBar_hi(7)] [relBar_lo(7)] [compressed XML...]
    //   relBar = 14-bit relative bar offset from the start of the song clip
    static constexpr int kMsgTypeOffset   = 3;
    static constexpr int kRelBarHiOffset  = 4;
    static constexpr int kRelBarLoOffset  = 5;
    static constexpr int kPayloadOffset   = 6;

    inline bool isJimmySysEx(const juce::uint8* data, int size)
    {
        return size > kHeaderSize
            && data[0] == kManufacturer
            && data[1] == kSubId1
            && data[2] == kSubId2;
    }

    inline int encodeRelBar(int bar)
    {
        return (bar > 0x3FFF) ? 0x3FFF : bar;
    }

    inline int decodeRelBar(juce::uint8 hi, juce::uint8 lo)
    {
        return ((hi & 0x7F) << 7) | (lo & 0x7F);
    }
}

// Lock-free SPSC FIFO for SysEx song data (audio thread -> UI thread).
// Audio thread pushes raw SysEx payloads; UI thread decodes XML.
struct SongDataEvent
{
    static constexpr int kMaxPayload = 16384;

    juce::uint8 data[kMaxPayload] {};
    int    size         = 0;
    double transportBar = 0.0;   // absolute bar when received
    bool   isSongEnd    = false; // true = clear active song
};

struct SongDataFifo
{
    static constexpr int kCapacity = 4;

    bool push(const juce::uint8* payload, int payloadSize, double bar, bool songEnd = false)
    {
        if (payloadSize > SongDataEvent::kMaxPayload)
            return false;

        auto w = writePos.load(std::memory_order_relaxed);
        auto r = readPos.load(std::memory_order_acquire);

        if (((w + 1) % kCapacity) == r)
            return false;  // full

        auto& slot = buffer[static_cast<size_t>(w)];
        if (payloadSize > 0)
            std::memcpy(slot.data, payload, static_cast<size_t>(payloadSize));
        slot.size = payloadSize;
        slot.transportBar = bar;
        slot.isSongEnd = songEnd;
        writePos.store((w + 1) % kCapacity, std::memory_order_release);
        return true;
    }

    bool pop(SongDataEvent& out)
    {
        auto r = readPos.load(std::memory_order_relaxed);
        auto w = writePos.load(std::memory_order_acquire);

        if (r == w)
            return false;  // empty

        out = buffer[static_cast<size_t>(r)];
        readPos.store((r + 1) % kCapacity, std::memory_order_release);
        return true;
    }

private:
    std::array<SongDataEvent, kCapacity> buffer {};
    std::atomic<int> readPos  { 0 };
    std::atomic<int> writePos { 0 };
};
