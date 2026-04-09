#pragma once

#include "PluginProcessor.h"
#include "LyricsEditor.h"
#include "TeleprompterView.h"
#include "MidiChordImporter.h"
#include "MidiImporter.h"
#include "MidiExporter.h"
#include <juce_gui_extra/juce_gui_extra.h>

class JimmyEditor : public juce::AudioProcessorEditor,
                    public juce::DragAndDropContainer,
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
    void paintExportDragZone(juce::Graphics& g, juce::Rectangle<int> area);
    void showHelpPopup();
    void importMidiFile(const juce::File& file);
    void updateLiveSourceButton();
    void updateTransposeLabel();

    JimmyProcessor& processorRef;

    // Mode
    enum class Mode { Edit, Live };
    Mode currentMode = Mode::Edit;
    juce::TextButton modeToggleBtn;

    // Live source toggle
    juce::TextButton liveSourceBtn;

    // Edit mode components
    LyricsEditor   lyricsEditor;

    // Export drag zone (Edit mode)
    class ExportDragZone : public juce::Component
    {
    public:
        ExportDragZone(JimmyEditor& owner) : editor(owner) {}
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
    private:
        JimmyEditor& editor;
        juce::File tempExportFile;
        bool dragStarted = false;
    };
    ExportDragZone exportDragZone { *this };

    // Live mode components
    TeleprompterView teleprompterView;
    juce::TextButton zoomInBtn  { "+" };
    juce::TextButton zoomOutBtn { "-" };
    juce::TextButton helpBtn    { "?" };
    juce::TextButton transposeUpBtn   { "T+" };
    juce::TextButton transposeDownBtn { "T-" };
    juce::Label      transposeLabel;

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

    // Multi-clip tracking (for "From Editor" mode)
    int lastActiveClipIndex = -1;

    // SysEx song tracking: absolute bar where the current SysEx song starts
    double sysExSongStartBar = 0.0;

    // Drag-and-drop state
    bool isDragOver = false;

    // Import feedback toast
    juce::String importToastMessage;
    int importToastCountdown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JimmyEditor)
};
