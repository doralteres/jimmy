#include "PluginEditor.h"

namespace
{
    const juce::Colour kBackground  (0xff1a1a1a);
    const juce::Colour kAccent      (0xff00bcd4);
    const juce::Colour kTextPrimary (0xffffffff);
    const juce::Colour kTextDim     (0xffaaaaaa);
    const juce::Colour kChordColour (0xff00e5ff);
    const juce::Colour kPlayingGreen(0xff4caf50);
    const juce::Colour kRecordingRed(0xfff44336);
    const juce::Colour kPanelBg     (0xff252525);

    const int kTransportBarHeight = 100;
    const int kToolbarHeight      = 36;

    juce::String noteNumberToName(int noteNumber)
    {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                           "F#", "G", "G#", "A", "A#", "B" };
        int octave = (noteNumber / 12) - 1;
        return juce::String(noteNames[noteNumber % 12]) + juce::String(octave);
    }
}

JimmyEditor::JimmyEditor(JimmyProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      lyricsEditor(p.songModel)
{
    setSize(900, 700);
    setResizable(true, true);
    setResizeLimits(600, 450, 2000, 1500);

    // Mode toggle button
    modeToggleBtn.setButtonText("LIVE MODE");
    modeToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7c4dff));
    modeToggleBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    modeToggleBtn.onClick = [this]
    {
        currentMode = (currentMode == Mode::Edit) ? Mode::Live : Mode::Edit;
        modeToggleBtn.setButtonText(currentMode == Mode::Edit ? "LIVE MODE" : "EDIT MODE");
        resized();
        repaint();
    };
    addAndMakeVisible(modeToggleBtn);

    addAndMakeVisible(lyricsEditor);

    // Teleprompter + zoom controls
    addChildComponent(teleprompterView);
    zoomInBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    zoomInBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    zoomInBtn.onClick = [this] { teleprompterView.setFontSize(teleprompterView.getFontSize() + 4.0f); };
    addChildComponent(zoomInBtn);

    zoomOutBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    zoomOutBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    zoomOutBtn.onClick = [this] { teleprompterView.setFontSize(teleprompterView.getFontSize() - 4.0f); };
    addChildComponent(zoomOutBtn);

    // Help button (always visible)
    helpBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    helpBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    helpBtn.onClick = [this] { showHelpPopup(); };
    addAndMakeVisible(helpBtn);

    startTimerHz(30);
}

JimmyEditor::~JimmyEditor()
{
    stopTimer();
}

void JimmyEditor::timerCallback()
{
    displayBpm         = processorRef.transportState.bpm.load(std::memory_order_relaxed);
    displayTimeSigNum  = processorRef.transportState.timeSigNum.load(std::memory_order_relaxed);
    displayTimeSigDen  = processorRef.transportState.timeSigDen.load(std::memory_order_relaxed);
    displayPpq         = processorRef.transportState.ppqPosition.load(std::memory_order_relaxed);
    displayBarStart    = processorRef.transportState.barStartPpq.load(std::memory_order_relaxed);
    displayBarCount    = processorRef.transportState.barCount.load(std::memory_order_relaxed);
    displayIsPlaying   = processorRef.transportState.isPlaying.load(std::memory_order_relaxed);
    displayIsRecording = processorRef.transportState.isRecording.load(std::memory_order_relaxed);
    displayHeldNotes   = processorRef.midiNoteState.getHeldNotes();
    displayCurrentChord = processorRef.currentDetectedChord;

    // Drain chord events from the lock-free FIFO into SongModel (UI thread safe)
    ChordEvent chordEvt;
    while (processorRef.chordEventFifo.pop(chordEvt))
        processorRef.songModel.setMidiChordAt(chordEvt.barPosition, chordEvt.getName());

    // Import toast countdown
    if (importToastCountdown > 0)
        --importToastCountdown;

    double currentBar = static_cast<double>(displayBarCount) + 1.0;
    displayCurrentSection = processorRef.songModel.getSectionAt(currentBar);

    // Feed teleprompter with current data
    if (currentMode == Mode::Live)
    {
        teleprompterView.setSongData(
            processorRef.songModel.getLyrics(),
            processorRef.songModel.getChords(),
            processorRef.songModel.getSections());
        teleprompterView.setPosition(currentBar);
    }

    repaint();
}

void JimmyEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBackground);

    auto area = getLocalBounds();

    // Transport bar at top (always visible)
    paintTransportBar(g, area.removeFromTop(kTransportBarHeight));

    if (currentMode == Mode::Live)
    {
        // Teleprompter component handles live mode rendering
    }

    // Show chord import hint when no chords exist
    if (processorRef.songModel.getChords().empty() && !isDragOver && importToastCountdown <= 0)
    {
        auto hintArea = getLocalBounds().withTrimmedTop(kTransportBarHeight + kToolbarHeight);
        auto hintBounds = hintArea.reduced(40).withHeight(80).withY(hintArea.getCentreY() - 40);

        g.setColour(juce::Colour(0x22ffffff));
        g.fillRoundedRectangle(hintBounds.toFloat(), 10.0f);

        // Dashed border
        g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.3f));
        auto r = hintBounds.toFloat().reduced(1.0f);
        for (float x = r.getX(); x < r.getRight(); x += 16.0f)
        {
            float w = std::min(10.0f, r.getRight() - x);
            g.fillRect(x, r.getY(), w, 1.5f);
            g.fillRect(x, r.getBottom() - 1.5f, w, 1.5f);
        }
        for (float yy = r.getY(); yy < r.getBottom(); yy += 16.0f)
        {
            float h = std::min(10.0f, r.getBottom() - yy);
            g.fillRect(r.getX(), yy, 1.5f, h);
            g.fillRect(r.getRight() - 1.5f, yy, 1.5f, h);
        }

        g.setColour(juce::Colour(Theme::kTextDim));
        g.setFont(juce::Font(juce::FontOptions(16.0f)));
        g.drawText("Drag a MIDI file here to import chords", hintBounds.removeFromTop(44),
                   juce::Justification::centredBottom);
        g.setColour(juce::Colour(Theme::kTextFaded));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Cubase: Project > Chord Track > Chords to MIDI, then drag the MIDI part",
                   hintBounds, juce::Justification::centredTop);
    }

    // Import feedback toast
    if (importToastCountdown > 0 && importToastMessage.isNotEmpty())
    {
        auto toastBounds = getLocalBounds().removeFromBottom(50).reduced(40, 8);
        g.setColour(juce::Colour(0xcc333333));
        g.fillRoundedRectangle(toastBounds.toFloat(), 6.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawText(importToastMessage, toastBounds, juce::Justification::centred);
    }

    // Drop overlay on top of everything
    if (isDragOver)
        paintDropOverlay(g);
}

