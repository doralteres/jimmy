#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "SongModel.h"
#include "MidiChordImporter.h"

// Imports a full Jimmy-encoded MIDI file (lyrics, chords, sections, metadata)
// or falls back to chord-only import for plain MIDI files.
class MidiImporter
{
public:
    static constexpr int kPPQ = 480;

    enum class ImportFormat { JimmyFull, ChordOnly, Unknown };

    struct FullImportResult
    {
        std::vector<LyricLine> lyrics;
        std::vector<Section>   sections;
        std::vector<Chord>     chords;
        double defaultBarsPerLine = 2.0;
        double bpm                = 120.0;
        int    timeSigNum         = 4;
        int    timeSigDen         = 4;
        bool   success            = false;
        juce::String errorMessage;
    };

    // Detect the format of a MIDI file without fully parsing it.
    static ImportFormat detectFormat(const juce::File& file)
    {
        if (!file.existsAsFile())
            return ImportFormat::Unknown;

        juce::FileInputStream stream(file);
        if (!stream.openedOk())
            return ImportFormat::Unknown;

        juce::MidiFile midiFile;
        if (!midiFile.readFrom(stream, true))
            return ImportFormat::Unknown;

        return detectFormatFromMidiFile(midiFile);
    }

    // Detect format from an already-parsed MidiFile.
    static ImportFormat detectFormatFromMidiFile(const juce::MidiFile& midiFile)
    {
        for (int track = 0; track < midiFile.getNumTracks(); ++track)
        {
            const auto* seq = midiFile.getTrack(track);
            if (seq == nullptr) continue;

            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto& msg = seq->getEventPointer(i)->message;
                if (msg.isTextMetaEvent())
                {
                    auto text = msg.getTextFromTextMetaEvent();
                    if (text == "JIMMY:v1")
                        return ImportFormat::JimmyFull;
                }
            }
        }
        return ImportFormat::ChordOnly;
    }

    // Full import: parse all lyrics, chords, sections, and metadata.
    static FullImportResult importFull(const juce::File& file)
    {
        FullImportResult result;

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

        return importFullFromMidiFile(midiFile);
    }

    // Full import from an already-parsed MidiFile.
    static FullImportResult importFullFromMidiFile(const juce::MidiFile& midiFile)
    {
        FullImportResult result;

        short timeFormat = midiFile.getTimeFormat();
        if (timeFormat <= 0)
        {
            result.errorMessage = "SMPTE time format not supported.";
            return result;
        }

        double ticksPerQN = static_cast<double>(timeFormat);

        // Build a time-signature map for tick→bar conversion
        juce::MidiMessageSequence timeSigEvents;
        midiFile.findAllTimeSigEvents(timeSigEvents);

        std::vector<MidiChordImporter::TimeSigEntry> timeSigMap;
        timeSigMap.push_back({ 0.0, 4, 4 });

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

        // Collect all meta events across all tracks into a flat list
        struct MetaEvent
        {
            double tick;
            bool   isLyric;     // true = Lyric (0x05), false = Text (0x01)
            juce::String text;
        };
        std::vector<MetaEvent> events;

        bool foundJimmyMarker = false;

        // Maps for per-line metadata
        std::map<int, double> lineLenOverrides;  // index → length in bars
        std::map<int, double> breakEntries;      // index → break length in bars

        for (int track = 0; track < midiFile.getNumTracks(); ++track)
        {
            const auto* seq = midiFile.getTrack(track);
            if (seq == nullptr) continue;

            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                const auto& msg = seq->getEventPointer(i)->message;
                if (!msg.isMetaEvent()) continue;

                auto type = msg.getMetaEventType();
                if (type == 0x01) // Text meta
                {
                    auto text = msg.getTextFromTextMetaEvent();
                    double tick = msg.getTimeStamp();

                    if (text == "JIMMY:v1")
                    {
                        foundJimmyMarker = true;
                        continue;
                    }

                    // Parse tick-0 metadata
                    if (tick < 1.0)
                    {
                        if (text.startsWith("DEFAULT_BPL:"))
                        {
                            result.defaultBarsPerLine = text.fromFirstOccurrenceOf("DEFAULT_BPL:", false, false).getDoubleValue();
                            if (result.defaultBarsPerLine <= 0.0)
                                result.defaultBarsPerLine = 2.0;
                            continue;
                        }
                        if (text.startsWith("LINE_LEN:"))
                        {
                            parseLineLenMeta(text, lineLenOverrides);
                            continue;
                        }
                        if (text.startsWith("BREAK:"))
                        {
                            parseBreakMeta(text, breakEntries);
                            continue;
                        }
                    }

                    events.push_back({ tick, false, text });
                }
                else if (type == 0x05) // Lyric meta
                {
                    auto text = msg.getTextFromTextMetaEvent();
                    events.push_back({ msg.getTimeStamp(), true, text });
                }
                else if (type == 0x51) // Tempo
                {
                    // Extract microseconds per QN from the raw data
                    auto* data = msg.getMetaEventData();
                    int len = msg.getMetaEventLength();
                    if (len >= 3)
                    {
                        int usPerQN = (data[0] << 16) | (data[1] << 8) | data[2];
                        result.bpm = 60000000.0 / usPerQN;
                    }
                }
                else if (type == 0x58) // Time signature
                {
                    int num = 0, den = 0;
                    msg.getTimeSignatureInfo(num, den);
                    if (num > 0 && den > 0)
                    {
                        result.timeSigNum = num;
                        result.timeSigDen = den;
                        // Update the default entry if it's at tick 0
                        if (msg.getTimeStamp() < 1.0 && !timeSigMap.empty())
                        {
                            timeSigMap[0].num = num;
                            timeSigMap[0].den = den;
                        }
                    }
                }
            }
        }

        if (!foundJimmyMarker)
        {
            result.errorMessage = "Not a Jimmy MIDI file (missing JIMMY:v1 marker).";
            return result;
        }

        // Sort events by tick
        std::sort(events.begin(), events.end(),
                  [](const MetaEvent& a, const MetaEvent& b) { return a.tick < b.tick; });

        // ── Parse events into lyrics, sections, and chords ──
        int lyricIndex = 0;

        for (const auto& evt : events)
        {
            double barPos = MidiChordImporter::tickToBarPosition(evt.tick, ticksPerQN, timeSigMap);

            if (!evt.isLyric)
            {
                // Text Meta Event
                if (evt.text.startsWith("SECTION:"))
                {
                    parseSectionMeta(evt.text, barPos, result.sections);
                }
                else if (evt.text.startsWith("CHORD:"))
                {
                    parseChordMeta(evt.text, barPos, result.chords);
                }
                // Other text events at tick > 0 are ignored
            }
            else
            {
                // Lyric Meta Event
                if (evt.text == "\r" || evt.text == "\n")
                    continue;  // CR/LF are structural markers, not content

                // This is a lyric line
                double lineLen = result.defaultBarsPerLine;

                // Check for per-line override
                auto overrideIt = lineLenOverrides.find(lyricIndex);
                if (overrideIt != lineLenOverrides.end())
                    lineLen = overrideIt->second;

                // breakEntries maps a final-list position N to a break length.
                // Position N in the result is a break entry; position N+1 is the next lyric.
                // Consume any breaks whose position matches lyricIndex before appending the lyric.

                while (breakEntries.count(lyricIndex))
                {
                    double breakLen = breakEntries[lyricIndex];
                    LyricLine breakLine;
                    breakLine.isBreak = true;
                    breakLine.startBar = barPos;
                    breakLine.endBar = barPos + breakLen;
                    breakLine.sectionIndex = findSectionIndex(barPos, result.sections);
                    result.lyrics.push_back(breakLine);
                    lyricIndex++;

                    // Re-check for line length override at new index
                    overrideIt = lineLenOverrides.find(lyricIndex);
                    if (overrideIt != lineLenOverrides.end())
                        lineLen = overrideIt->second;
                }

                LyricLine line;
                line.text = evt.text;
                line.startBar = barPos;
                line.endBar = barPos + lineLen;
                line.sectionIndex = findSectionIndex(barPos, result.sections);
                result.lyrics.push_back(line);
                lyricIndex++;
            }
        }

        // Insert any remaining breaks at the end
        while (breakEntries.count(lyricIndex))
        {
            double breakLen = breakEntries[lyricIndex];
            double barPos = result.lyrics.empty() ? 1.0 : result.lyrics.back().endBar;
            LyricLine breakLine;
            breakLine.isBreak = true;
            breakLine.startBar = barPos;
            breakLine.endBar = barPos + breakLen;
            breakLine.sectionIndex = findSectionIndex(barPos, result.sections);
            result.lyrics.push_back(breakLine);
            lyricIndex++;
        }

        result.success = true;
        return result;
    }

    // Chord-only import — delegates to existing MidiChordImporter.
    static MidiChordImporter::ImportResult importChordsOnly(const juce::File& file)
    {
        return MidiChordImporter::importFromFile(file);
    }

