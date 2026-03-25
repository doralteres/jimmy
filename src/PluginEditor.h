#pragma once

#include "PluginProcessor.h"
#include "SectionManager.h"
#include "LyricsEditor.h"
#include "TeleprompterView.h"
#include <juce_gui_extra/juce_gui_extra.h>

class JimmyEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit JimmyEditor(JimmyProcessor&);
    ~JimmyEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintTransportBar(juce::Graphics& g, juce::Rectangle<int> area);
    void paintChordDisplay(juce::Graphics& g, juce::Rectangle<int> area);

    JimmyProcessor& processorRef;

    // Mode
    enum class Mode { Edit, Live };
    Mode currentMode = Mode::Edit;
    juce::TextButton modeToggleBtn;

    // Edit mode tab buttons
    juce::TextButton sectionsTabBtn { "Sections" };
    juce::TextButton lyricsTabBtn  { "Lyrics" };
    enum class EditTab { Sections, Lyrics };
    EditTab currentTab = EditTab::Lyrics;

    // Edit mode components
    SectionManager sectionManager;
    LyricsEditor   lyricsEditor;

    // Live mode components
    TeleprompterView teleprompterView;
    juce::TextButton zoomInBtn  { "+" };
    juce::TextButton zoomOutBtn { "-" };

    // Cached display values (updated by timer, read by paint)
    double   displayBpm        = 120.0;
    int      displayTimeSigNum = 4;
    int      displayTimeSigDen = 4;
    double   displayPpq        = 0.0;
    double   displayBarStart   = 0.0;
    int64_t  displayBarCount   = 0;
    bool     displayIsPlaying  = false;
    bool     displayIsRecording = false;
    std::vector<int> displayHeldNotes;
    juce::String displayCurrentChord;
    juce::String displayCurrentSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JimmyEditor)
};
