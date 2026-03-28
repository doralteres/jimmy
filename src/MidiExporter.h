#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "SongModel.h"

// Encodes the full SongModel state into a Standard MIDI File (Type 0, 480 PPQ).
// Uses RP-017 Lyric Meta Events for lyrics and prefixed Text Meta Events for
// chords, sections, and plugin metadata.
class MidiExporter
{
public:
    static constexpr int kPPQ = 480;

    struct ExportContext
    {
        double bpm        = 120.0;
        int    timeSigNum = 4;
        int    timeSigDen = 4;
    };

    // Serialize SongModel into a MIDI file stored in a MemoryBlock.
    static juce::MemoryBlock exportToSmf(const SongModel& model, const ExportContext& ctx)
    {
        juce::MidiMessageSequence seq;

        double defaultBPL = model.getDefaultBarsPerLine();
        auto lyrics   = model.getLyrics();
        auto chords   = model.getChords();
        auto sections = model.getSections();

        // ── Tick 0: metadata ──
        addTextMeta(seq, 0.0, "JIMMY:v1");
        addTempoMeta(seq, 0.0, ctx.bpm);
        addTimeSigMeta(seq, 0.0, ctx.timeSigNum, ctx.timeSigDen);
        addTextMeta(seq, 0.0, "DEFAULT_BPL:" + juce::String(defaultBPL));

        // Per-line metadata (LINE_LEN overrides, BREAK markers)
        for (int i = 0; i < (int)lyrics.size(); ++i)
        {
            const auto& line = lyrics[static_cast<size_t>(i)];
            if (line.isBreak)
            {
                double breakBars = line.endBar - line.startBar;
                addTextMeta(seq, 0.0, "BREAK:" + juce::String(i) + ":" + juce::String(breakBars));
            }
            else
            {
                double lineLen = line.endBar - line.startBar;
                if (std::abs(lineLen - defaultBPL) > 0.001)
                    addTextMeta(seq, 0.0, "LINE_LEN:" + juce::String(i) + ":" + juce::String(lineLen));
            }
        }

        // ── Sections as Text Meta Events ──
        for (const auto& section : sections)
        {
            double tick = barToTick(section.startBar, ctx.timeSigNum);
            addTextMeta(seq, tick, "SECTION:" + section.name
                        + ":endBar=" + juce::String(section.endBar)
                        + ":colour=" + section.colour.toString());
        }

        // ── Lyrics as RP-017 Lyric Meta Events ──
        int currentSectionIdx = -1;
        for (int i = 0; i < (int)lyrics.size(); ++i)
        {
            const auto& line = lyrics[static_cast<size_t>(i)];
            double tick = barToTick(line.startBar, ctx.timeSigNum);

            if (line.isBreak)
                continue;  // breaks are encoded as metadata only

            // Detect section change — emit LF (paragraph end) for the previous section
            if (line.sectionIndex != currentSectionIdx && currentSectionIdx >= 0)
                addLyricMeta(seq, tick - 1.0, "\n");  // LF = section/paragraph end (RP-017)

            currentSectionIdx = line.sectionIndex;

            // Lyric text
            addLyricMeta(seq, tick, line.text);
            // CR = end of display line (RP-017)
            addLyricMeta(seq, tick + 1.0, "\r");
        }
        // Final section end
        if (!lyrics.empty() && currentSectionIdx >= 0)
        {
            double lastTick = barToTick(lyrics.back().startBar, ctx.timeSigNum) + 2.0;
            addLyricMeta(seq, lastTick, "\n");
        }

        // ── Chords as Text Meta Events ──
        for (const auto& chord : chords)
        {
            double tick = barToTick(chord.barPosition, ctx.timeSigNum);
            juce::String sourceStr = (chord.source == Chord::Manual) ? "manual" : "midi";
            addTextMeta(seq, tick, "CHORD:" + chord.name + ":" + sourceStr);
        }

        seq.sort();
        seq.updateMatchedPairs();

        // Build MidiFile
        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote(kPPQ);
        midiFile.addTrack(seq);

        // Write to MemoryBlock
        juce::MemoryOutputStream mos;
        midiFile.writeTo(mos);

        return mos.getMemoryBlock();
    }

    // Write SongModel to a MIDI file on disk.
    static bool writeToFile(const SongModel& model, const ExportContext& ctx, const juce::File& dest)
    {
        auto data = exportToSmf(model, ctx);
        return dest.replaceWithData(data.getData(), data.getSize());
    }

    // Convert a 1-based bar position to MIDI ticks (public for testing).
    // NOTE: Assumes a single fixed time signature throughout the song.
    // If the time signature changes mid-song this formula will produce incorrect
    // tick positions for bars after the change.
    static double barToTick(double barPosition, int timeSigNum)
    {
        return (barPosition - 1.0) * timeSigNum * kPPQ;
    }

private:
    static void addTextMeta(juce::MidiMessageSequence& seq, double tick, const juce::String& text)
    {
        seq.addEvent(juce::MidiMessage::textMetaEvent(1, text), tick);
    }

    static void addLyricMeta(juce::MidiMessageSequence& seq, double tick, const juce::String& text)
    {
        seq.addEvent(juce::MidiMessage::textMetaEvent(5, text), tick);
    }

    static void addTempoMeta(juce::MidiMessageSequence& seq, double tick, double bpm)
    {
        double microsecondsPerQN = 60000000.0 / bpm;
        seq.addEvent(juce::MidiMessage::tempoMetaEvent(
            static_cast<int>(std::round(microsecondsPerQN))), tick);
    }

    static void addTimeSigMeta(juce::MidiMessageSequence& seq, double tick, int num, int den)
    {
        // Time sig meta event: denominator is log2
        int denPow = 0;
        int d = den;
        while (d > 1) { d >>= 1; ++denPow; }

        juce::uint8 rawData[] = {
            0xFF, 0x58, 0x04,
            static_cast<juce::uint8>(num),
            static_cast<juce::uint8>(denPow),
            24,  // MIDI clocks per metronome click
            8    // 32nd notes per MIDI quarter note
        };
        auto msg = juce::MidiMessage(rawData, (int)sizeof(rawData));
        msg.setTimeStamp(tick);
        seq.addEvent(msg);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiExporter)
};