private:
    // Parse "LINE_LEN:<index>:<length>"
    static void parseLineLenMeta(const juce::String& text, std::map<int, double>& out)
    {
        auto parts = juce::StringArray::fromTokens(text.fromFirstOccurrenceOf("LINE_LEN:", false, false), ":", "");
        if (parts.size() >= 2)
        {
            int idx = parts[0].getIntValue();
            double len = parts[1].getDoubleValue();
            if (idx >= 0 && len > 0.0)
                out[idx] = len;
        }
    }

    // Parse "BREAK:<index>:<length>"
    static void parseBreakMeta(const juce::String& text, std::map<int, double>& out)
    {
        auto parts = juce::StringArray::fromTokens(text.fromFirstOccurrenceOf("BREAK:", false, false), ":", "");
        if (parts.size() >= 2)
        {
            int idx = parts[0].getIntValue();
            double len = parts[1].getDoubleValue();
            if (idx >= 0 && len > 0.0)
                out[idx] = len;
        }
    }

    // Parse "SECTION:Name:endBar=N:colour=RRGGBBAA"
    static void parseSectionMeta(const juce::String& text, double barPos, std::vector<Section>& sections)
    {
        auto content = text.fromFirstOccurrenceOf("SECTION:", false, false);

        Section section;
        section.startBar = static_cast<int>(std::round(barPos));

        // Parse the colon-delimited attributes
        // Format: "SectionName:endBar=N:colour=RRGGBBAA"
        auto firstColon = content.indexOf(":endBar=");
        if (firstColon >= 0)
        {
            section.name = content.substring(0, firstColon);
            auto rest = content.substring(firstColon + 1);

            auto endBarStr = rest.fromFirstOccurrenceOf("endBar=", false, false)
                                 .upToFirstOccurrenceOf(":", false, false);
            section.endBar = endBarStr.getIntValue();

            auto colourStr = rest.fromFirstOccurrenceOf("colour=", false, false);
            if (colourStr.isNotEmpty())
                section.colour = juce::Colour::fromString(colourStr);
        }
        else
        {
            section.name = content;
            section.endBar = section.startBar + 8;  // default range
        }

        if (section.endBar < section.startBar)
            section.endBar = section.startBar;

        sections.push_back(section);
    }

    // Parse "CHORD:Name:source"
    static void parseChordMeta(const juce::String& text, double barPos, std::vector<Chord>& chords)
    {
        auto content = text.fromFirstOccurrenceOf("CHORD:", false, false);

        Chord chord;
        chord.barPosition = barPos;

        // Split "Name:source"
        auto lastColon = content.lastIndexOf(":");
        if (lastColon > 0)
        {
            auto sourceStr = content.substring(lastColon + 1);
            if (sourceStr == "manual" || sourceStr == "midi")
            {
                chord.name = content.substring(0, lastColon);
                chord.source = (sourceStr == "manual") ? Chord::Manual : Chord::Midi;
            }
            else
            {
                // No valid source suffix — entire content is the name
                chord.name = content;
                chord.source = Chord::Midi;
            }
        }
        else
        {
            chord.name = content;
            chord.source = Chord::Midi;
        }

        if (chord.name.isNotEmpty())
            chords.push_back(chord);
    }

    // Find which section index a bar position falls into.
    static int findSectionIndex(double barPos, const std::vector<Section>& sections)
    {
        for (int i = (int)sections.size() - 1; i >= 0; --i)
        {
            if (barPos >= sections[static_cast<size_t>(i)].startBar &&
                barPos <= sections[static_cast<size_t>(i)].endBar)
                return i;
        }
        return -1;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiImporter)
};