void JimmyEditor::paintTransportBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(kPanelBg);
    g.fillRect(area);

    auto inner = area.reduced(12, 8);

    // Status indicator (left side)
    auto statusArea = inner.removeFromLeft(100);
    juce::String statusText;
    if (displayIsRecording)
    {
        g.setColour(kRecordingRed);
        statusText = "REC";
    }
    else if (displayIsPlaying)
    {
        g.setColour(kPlayingGreen);
        statusText = "PLAY";
    }
    else
    {
        g.setColour(kTextDim);
        statusText = "STOP";
    }
    g.setFont(juce::Font(juce::FontOptions(16.0f)));
    g.drawText(statusText, statusArea.removeFromTop(24), juce::Justification::centredLeft);

    // Current chord in transport bar
    if (displayCurrentChord.isNotEmpty())
    {
        g.setColour(kChordColour);
        g.setFont(juce::Font(juce::FontOptions(24.0f)));
        g.drawText(displayCurrentChord, statusArea, juce::Justification::centredLeft);
    }
    else if (!displayIsPlaying)
    {
        // Hint: show drag-drop instruction when no chords and stopped
        g.setColour(kTextDim.withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::CharPointer_UTF8("\xf0\x9f\x8e\xb5 Drag .mid here"), statusArea, juce::Justification::centredLeft);
    }

    // Transport info columns
    int colWidth = inner.getWidth() / 4;
    auto drawInfo = [&](int col, const juce::String& label, const juce::String& value)
    {
        auto colArea = inner.withX(inner.getX() + col * colWidth).withWidth(colWidth);
        g.setColour(kTextDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(label, colArea.removeFromTop(18), juce::Justification::centred);
        g.setColour(kAccent);
        g.setFont(juce::Font(juce::FontOptions(20.0f)));
        g.drawText(value, colArea.removeFromTop(28), juce::Justification::centred);
    };

    double ppqPerBeat = 4.0 / displayTimeSigDen;
    double ppqInBar = displayPpq - displayBarStart;
    double beatInBar = (ppqPerBeat > 0.0) ? (ppqInBar / ppqPerBeat) + 1.0 : 1.0;

    drawInfo(0, "BAR",      juce::String(displayBarCount + 1));
    drawInfo(1, "BEAT",     juce::String(beatInBar, 1));
    drawInfo(2, "BPM",      juce::String(displayBpm, 1));
    drawInfo(3, "TIME SIG", juce::String(displayTimeSigNum) + "/" + juce::String(displayTimeSigDen));

    // MIDI notes (small, bottom of transport bar)
    if (!displayHeldNotes.empty())
    {
        auto notesRow = area.withTrimmedTop(area.getHeight() - 18).reduced(12, 0);
        int x = notesRow.getX();
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        for (int note : displayHeldNotes)
        {
            g.setColour(kChordColour.withAlpha(0.7f));
            g.drawText(noteNumberToName(note), x, notesRow.getY(), 32, 16, juce::Justification::centredLeft);
            x += 34;
            if (x > notesRow.getRight() - 40) break;
        }
    }

    // Bottom border
    g.setColour(kTextDim.withAlpha(0.2f));
    g.drawHorizontalLine(area.getBottom() - 1, (float)area.getX(), (float)area.getRight());
}

void JimmyEditor::paintChordDisplay(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Section name
    if (displayCurrentSection.isNotEmpty())
    {
        auto sectionArea = area.removeFromTop(40);
        g.setColour(kAccent);
        g.setFont(juce::Font(juce::FontOptions(20.0f)));
        g.drawText(displayCurrentSection, sectionArea, juce::Justification::centred);
    }

    // Big chord name
    if (displayCurrentChord.isNotEmpty())
    {
        g.setColour(kChordColour);
        g.setFont(juce::Font(juce::FontOptions(72.0f)));
        g.drawText(displayCurrentChord, area, juce::Justification::centred);
    }
    else
    {
        g.setColour(kTextDim.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(28.0f)));
        g.drawText("Waiting for chords...", area, juce::Justification::centred);
    }
}

void JimmyEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kTransportBarHeight);

    // Toolbar: mode toggle + help + tabs
    auto toolbar = area.removeFromTop(kToolbarHeight);
    modeToggleBtn.setBounds(toolbar.removeFromRight(110).reduced(4));
    helpBtn.setBounds(toolbar.removeFromRight(36).reduced(4));

    if (currentMode == Mode::Edit)
    {
        lyricsEditor.setVisible(true);
        teleprompterView.setVisible(false);
        zoomInBtn.setVisible(false);
        zoomOutBtn.setVisible(false);

        lyricsEditor.setBounds(area);
    }
    else
    {
        // Live mode: show teleprompter + zoom controls
        lyricsEditor.setVisible(false);

        // Zoom buttons in toolbar
        zoomOutBtn.setVisible(true);
        zoomInBtn.setVisible(true);
        zoomOutBtn.setBounds(toolbar.removeFromLeft(36).reduced(4));
        zoomInBtn.setBounds(toolbar.removeFromLeft(36).reduced(4));

        teleprompterView.setVisible(true);
        teleprompterView.setBounds(area);
    }
}

// --- FileDragAndDropTarget ---

bool JimmyEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi"))
            return true;
    return false;
}

void JimmyEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    isDragOver = true;
    repaint();
}

void JimmyEditor::fileDragExit(const juce::StringArray&)
{
    isDragOver = false;
    repaint();
}

void JimmyEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDragOver = false;

    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi"))
        {
            importMidiFile(juce::File(f));
            break;
        }
    }

    repaint();
}

void JimmyEditor::importMidiFile(const juce::File& file)
{
    auto result = MidiChordImporter::importFromFile(file);

    if (!result.success)
    {
        importToastMessage = result.errorMessage;
        importToastCountdown = 120;  // ~4 seconds at 30Hz
        return;
    }

    if (result.chords.empty())
    {
        importToastMessage = "No chords detected in MIDI file.";
        importToastCountdown = 120;
        return;
    }

    // Clear existing MIDI chords and add imported ones
    processorRef.songModel.clearMidiChords();
    for (const auto& chord : result.chords)
        processorRef.songModel.addChord(chord);

    importToastMessage = "Imported " + juce::String((int)result.chords.size()) + " chords from " + file.getFileName();
    importToastCountdown = 120;
}

void JimmyEditor::paintDropOverlay(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xaa000000));
    g.fillRect(getLocalBounds());

    auto bounds = getLocalBounds().reduced(40);
    g.setColour(juce::Colour(0xff00bcd4));

    // Dashed border effect
    float dashLength = 12.0f;
    float gapLength = 8.0f;
    auto rect = bounds.toFloat();

    for (float x = rect.getX(); x < rect.getRight(); x += dashLength + gapLength)
    {
        float w = std::min(dashLength, rect.getRight() - x);
        g.fillRect(x, rect.getY(), w, 2.0f);
        g.fillRect(x, rect.getBottom() - 2.0f, w, 2.0f);
    }
    for (float y = rect.getY(); y < rect.getBottom(); y += dashLength + gapLength)
    {
        float h = std::min(dashLength, rect.getBottom() - y);
        g.fillRect(rect.getX(), y, 2.0f, h);
        g.fillRect(rect.getRight() - 2.0f, y, 2.0f, h);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(24.0f)));
    g.drawText("Drop MIDI file to import chords", bounds, juce::Justification::centred);
}

void JimmyEditor::showHelpPopup()
{
    auto helpText = juce::String(
        "JIMMY - Live Teleprompter for Cubase\n"
        "=====================================\n\n"

        "LYRICS FORMAT\n"
        "Enter lyrics in Edit Mode > Lyrics tab. One phrase per line.\n"
        "Click 'Apply Lyrics' to map them to the timeline.\n\n"

        "SECTION MARKERS\n"
        "Add [Section Name] on its own line to create sections:\n"
        "  [Verse 1]\n"
        "  First line of verse\n"
        "  [Chorus]\n"
        "  Chorus line\n\n"

        "TIMELINE DIRECTIVES\n"
        "  [break: N]       Skip N bars before the next line\n"
        "  Line [length: N] Set this line's duration to N bars\n"
        "  (default is 2 bars per line)\n\n"

        "Example:\n"
        "  [Verse 1]\n"
        "  [break: 2]\n"
        "  Hello world [length: 4]\n"
        "  Second line [length: 3]\n\n"

        "CHORDS\n"
        "Route your Cubase chord track MIDI to Jimmy's instrument\n"
        "track. Chords are detected automatically and displayed\n"
        "above lyrics in Live Mode.\n\n"

        "IMPORTING CHORDS FROM MIDI\n"
        "To import all chords at once:\n"
        "1. In Cubase: Project > Chord Track > Chords to MIDI\n"
        "2. Drag the resulting MIDI part from the arrangement\n"
        "   onto Jimmy's plugin window\n"
        "3. Or export a .mid file and drag that onto the window\n"
        "All chords will be placed at their correct bar positions.\n"
        "Use 'Clear MIDI Chords' in Edit mode to remove them.\n\n"

        "LIVE MODE\n"
        "Click 'LIVE MODE' to switch to the teleprompter view.\n"
        "Use +/- to zoom. The display auto-scrolls with the DAW.\n"
        "All chords are shown above each lyric line at their\n"
        "proportional position. The active chord is brighter.\n"
    );

    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "Jimmy Help",
        helpText,
        "OK",
        this);
}
