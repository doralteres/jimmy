#include "PluginEditor.h"
#include "Theme.h"
#include "MidiImporter.h"
#include "MidiExporter.h"

namespace
{
    const int kTransportBarHeight = 80;
    const int kToolbarHeight      = 40;

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

    // Mode toggle button — dominant amber accent CTA
    modeToggleBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x96\xb6 LIVE"));
    modeToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kAccent));
    modeToggleBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kBackground));
    modeToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kBackground));
    modeToggleBtn.onClick = [this]
    {
        currentMode = (currentMode == Mode::Edit) ? Mode::Live : Mode::Edit;
        if (currentMode == Mode::Edit)
        {
            modeToggleBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x96\xb6 LIVE"));
            modeToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kAccent));
            modeToggleBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kBackground));
            modeToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kBackground));
        }
        else
        {
            modeToggleBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x9c\x8e EDIT"));
            modeToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSurfaceLight));
            modeToggleBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextSecondary));
            modeToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kTextSecondary));
        }
        resized();
        repaint();
    };
    addAndMakeVisible(modeToggleBtn);

    addAndMakeVisible(lyricsEditor);

    // Teleprompter + zoom controls
    addChildComponent(teleprompterView);
    zoomInBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSurfaceLight));
    zoomInBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextSecondary));
    zoomInBtn.onClick = [this] { teleprompterView.setFontSize(teleprompterView.getFontSize() + 4.0f); };
    addChildComponent(zoomInBtn);

    zoomOutBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSurfaceLight));
    zoomOutBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextSecondary));
    zoomOutBtn.onClick = [this] { teleprompterView.setFontSize(teleprompterView.getFontSize() - 4.0f); };
    addChildComponent(zoomOutBtn);

    // Help button — subtle circle
    helpBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSurfaceLight));
    helpBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextSecondary));
    helpBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kTextSecondary));
    helpBtn.onClick = [this] { showHelpPopup(); };
    addAndMakeVisible(helpBtn);

    // Live source toggle button
    liveSourceBtn.onClick = [this]
    {
        auto mode = processorRef.songModel.getLiveSourceMode();
        auto newMode = (mode == LiveSourceMode::LiveInput)
                           ? LiveSourceMode::FromEditor
                           : LiveSourceMode::LiveInput;
        processorRef.songModel.setLiveSourceMode(newMode);
        processorRef.transportState.liveSourceMode.store(
            newMode == LiveSourceMode::FromEditor ? 1 : 0,
            std::memory_order_relaxed);
        updateLiveSourceButton();
    };
    updateLiveSourceButton();
    addAndMakeVisible(liveSourceBtn);

    // Export drag zone (Edit mode)
    addChildComponent(exportDragZone);

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

    // Drain chord events from the lock-free FIFO into SongModel (UI thread safe)
    ChordEvent chordEvt;
    while (processorRef.chordEventFifo.pop(chordEvt))
    {
        processorRef.songModel.setMidiChordAt(chordEvt.barPosition, chordEvt.getName());
        displayCurrentChord = chordEvt.getName();
    }

    // Clear chord display when no notes are held
    if (displayHeldNotes.empty() && processorRef.songModel.getLiveSourceMode() == LiveSourceMode::LiveInput)
        displayCurrentChord = {};

    // Drain SysEx song data from the lock-free FIFO (audio thread → UI thread)
    SongDataEvent songEvt;
    while (processorRef.songDataFifo.pop(songEvt))
    {
        if (songEvt.isSongEnd)
        {
            processorRef.songModel.loadSongData({});
            lyricsEditor.refreshFromModel();
            sysExSongStartBar = 0.0;
            continue;
        }

        auto xmlString = MidiExporter::decompressSongXml(songEvt.data, songEvt.size);
        if (xmlString.isEmpty())
            continue;

        auto songData = SongModel::songFromXml(xmlString);
        if (songData.lyrics.empty() && songData.chords.empty())
            continue;

        // transportBar from FIFO = absoluteStartBar of this song
        sysExSongStartBar = songEvt.transportBar;

        processorRef.songModel.loadSongData(songData);
        lyricsEditor.refreshFromModel();
    }

    // Import toast countdown
    if (importToastCountdown > 0)
        --importToastCountdown;

    // Compute fractional bar position so sub-bar chords (e.g. at beat 3 of 4)
    // are highlighted exactly when the playhead reaches them, not just at bar boundaries.
    double ppqPerBar = displayTimeSigNum * (4.0 / juce::jmax(1, displayTimeSigDen));
    double fracInBar = (ppqPerBar > 0.0)
                           ? juce::jlimit(0.0, 0.9999, (displayPpq - displayBarStart) / ppqPerBar)
                           : 0.0;
    double currentBar = static_cast<double>(displayBarCount) + 1.0 + fracInBar;
    displayCurrentSection = processorRef.songModel.getSectionAt(currentBar);

    // Feed teleprompter with current data
    if (currentMode == Mode::Live)
    {
        auto liveMode = processorRef.songModel.getLiveSourceMode();

        if (liveMode == LiveSourceMode::FromEditor)
        {
            // Multi-clip: find active clip and use its data with relative bar
            auto clips = processorRef.songModel.getClips();
            int activeIdx = processorRef.songModel.getActiveClipIndex(currentBar);

            if (activeIdx >= 0)
            {
                const auto& clip = clips[static_cast<size_t>(activeIdx)];
                double relativeBar = currentBar - clip.absoluteStartBar + 1.0;

                teleprompterView.setSongData(clip.lyrics, clip.chords, clip.sections);
                teleprompterView.setPosition(relativeBar);
                lastActiveClipIndex = activeIdx;
            }
            else if (!clips.empty())
            {
                // In a gap between clips — show blank
                if (lastActiveClipIndex >= 0)
                {
                    teleprompterView.setSongData({}, {}, {});
                    lastActiveClipIndex = -1;
                }
            }
            else
            {
                // No clips — use main SongModel data (single-song "From Editor" mode)
                teleprompterView.setSongData(
                    processorRef.songModel.getLyrics(),
                    processorRef.songModel.getChords(),
                    processorRef.songModel.getSections());
                teleprompterView.setPosition(currentBar);
            }
        }
        else
        {
            // Live Input mode (or SysEx-loaded song)
            teleprompterView.setSongData(
                processorRef.songModel.getLyrics(),
                processorRef.songModel.getChords(),
                processorRef.songModel.getSections());

            // If a SysEx song is loaded, compute relative bar position
            if (processorRef.sysExSongActive.load(std::memory_order_relaxed) && sysExSongStartBar > 0.0)
            {
                double relativeBar = currentBar - sysExSongStartBar + 1.0;
                teleprompterView.setPosition(relativeBar);
                displayCurrentSection = processorRef.songModel.getSectionAt(relativeBar);
                displayCurrentChord = processorRef.songModel.getChordAt(relativeBar);
            }
            else
            {
                teleprompterView.setPosition(currentBar);
            }
        }
    }

    // Show/hide export drag zone based on whether there are lyrics
    if (currentMode == Mode::Edit)
    {
        bool hasLyrics = !processorRef.songModel.getLyrics().empty();
        if (exportDragZone.isVisible() != hasLyrics)
        {
            exportDragZone.setVisible(hasLyrics);
            resized();
        }
    }

    repaint();
}

void JimmyEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::kBackground));

    auto area = getLocalBounds();

    // Transport bar at top (always visible)
    paintTransportBar(g, area.removeFromTop(kTransportBarHeight));

    // Toolbar separator
    g.setColour(juce::Colour(Theme::kBorder));
    g.drawHorizontalLine(area.getY() + kToolbarHeight - 1,
                         (float)area.getX(), (float)area.getRight());
}

void JimmyEditor::paintOverChildren(juce::Graphics& g)
{
    // Show chord import hint when no chords exist (Edit mode only)
    if (currentMode == Mode::Edit && processorRef.songModel.getChords().empty() && !isDragOver && importToastCountdown <= 0)
    {
        auto hintArea = getLocalBounds().withTrimmedTop(kTransportBarHeight + kToolbarHeight);
        auto hintBounds = hintArea.removeFromBottom(70).reduced(20, 4);

        g.setColour(juce::Colour(Theme::kSurface));
        g.fillRoundedRectangle(hintBounds.toFloat(), 8.0f);

        // Dashed border
        g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.2f));
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

        g.setColour(juce::Colour(Theme::kTextSecondary));
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawText(juce::CharPointer_UTF8("\xf0\x9f\x8e\xb5 Drag a MIDI file here to import chords"),
                   hintBounds.removeFromTop(36),
                   juce::Justification::centredBottom);
        g.setColour(juce::Colour(Theme::kTextFaded));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("Cubase: Project > Chord Track > Chords to MIDI, then drag the MIDI part",
                   hintBounds, juce::Justification::centredTop);
    }
    // Show chord count indicator when chords are loaded (Edit mode)
    else if (currentMode == Mode::Edit && !processorRef.songModel.getChords().empty() && !isDragOver && importToastCountdown <= 0)
    {
        auto indicatorArea = getLocalBounds().withTrimmedTop(kTransportBarHeight + kToolbarHeight);
        auto indicatorBounds = indicatorArea.removeFromBottom(28).reduced(20, 4);

        g.setColour(juce::Colour(Theme::kTextFaded));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        auto numChords = (int)processorRef.songModel.getChords().size();
        g.drawText(juce::String(numChords) + " chords loaded "
                   + juce::String(juce::CharPointer_UTF8("\xe2\x9c\x93")),
                   indicatorBounds, juce::Justification::centredRight);
    }

    // Import feedback toast
    if (importToastCountdown > 0 && importToastMessage.isNotEmpty())
    {
        auto toastBounds = getLocalBounds().removeFromBottom(50).reduced(40, 8);
        g.setColour(juce::Colour(Theme::kSurface).withAlpha(0.95f));
        g.fillRoundedRectangle(toastBounds.toFloat(), 6.0f);
        g.setColour(juce::Colour(Theme::kBorder));
        g.drawRoundedRectangle(toastBounds.toFloat(), 6.0f, 1.0f);
        g.setColour(juce::Colour(Theme::kTextPrimary));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(importToastMessage, toastBounds, juce::Justification::centred);
    }

    // Drop overlay on top of everything
    if (isDragOver)
        paintDropOverlay(g);
}

