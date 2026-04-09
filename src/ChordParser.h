#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

// Identifies chord names from a set of MIDI note numbers.
class ChordParser
{
public:
    struct ChordResult
    {
        juce::String name;       // e.g. "Cmaj7", "Am", "F#dim"
        juce::String root;       // e.g. "C", "A", "F#"
        juce::String quality;    // e.g. "maj7", "m", "dim"
        int rootNote = -1;       // MIDI note class (0-11)
    };

    static ChordResult identify(const std::vector<int>& midiNotes)
    {
        if (midiNotes.empty())
            return { "", "", "", -1 };

        // Reduce to pitch classes (0-11), deduplicated and sorted
        std::vector<int> pitchClasses;
        for (int note : midiNotes)
        {
            int pc = note % 12;
            if (std::find(pitchClasses.begin(), pitchClasses.end(), pc) == pitchClasses.end())
                pitchClasses.push_back(pc);
        }
        std::sort(pitchClasses.begin(), pitchClasses.end());

        if (pitchClasses.size() == 1)
        {
            auto root = noteNameFromPitchClass(pitchClasses[0]);
            return { root, root, "", pitchClasses[0] };
        }

        // Try each pitch class as potential root and find best match
        ChordResult bestResult;
        int bestScore = -1;

        for (int rootPc : pitchClasses)
        {
            auto intervals = getIntervals(rootPc, pitchClasses);
            auto [quality, score] = matchQuality(intervals);

            if (score > bestScore)
            {
                bestScore = score;
                bestResult.rootNote = rootPc;
                bestResult.root = noteNameFromPitchClass(rootPc);
                bestResult.quality = quality;
                bestResult.name = bestResult.root + quality;
            }
        }

        // If bass note differs from root, add slash notation
        int bassNote = midiNotes.front();
        for (int n : midiNotes)
            if (n < bassNote) bassNote = n;
        int bassPc = bassNote % 12;

        if (bassPc != bestResult.rootNote && bestScore > 0)
            bestResult.name += "/" + noteNameFromPitchClass(bassPc);

        return bestResult;
    }

    // ── Public helpers for transposition ──────────────────────────────

    // Returns the note name string for a pitch class (0–11).
    static juce::String noteNameFromPitchClass(int pc)
    {
        static const char* names[] = { "C", "Db", "D", "Eb", "E", "F",
                                       "F#", "G", "Ab", "A", "Bb", "B" };
        return names[pc % 12];
    }

    // Returns the pitch class (0–11) for a root-note string.
    // Accepts sharps ("C#", "F#") and flats ("Db", "Bb").
    // Returns -1 if the string is not a recognised note name.
    static int pitchClassFromNoteName(const juce::String& name)
    {
        static const std::pair<const char*, int> table[] = {
            { "C#", 1 }, { "Db", 1 },
            { "D#", 3 }, { "Eb", 3 },
            { "F#", 6 }, { "Gb", 6 },
            { "G#", 8 }, { "Ab", 8 },
            { "A#", 10 }, { "Bb", 10 },
            { "C",  0 }, { "D",  2 }, { "E",  4 }, { "F",  5 },
            { "G",  7 }, { "A",  9 }, { "B",  11 },
        };
        for (const auto& entry : table)
            if (name.startsWith(entry.first))
                return entry.second;
        return -1;
    }

    // Returns the number of characters consumed by the root-note prefix
    // ("C" → 1, "C#" → 2, "Bb" → 2, etc.). Returns 0 if not recognised.
    static int rootPrefixLength(const juce::String& name)
    {
        // Only consider 2-char prefix if the second char is '#' or 'b'
        // (a proper accidental), so "Am" is NOT treated as a 2-char root.
        if (name.length() >= 2)
        {
            juce::juce_wchar second = name[1];
            if (second == '#' || second == 'b')
            {
                auto twoChar = name.substring(0, 2);
                if (pitchClassFromNoteName(twoChar) >= 0)
                    return 2;
            }
        }
        if (name.length() >= 1)
        {
            auto oneChar = name.substring(0, 1);
            if (pitchClassFromNoteName(oneChar) >= 0)
                return 1;
        }
        return 0;
    }

