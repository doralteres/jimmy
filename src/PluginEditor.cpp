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
      sectionManager(p.songModel),
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

    // Tab buttons
    auto setupTabBtn = [](juce::TextButton& btn, bool active)
    {
        btn.setColour(juce::TextButton::buttonColourId,
                      active ? juce::Colour(0xff00bcd4) : juce::Colour(0xff333333));
        btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcccccc));
    };

    setupTabBtn(lyricsTabBtn, true);
    setupTabBtn(sectionsTabBtn, false);

    lyricsTabBtn.onClick = [this]
    {
        currentTab = EditTab::Lyrics;
        lyricsTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00bcd4));
        sectionsTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        resized();
    };
    sectionsTabBtn.onClick = [this]
    {
        currentTab = EditTab::Sections;
        sectionsTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00bcd4));
        lyricsTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        resized();
    };

    addAndMakeVisible(lyricsTabBtn);
    addAndMakeVisible(sectionsTabBtn);

    addChildComponent(sectionManager);
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

    // Toolbar: mode toggle + tabs
    auto toolbar = area.removeFromTop(kToolbarHeight);
    modeToggleBtn.setBounds(toolbar.removeFromRight(110).reduced(4));

    if (currentMode == Mode::Edit)
    {
        sectionsTabBtn.setVisible(true);
        lyricsTabBtn.setVisible(true);
        lyricsTabBtn.setBounds(toolbar.removeFromLeft(90).reduced(4));
        sectionsTabBtn.setBounds(toolbar.removeFromLeft(90).reduced(4));

        bool showLyrics = (currentTab == EditTab::Lyrics);
        lyricsEditor.setVisible(showLyrics);
        sectionManager.setVisible(!showLyrics);
        teleprompterView.setVisible(false);
        zoomInBtn.setVisible(false);
        zoomOutBtn.setVisible(false);

        lyricsEditor.setBounds(area);
        sectionManager.setBounds(area);
    }
    else
    {
        // Live mode: show teleprompter + zoom controls
        sectionsTabBtn.setVisible(false);
        lyricsTabBtn.setVisible(false);
        lyricsEditor.setVisible(false);
        sectionManager.setVisible(false);

        // Zoom buttons in toolbar
        zoomOutBtn.setVisible(true);
        zoomInBtn.setVisible(true);
        zoomOutBtn.setBounds(toolbar.removeFromLeft(36).reduced(4));
        zoomInBtn.setBounds(toolbar.removeFromLeft(36).reduced(4));

        teleprompterView.setVisible(true);
        teleprompterView.setBounds(area);
    }
}
