#pragma once

#include "PluginProcessor.h"
#include "LyricsEditor.h"
#include "TeleprompterView.h"
#include "MidiChordImporter.h"
#include <juce_gui_extra/juce_gui_extra.h>

class JimmyEditor : public juce::AudioProcessorEditor,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer
{
public:
    explicit JimmyEditor(JimmyProcessor&);
    ~JimmyEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void paintTransportBar(juce::Graphics& g, juce::Rectangle<int> area);
    void paintDropOverlay(juce::Graphics& g);
    void showHelpPopup();
    void importMidiFile(const juce::File& file);

    JimmyProcessor& processorRef;

    // Mode
    enum class Mode { Edit, Live };
    Mode currentMode = Mode::Edit;
    juce::TextButton modeToggleBtn;

    // Edit mode components
    LyricsEditor   lyricsEditor;

    // Live mode components
    TeleprompterView teleprompterView;
    juce::TextButton zoomInBtn  { "+" };
    juce::TextButton zoomOutBtn { "-" };
    juce::TextButton helpBtn    { "?" };

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

    // Drag-and-drop state
    bool isDragOver = false;

    // Import feedback toast
    juce::String importToastMessage;
    int importToastCountdown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JimmyEditor)
};