    // Transposes a chord name string (e.g. "Am7", "C/E", "F#dim") by
    // the given number of semitones (may be negative).  Slash bass notes
    // are transposed independently.  Returns the original string unchanged
    // if it cannot be parsed (empty, unknown root, etc.).
    static juce::String transposeChordName(const juce::String& name, int semitones)
    {
        if (name.isEmpty() || semitones == 0)
            return name;

        // Helper lambda that shifts a root string by semitones.
        auto transposeRoot = [&](const juce::String& rootStr) -> juce::String
        {
            int pc = pitchClassFromNoteName(rootStr);
            if (pc < 0)
                return rootStr;
            int newPc = ((pc + semitones) % 12 + 12) % 12;
            return noteNameFromPitchClass(newPc);
        };

        // Check for slash bass note first (e.g. "Am/E")
        int slashPos = name.lastIndexOfChar('/');
        juce::String mainPart = (slashPos > 0) ? name.substring(0, slashPos) : name;
        juce::String bassPart = (slashPos > 0) ? name.substring(slashPos + 1) : juce::String();

        // Transpose main chord root
        int prefixLen = rootPrefixLength(mainPart);
        if (prefixLen == 0)
            return name;  // unrecognised root — leave unchanged

        juce::String rootStr   = mainPart.substring(0, prefixLen);
        juce::String qualityStr = mainPart.substring(prefixLen);
        juce::String newMain   = transposeRoot(rootStr) + qualityStr;

        // Transpose bass note (if present)
        if (slashPos > 0)
        {
            int bassPrefixLen = rootPrefixLength(bassPart);
            if (bassPrefixLen > 0)
            {
                juce::String bassRoot    = bassPart.substring(0, bassPrefixLen);
                juce::String bassQuality = bassPart.substring(bassPrefixLen);
                juce::String newBass     = transposeRoot(bassRoot) + bassQuality;
                return newMain + "/" + newBass;
            }
        }

        return newMain;
    }

private:
    // Returns intervals relative to root, sorted
    static std::vector<int> getIntervals(int root, const std::vector<int>& pitchClasses)
    {
        std::vector<int> intervals;
        for (int pc : pitchClasses)
        {
            int interval = (pc - root + 12) % 12;
            if (interval != 0)
                intervals.push_back(interval);
        }
        std::sort(intervals.begin(), intervals.end());
        return intervals;
    }

    // Returns (quality string, match score). Higher score = better match.
    static std::pair<juce::String, int> matchQuality(const std::vector<int>& intervals)
    {
        // Define chord templates: { intervals, quality name, score }
        struct Template {
            std::vector<int> intervals;
            const char* name;
            int score;
        };

        static const Template templates[] = {
            // Triads (high base score)
            { {4, 7},       "",      100 },  // major
            { {3, 7},       "m",     100 },  // minor
            { {3, 6},       "dim",    90 },  // diminished
            { {4, 8},       "aug",    90 },  // augmented
            { {5, 7},       "sus4",   85 },  // sus4
            { {2, 7},       "sus2",   85 },  // sus2

            // Seventh chords
            { {4, 7, 11},   "maj7",  110 },  // major 7
            { {4, 7, 10},   "7",     110 },  // dominant 7
            { {3, 7, 10},   "m7",    110 },  // minor 7
            { {3, 6, 10},   "m7b5",  105 },  // half-dim
            { {3, 6, 9},    "dim7",  105 },  // full-dim 7
            { {3, 7, 11},   "mMaj7", 105 },  // minor-major 7
            { {4, 8, 11},   "maj7#5",100 },  // aug major 7
            { {4, 8, 10},   "7#5",   100 },  // aug dominant 7

            // Extended / add chords
            { {4, 7, 10, 14 % 12},     "9",     115 },
            { {4, 7, 11, 14 % 12},     "maj9",  115 },
            { {3, 7, 10, 14 % 12},     "m9",    115 },
            { {4, 7, 10, 14 % 12, 17 % 12},  "11",  110 },
            { {4, 7, 10, 14 % 12, 21 % 12},  "13",  110 },
            { {4, 7, 14 % 12},         "add9",  95 },
            { {4, 7, 9},               "6",     95 },
            { {3, 7, 9},               "m6",    95 },

            // Power chord
            { {7},          "5",      70 },
        };

        int bestScore = 0;
        juce::String bestQuality = "";

        for (const auto& tmpl : templates)
        {
            // Check if all template intervals are present in input
            int matched = 0;
            for (int ti : tmpl.intervals)
            {
                if (std::find(intervals.begin(), intervals.end(), ti) != intervals.end())
                    matched++;
            }

            if (matched == (int)tmpl.intervals.size())
            {
                // Penalise for extra unmatched intervals
                int extra = (int)intervals.size() - matched;
                int score = tmpl.score - extra * 5;

                if (score > bestScore)
                {
                    bestScore = score;
                    bestQuality = tmpl.name;
                }
            }
        }

        return { bestQuality, bestScore };
    }
};
