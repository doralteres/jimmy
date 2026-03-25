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
