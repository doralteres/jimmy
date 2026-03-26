#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "SongModel.h"
#include "ChordParser.h"
#include <vector>
#include <map>

// Imports chords from a MIDI file (.mid) by grouping simultaneous notes
// into chord voicings and identifying them via ChordParser.
class MidiChordImporter
{
public:
    struct ImportResult
    {
        std::vector<Chord> chords;
        juce::String errorMessage;
        bool success = false;
    };

    static ImportResult importFromFile(const juce::File& file)
    {
        ImportResult result;

        if (!file.existsAsFile())
        {
            result.errorMessage = "File not found: " + file.getFileName();
            return result;
        }

        juce::FileInputStream stream(file);
        if (!stream.openedOk())
        {
            result.errorMessage = "Could not open file: " + file.getFileName();
            return result;
        }

        juce::MidiFile midiFile;
        if (!midiFile.readFrom(stream, true))
        {
            result.errorMessage = "Invalid MIDI file: " + file.getFileName();
            return result;
        }

        short timeFormat = midiFile.getTimeFormat();
        if (timeFormat <= 0)
        {
            result.errorMessage = "SMPTE time format not supported. Use PPQ-based MIDI files.";
            return result;
        }

        double ticksPerQuarterNote = static_cast<double>(timeFormat);

        // Extract time signature events to compute beats per bar
        juce::MidiMessageSequence timeSigEvents;
        midiFile.findAllTimeSigEvents(timeSigEvents);

        // Collect all note-on events across all tracks with their tick timestamps
        struct NoteEvent
        {
            double tick;
            int noteNumber;
        };
        std::vector<NoteEvent> allNotes;

        for (int track = 0; track < midiFile.getNumTracks(); ++track)
        {
            const auto* seq = midiFile.getTrack(track);
            if (seq == nullptr) continue;

            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto& event = *seq->getEventPointer(i);
                if (event.message.isNoteOn(false))
                    allNotes.push_back({ event.message.getTimeStamp(), event.message.getNoteNumber() });
            }
        }

        if (allNotes.empty())
        {
            result.errorMessage = "No MIDI notes found in file.";
            return result;
        }

        // Sort by tick time
        std::sort(allNotes.begin(), allNotes.end(),
                  [](const NoteEvent& a, const NoteEvent& b) { return a.tick < b.tick; });

        // Group notes that are within a small tick window into chords
        const double groupThreshold = ticksPerQuarterNote * 0.05;  // ~5% of a beat
        std::vector<std::pair<double, std::vector<int>>> chordGroups;

        double groupStart = allNotes[0].tick;
        std::vector<int> currentGroup;

        for (const auto& note : allNotes)
        {
            if (note.tick - groupStart > groupThreshold && !currentGroup.empty())
            {
                chordGroups.push_back({ groupStart, currentGroup });
                currentGroup.clear();
                groupStart = note.tick;
            }
            currentGroup.push_back(note.noteNumber);
        }
        if (!currentGroup.empty())
            chordGroups.push_back({ groupStart, currentGroup });

        // Build a time signature map: tick -> (numerator, denominator)
        std::vector<TimeSigEntry> timeSigMap;
        timeSigMap.push_back({ 0.0, 4, 4 });  // default

        for (int i = 0; i < timeSigEvents.getNumEvents(); ++i)
        {
            const auto& msg = timeSigEvents.getEventPointer(i)->message;
            if (msg.isTimeSignatureMetaEvent())
            {
                int num = 0, den = 0;
                msg.getTimeSignatureInfo(num, den);
                if (num > 0 && den > 0)
                    timeSigMap.push_back({ msg.getTimeStamp(), num, den });
            }
        }

        // Convert each chord group's tick to a 1-based bar position and identify
        for (const auto& [tick, notes] : chordGroups)
        {
            auto chordResult = ChordParser::identify(notes);
            if (chordResult.name.isEmpty())
                continue;

            double barPos = tickToBarPosition(tick, ticksPerQuarterNote, timeSigMap);

            Chord chord;
            chord.name = chordResult.name;
            chord.barPosition = barPos;
            chord.source = Chord::Midi;
            result.chords.push_back(chord);
        }

        // Remove duplicate chords at very close positions with the same name
        if (result.chords.size() > 1)
        {
            std::vector<Chord> deduped;
            deduped.push_back(result.chords[0]);
            for (size_t i = 1; i < result.chords.size(); ++i)
            {
                const auto& prev = deduped.back();
                const auto& curr = result.chords[i];
                if (curr.name != prev.name || std::abs(curr.barPosition - prev.barPosition) > 0.05)
                    deduped.push_back(curr);
            }
            result.chords = std::move(deduped);
        }

        result.success = true;
        return result;
    }

    struct TimeSigEntry { double tick; int num; int den; };

private:
    // Convert a tick position to a 1-based bar number
    static double tickToBarPosition(double tick,
                                    double ticksPerQN,
                                    const std::vector<TimeSigEntry>& timeSigMap)
    {
        double barPos = 1.0;
        double currentTick = 0.0;

        for (size_t i = 0; i < timeSigMap.size(); ++i)
        {
            int num = timeSigMap[i].num;
            int den = timeSigMap[i].den;

            double nextBoundary = (i + 1 < timeSigMap.size())
                ? std::min(tick, timeSigMap[i + 1].tick)
                : tick;

            if (currentTick >= tick)
                break;

            double ticksPerBar = num * (4.0 / den) * ticksPerQN;
            if (ticksPerBar <= 0.0)
                ticksPerBar = 4.0 * ticksPerQN;

            double tickSpan = nextBoundary - currentTick;
            if (tickSpan > 0.0)
            {
                double bars = tickSpan / ticksPerBar;
                barPos += bars;
            }

            currentTick = nextBoundary;
        }

        return barPos;
    }
};