void JimmyEditor::paintTransportBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::kPanelBg));
    g.fillRect(area);

    auto inner = area.reduced(12, 6);

    // Status indicator (left side) — circle glyph + chord
    auto statusArea = inner.removeFromLeft(100);

    // Status circle + label
    auto statusRow = statusArea.removeFromTop(22);
    juce::String statusText;
    juce::Colour statusColour;
    if (displayIsRecording)
    {
        statusColour = juce::Colour(Theme::kRecordingRed);
        statusText = "REC";
    }
    else if (displayIsPlaying)
    {
        statusColour = juce::Colour(Theme::kPlayingGreen);
        statusText = "PLAY";
    }
    else
    {
        statusColour = juce::Colour(Theme::kTextFaded);
        statusText = "STOP";
    }
    // Draw status circle
    g.setColour(statusColour);
    float circleY = statusRow.getY() + (statusRow.getHeight() - 8) / 2.0f;
    if (displayIsRecording || displayIsPlaying)
        g.fillEllipse(statusRow.getX() + 0.0f, circleY, 8.0f, 8.0f);
    else
        g.drawEllipse(statusRow.getX() + 0.0f, circleY, 8.0f, 8.0f, 1.5f);

    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(statusText, statusRow.withTrimmedLeft(14), juce::Justification::centredLeft);

    // Current chord in transport bar
    if (displayCurrentChord.isNotEmpty())
    {
        g.setColour(juce::Colour(Theme::kChordColour));
        g.setFont(juce::Font(juce::FontOptions(22.0f)));
        g.drawText(displayCurrentChord, statusArea, juce::Justification::centredLeft);
    }
    else if (!displayIsPlaying)
    {
        g.setColour(juce::Colour(Theme::kTextFaded));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::CharPointer_UTF8("\xf0\x9f\x8e\xb5 Drag .mid here"), statusArea, juce::Justification::centredLeft);
    }

    // Transport info columns
    int colWidth = inner.getWidth() / 4;
    auto drawInfo = [&](int col, const juce::String& label, const juce::String& value)
    {
        auto colArea = inner.withX(inner.getX() + col * colWidth).withWidth(colWidth);
        g.setColour(juce::Colour(Theme::kTextSecondary));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(label, colArea.removeFromTop(16), juce::Justification::centred);
        g.setColour(juce::Colour(Theme::kAccent));
        g.setFont(juce::Font(juce::FontOptions(22.0f)));
        g.drawText(value, colArea.removeFromTop(30), juce::Justification::centred);
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
        auto notesRow = area.withTrimmedTop(area.getHeight() - 16).reduced(12, 0);
        int x = notesRow.getX();
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        for (int note : displayHeldNotes)
        {
            g.setColour(juce::Colour(Theme::kChordColour).withAlpha(0.6f));
            g.drawText(noteNumberToName(note), x, notesRow.getY(), 30, 14, juce::Justification::centredLeft);
            x += 32;
            if (x > notesRow.getRight() - 40) break;
        }
    }

    // Bottom border
    g.setColour(juce::Colour(Theme::kBorder));
    g.drawHorizontalLine(area.getBottom() - 1, (float)area.getX(), (float)area.getRight());
}

void JimmyEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kTransportBarHeight);

    // Toolbar strip
    auto toolbar = area.removeFromTop(kToolbarHeight);
    auto toolbarInner = toolbar.reduced(6, 4);

    modeToggleBtn.setBounds(toolbarInner.removeFromRight(110).reduced(2));
    toolbarInner.removeFromRight(4);
    helpBtn.setBounds(toolbarInner.removeFromRight(30).reduced(1));
    toolbarInner.removeFromRight(4);
    liveSourceBtn.setBounds(toolbarInner.removeFromRight(120).reduced(2));

    if (currentMode == Mode::Edit)
    {
        lyricsEditor.setVisible(true);
        teleprompterView.setVisible(false);
        zoomInBtn.setVisible(false);
        zoomOutBtn.setVisible(false);

        // Export drag zone at bottom of edit area
        bool hasLyrics = !processorRef.songModel.getLyrics().empty();
        if (hasLyrics)
        {
            auto exportArea = area.removeFromBottom(44);
            exportDragZone.setVisible(true);
            exportDragZone.setBounds(exportArea.reduced(16, 4));
        }
        else
        {
            exportDragZone.setVisible(false);
        }

        lyricsEditor.setBounds(area);
    }
    else
    {
        // Live mode: show teleprompter + zoom controls
        lyricsEditor.setVisible(false);
        exportDragZone.setVisible(false);

        // Zoom buttons in toolbar
        zoomOutBtn.setVisible(true);
        zoomInBtn.setVisible(true);
        zoomOutBtn.setBounds(toolbarInner.removeFromLeft(32).reduced(1));
        zoomInBtn.setBounds(toolbarInner.removeFromLeft(32).reduced(1));

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
    auto format = MidiImporter::detectFormat(file);

    if (format == MidiImporter::ImportFormat::JimmyFull)
    {
        auto result = MidiImporter::importFull(file);

        if (!result.success)
        {
            importToastMessage = result.errorMessage;
            importToastCountdown = 120;
            return;
        }

        // Check if clips already exist — offer to add as clip
        auto existingClips = processorRef.songModel.getClips();
        if (!existingClips.empty())
        {
            // Add as a new clip after the last existing clip in time
            double maxExistingEndBar = existingClips.back().absoluteEndBar;
            for (const auto& existingClip : existingClips)
                maxExistingEndBar = std::max(maxExistingEndBar, existingClip.absoluteEndBar);

            SongClip clip;
            clip.absoluteStartBar = maxExistingEndBar + 1.0;
            clip.lyrics = result.lyrics;
            clip.sections = result.sections;
            clip.chords = result.chords;
            clip.defaultBarsPerLine = result.defaultBarsPerLine;
            clip.bpm = result.bpm;
            clip.timeSigNum = result.timeSigNum;
            clip.timeSigDen = result.timeSigDen;

            // Compute absolute end bar from lyrics/chords
            double maxBar = 1.0;
            for (const auto& l : clip.lyrics)
                maxBar = std::max(maxBar, l.endBar);
            for (const auto& c : clip.chords)
                maxBar = std::max(maxBar, c.barPosition + 1.0);
            clip.absoluteEndBar = clip.absoluteStartBar + maxBar - 1.0;

            processorRef.songModel.addClip(clip);

            lyricsEditor.refreshFromModel();

            importToastMessage = "Added clip at bar " + juce::String((int)clip.absoluteStartBar)
                                 + " (" + juce::String((int)result.lyrics.size()) + " lines, "
                                 + juce::String((int)result.chords.size()) + " chords)";
        }
        else
        {
            // First import — replace SongModel data directly
            processorRef.songModel.setLyrics(result.lyrics);
            processorRef.songModel.setSections(result.sections);

            // Replace all chords (clear all sources, add imported)
            processorRef.songModel.clearAllChords();
            for (const auto& chord : result.chords)
                processorRef.songModel.addChord(chord);

            processorRef.songModel.setDefaultBarsPerLine(result.defaultBarsPerLine);

            // Refresh the lyrics editor UI
            lyricsEditor.refreshFromModel();

            importToastMessage = "Imported full song: " + juce::String((int)result.lyrics.size()) + " lines, "
                                 + juce::String((int)result.chords.size()) + " chords, "
                                 + juce::String((int)result.sections.size()) + " sections";
        }

        importToastCountdown = 120;
        return;
    }

    // Chord-only fallback (plain MIDI)
    auto result = MidiChordImporter::importFromFile(file);

    if (!result.success)
    {
        importToastMessage = result.errorMessage;
        importToastCountdown = 120;
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
    g.setColour(juce::Colour(0xcc000000));
    g.fillRect(getLocalBounds());

    auto bounds = getLocalBounds().reduced(40);
    g.setColour(juce::Colour(Theme::kAccent));

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

    g.setColour(juce::Colour(Theme::kTextPrimary));
    g.setFont(juce::Font(juce::FontOptions(22.0f)));
    g.drawText("Drop MIDI file to import", bounds, juce::Justification::centred);
}

void JimmyEditor::updateLiveSourceButton()
{
    auto mode = processorRef.songModel.getLiveSourceMode();
    if (mode == LiveSourceMode::LiveInput)
    {
        liveSourceBtn.setButtonText("LIVE INPUT");
        liveSourceBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kAccentDim));
        liveSourceBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextPrimary));
        liveSourceBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kTextPrimary));
    }
    else
    {
        liveSourceBtn.setButtonText("FROM EDITOR");
        liveSourceBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kPlayingGreen));
        liveSourceBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kBackground));
        liveSourceBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kBackground));
    }
    repaint();
}

