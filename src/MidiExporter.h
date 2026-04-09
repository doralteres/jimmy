#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "SongModel.h"
#include "SharedState.h"

// Encodes the full SongModel state into a Standard MIDI File (Type 0, 480 PPQ).
// Uses RP-017 Lyric Meta Events for lyrics and prefixed Text Meta Events for
// chords, sections, and plugin metadata.
class MidiExporter
{
public:
    static constexpr int kPPQ = 480;
    static constexpr int kSysExRepeatBars = 16; // repeat song SysEx every N bars

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

        int transposeOffset = model.getTransposeOffset();
        if (transposeOffset != 0)
            addTextMeta(seq, 0.0, "TRANSPOSE:" + juce::String(transposeOffset));

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

        // ── SysEx: encode full song data for DAW-track playback ──
        // Pipeline: songToXml() → gzip → base64 → SysEx with Jimmy header
        auto compressedPayload = compressSongXml(model.songToXml());
        if (compressedPayload.getSize() > 0)
        {
            // Compute the last bar in the song for repeat placement
            double lastBar = 1.0;
            for (const auto& l : lyrics)
                lastBar = std::max(lastBar, l.endBar);
            for (const auto& c : chords)
                lastBar = std::max(lastBar, c.barPosition + 1.0);
            for (const auto& s : sections)
                lastBar = std::max(lastBar, static_cast<double>(s.endBar));

            // Place song header SysEx at tick 0, then every kSysExRepeatBars
            for (int bar = 0; bar <= static_cast<int>(lastBar); bar += kSysExRepeatBars)
            {
                double tick = barToTick(static_cast<double>(bar) + 1.0, ctx.timeSigNum);
                auto sysExMsg = buildSongHeaderSysEx(compressedPayload, bar);
                seq.addEvent(sysExMsg, tick);
            }

            // Song end marker at the last bar
            double endTick = barToTick(lastBar + 1.0, ctx.timeSigNum);
            auto endMsg = buildSongEndSysEx();
            seq.addEvent(endMsg, endTick);

            seq.sort();
        }

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

    // ── SysEx helpers ──

    // Compress song XML: gzip → base64 → MemoryBlock
    static juce::MemoryBlock compressSongXml(const juce::String& xmlString)
    {
        // Gzip compress
        juce::MemoryBlock gzipped;
        {
            juce::MemoryOutputStream raw(gzipped, false);
            juce::GZIPCompressorOutputStream gzip(raw);
            gzip.write(xmlString.toRawUTF8(), xmlString.getNumBytesAsUTF8());
            gzip.flush();
        }

        // Base64 encode (SysEx bytes must be 0x00-0x7F; base64 guarantees that)
        auto base64 = juce::Base64::toBase64(gzipped.getData(), gzipped.getSize());

        juce::MemoryBlock result;
        result.append(base64.toRawUTF8(), base64.getNumBytesAsUTF8());
        return result;
    }

    // Decompress: base64 → gunzip → XML string (public for testing & UI thread use)
public:
    static juce::String decompressSongXml(const juce::uint8* data, int size)
    {
        // Base64 decode
        juce::MemoryOutputStream decoded;
        if (!juce::Base64::convertFromBase64(decoded, juce::String::fromUTF8(reinterpret_cast<const char*>(data), size)))
            return {};

        // Gunzip
        juce::MemoryInputStream compressedStream(decoded.getData(), decoded.getDataSize(), false);
        juce::GZIPDecompressorInputStream gunzip(compressedStream);

        juce::MemoryOutputStream xmlOut;
        xmlOut.writeFromInputStream(gunzip, -1);
        return xmlOut.toString();
    }

private:
    // Build a SysEx message for a Song Header
    static juce::MidiMessage buildSongHeaderSysEx(const juce::MemoryBlock& compressedPayload, int relativeBar)
    {
        int encodedBar = JimmySysEx::encodeRelBar(relativeBar);
        juce::uint8 relBarHi = static_cast<juce::uint8>((encodedBar >> 7) & 0x7F);
        juce::uint8 relBarLo = static_cast<juce::uint8>(encodedBar & 0x7F);

        // Header: [manufacturer][subId1][subId2][msgType][relBarHi][relBarLo][payload...]
        juce::MemoryBlock sysExBody;
        juce::uint8 header[] = {
            JimmySysEx::kManufacturer,
            JimmySysEx::kSubId1,
            JimmySysEx::kSubId2,
            JimmySysEx::kSongHeader,
            relBarHi,
            relBarLo
        };
        sysExBody.append(header, sizeof(header));
        sysExBody.append(compressedPayload.getData(), compressedPayload.getSize());

        return juce::MidiMessage::createSysExMessage(sysExBody.getData(),
                                                     static_cast<int>(sysExBody.getSize()));
    }

    // Build a SysEx message for Song End
    static juce::MidiMessage buildSongEndSysEx()
    {
        juce::uint8 body[] = {
            JimmySysEx::kManufacturer,
            JimmySysEx::kSubId1,
            JimmySysEx::kSubId2,
            JimmySysEx::kSongEnd
        };
        return juce::MidiMessage::createSysExMessage(body, sizeof(body));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiExporter)
};
