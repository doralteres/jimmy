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