// ── ExportDragZone ──

void JimmyEditor::ExportDragZone::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(Theme::kSurface));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Dashed border in accent color
    g.setColour(juce::Colour(Theme::kAccent).withAlpha(0.4f));
    float dashLen = 10.0f, gapLen = 6.0f;
    auto r = bounds.reduced(1.0f);
    for (float x = r.getX(); x < r.getRight(); x += dashLen + gapLen)
    {
        float w = std::min(dashLen, r.getRight() - x);
        g.fillRect(x, r.getY(), w, 1.5f);
        g.fillRect(x, r.getBottom() - 1.5f, w, 1.5f);
    }
    for (float y = r.getY(); y < r.getBottom(); y += dashLen + gapLen)
    {
        float h = std::min(dashLen, r.getBottom() - y);
        g.fillRect(r.getX(), y, 1.5f, h);
        g.fillRect(r.getRight() - 1.5f, y, 1.5f, h);
    }

    g.setColour(juce::Colour(Theme::kAccent));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(juce::CharPointer_UTF8("Drag to Jimmy track \xe2\x86\x97"),
               getLocalBounds(), juce::Justification::centred);
}

void JimmyEditor::ExportDragZone::mouseDown(const juce::MouseEvent&)
{
    // Serialize SongModel to a temp MIDI file
    MidiExporter::ExportContext ctx;
    ctx.bpm = editor.displayBpm;
    ctx.timeSigNum = editor.displayTimeSigNum;
    ctx.timeSigDen = editor.displayTimeSigDen;

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    tempExportFile = tempDir.getChildFile("jimmy_export.mid");
    MidiExporter::writeToFile(editor.processorRef.songModel, ctx, tempExportFile);
    dragStarted = false;
}

