#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <mutex>
#include <algorithm>

// Represents a chord at a specific timeline position.
struct Chord
{
    juce::String name;
    double barPosition = 0.0;  // bar number (1-based)
    enum Source { Midi, Manual } source = Midi;

    bool operator<(const Chord& other) const { return barPosition < other.barPosition; }
};

// Represents a song section (verse, chorus, etc.)
struct Section
{
    juce::String name;
    int startBar = 1;
    int endBar   = 1;
    juce::Colour colour { 0xff00bcd4 };
};

// Represents a line of lyrics mapped to a bar range.
struct LyricLine
{
    juce::String text;
    double startBar = 1.0;
    double endBar   = 2.0;
    int sectionIndex = -1;  // optional link to a Section
};

// The complete song data model. Thread-safe via mutex for UI thread access.
// Audio thread should NOT lock — use the SharedState atomics instead.
class SongModel
{
public:
    // --- Chord operations ---
    void addChord(const Chord& chord)
    {
        std::lock_guard<std::mutex> lock(mutex);
        chords.push_back(chord);
        std::sort(chords.begin(), chords.end());
    }

    void setMidiChordAt(double barPos, const juce::String& name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Replace existing MIDI chord near this position, or add new one
        for (auto& c : chords)
        {
            if (c.source == Chord::Midi && std::abs(c.barPosition - barPos) < 0.01)
            {
                c.name = name;
                return;
            }
        }
        chords.push_back({ name, barPos, Chord::Midi });
        std::sort(chords.begin(), chords.end());
    }

    void removeChord(int index)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= 0 && index < (int)chords.size())
            chords.erase(chords.begin() + index);
    }

    std::vector<Chord> getChords() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return chords;
    }

    void clearMidiChords()
    {
        std::lock_guard<std::mutex> lock(mutex);
        chords.erase(std::remove_if(chords.begin(), chords.end(),
                     [](const Chord& c) { return c.source == Chord::Midi; }),
                     chords.end());
    }

    // Returns the active chord at a given bar position
    juce::String getChordAt(double barPos) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        juce::String result;
        for (const auto& c : chords)
        {
            if (c.barPosition <= barPos)
                result = c.name;
            else
                break;
        }
        return result;
    }

    // --- Section operations ---
    void addSection(const Section& section)
    {
        std::lock_guard<std::mutex> lock(mutex);
        sections.push_back(section);
        std::sort(sections.begin(), sections.end(),
                  [](const Section& a, const Section& b) { return a.startBar < b.startBar; });
    }

    void removeSection(int index)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= 0 && index < (int)sections.size())
            sections.erase(sections.begin() + index);
    }

    void updateSection(int index, const Section& section)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= 0 && index < (int)sections.size())
            sections[static_cast<size_t>(index)] = section;
    }

    std::vector<Section> getSections() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return sections;
    }

    juce::String getSectionAt(double barPos) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = sections.rbegin(); it != sections.rend(); ++it)
        {
            if (barPos >= it->startBar && barPos <= it->endBar)
                return it->name;
        }
        return {};
    }

    // --- Lyrics operations ---
    void addLyricLine(const LyricLine& line)
    {
        std::lock_guard<std::mutex> lock(mutex);
        lyrics.push_back(line);
        std::sort(lyrics.begin(), lyrics.end(),
                  [](const LyricLine& a, const LyricLine& b) { return a.startBar < b.startBar; });
    }

    void removeLyricLine(int index)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= 0 && index < (int)lyrics.size())
            lyrics.erase(lyrics.begin() + index);
    }

    void updateLyricLine(int index, const LyricLine& line)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= 0 && index < (int)lyrics.size())
            lyrics[static_cast<size_t>(index)] = line;
    }

    void setLyrics(const std::vector<LyricLine>& newLyrics)
    {
        std::lock_guard<std::mutex> lock(mutex);
        lyrics = newLyrics;
    }

    void setSections(const std::vector<Section>& newSections)
    {
        std::lock_guard<std::mutex> lock(mutex);
        sections = newSections;
    }

    std::vector<LyricLine> getLyrics() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return lyrics;
    }

    // --- Serialization ---
    juce::XmlElement* toXml() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto* root = new juce::XmlElement("JimmySong");
        root->setAttribute("version", 1);

        auto* chordsXml = root->createNewChildElement("Chords");
        for (const auto& c : chords)
        {
            auto* el = chordsXml->createNewChildElement("Chord");
            el->setAttribute("name", c.name);
            el->setAttribute("bar", c.barPosition);
            el->setAttribute("source", c.source == Chord::Manual ? "manual" : "midi");
        }

        auto* sectionsXml = root->createNewChildElement("Sections");
        for (const auto& s : sections)
        {
            auto* el = sectionsXml->createNewChildElement("Section");
            el->setAttribute("name", s.name);
            el->setAttribute("startBar", s.startBar);
            el->setAttribute("endBar", s.endBar);
            el->setAttribute("colour", s.colour.toString());
        }

        auto* lyricsXml = root->createNewChildElement("Lyrics");
        for (const auto& l : lyrics)
        {
            auto* el = lyricsXml->createNewChildElement("Line");
            el->setAttribute("text", l.text);
            el->setAttribute("startBar", l.startBar);
            el->setAttribute("endBar", l.endBar);
            el->setAttribute("section", l.sectionIndex);
        }

        return root;
    }

    void fromXml(const juce::XmlElement* root)
    {
        if (root == nullptr || !root->hasTagName("JimmySong"))
            return;

        std::lock_guard<std::mutex> lock(mutex);

        chords.clear();
        if (auto* chordsXml = root->getChildByName("Chords"))
        {
            for (auto* el : chordsXml->getChildIterator())
            {
                Chord c;
                c.name = el->getStringAttribute("name");
                c.barPosition = el->getDoubleAttribute("bar");
                c.source = el->getStringAttribute("source") == "manual" ? Chord::Manual : Chord::Midi;
                chords.push_back(c);
            }
        }

        sections.clear();
        if (auto* sectionsXml = root->getChildByName("Sections"))
        {
            for (auto* el : sectionsXml->getChildIterator())
            {
                Section s;
                s.name = el->getStringAttribute("name");
                s.startBar = el->getIntAttribute("startBar", 1);
                s.endBar = el->getIntAttribute("endBar", 1);
                s.colour = juce::Colour::fromString(el->getStringAttribute("colour", "ff00bcd4"));
                sections.push_back(s);
            }
        }

        lyrics.clear();
        if (auto* lyricsXml = root->getChildByName("Lyrics"))
        {
            for (auto* el : lyricsXml->getChildIterator())
            {
                LyricLine l;
                l.text = el->getStringAttribute("text");
                l.startBar = el->getDoubleAttribute("startBar", 1.0);
                l.endBar = el->getDoubleAttribute("endBar", 2.0);
                l.sectionIndex = el->getIntAttribute("section", -1);
                lyrics.push_back(l);
            }
        }
    }

private:
    mutable std::mutex mutex;
    std::vector<Chord> chords;
    std::vector<Section> sections;
    std::vector<LyricLine> lyrics;
};