void JimmyEditor::ExportDragZone::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragStarted && tempExportFile.existsAsFile() && e.getDistanceFromDragStart() > 4)
    {
        dragStarted = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            { tempExportFile.getFullPathName() }, false);
    }
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

        "IMPORTING FROM MIDI\n"
        "To import all chords at once:\n"
        "1. In Cubase: Project > Chord Track > Chords to MIDI\n"
        "2. Drag the resulting MIDI part from the arrangement\n"
        "   onto Jimmy's plugin window\n"
        "3. Or export a .mid file and drag that onto the window\n"
        "All chords will be placed at their correct bar positions.\n"
        "Use 'Clear MIDI Chords' in Edit mode to remove them.\n\n"

        "EXPORTING TO MIDI\n"
        "When lyrics are loaded, a 'Drag to Jimmy track' zone appears at\n"
        "the bottom of Edit Mode. Drag it onto your Jimmy instrument\n"
        "track in Cubase to create a synced MIDI clip.\n"
        "The clip contains all lyrics, chords, sections and timing\n"
        "encoded as SysEx — it syncs with the DAW timeline.\n"
        "Moving the clip in Cubase automatically moves the song.\n\n"

        "LIVE SOURCE TOGGLE\n"
        "The toolbar has a 'LIVE INPUT' / 'FROM EDITOR' toggle:\n"
        "- LIVE INPUT: chords come from real-time MIDI input\n"
        "- FROM EDITOR: chords and lyrics come from imported data,\n"
        "  using the DAW transport position for timeline sync.\n"
        "  Use this mode with exported MIDI clips for a fully\n"
        "  pre-programmed live show setup.\n\n"

        "MULTI-SONG SETLIST\n"
        "Export multiple songs as MIDI clips and place them\n"
        "sequentially on the same Jimmy instrument track.\n"
        "Jimmy automatically detects which song is playing\n"
        "based on the transport position and displays the\n"
        "correct lyrics and chords. Moving clips in your\n"
        "arrangement keeps everything in sync.\n\n"

        "LIVE MODE\n"
        "Click 'LIVE MODE' to switch to the teleprompter view.\n"
        "Use +/- to zoom. The display auto-scrolls with the DAW.\n"
        "All chords are shown above each lyric line at their\n"
        "proportional position. The active chord is brighter.\n"
    );

    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        juce::String("Jimmy v") + JucePlugin_VersionString + " — Help",
        helpText,
        "OK",
        this);
}
