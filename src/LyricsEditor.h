#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongModel.h"
#include "Theme.h"

// Lyrics editor: enter lyrics text and map each line to a bar range.
class LyricsEditor : public juce::Component,
                     public juce::TableListBoxModel
{
public:
    explicit LyricsEditor(SongModel& model)
        : songModel(model)
    {
        // Bulk text editor — refined dark surface
        bulkEditor.setMultiLine(true, true);
        bulkEditor.setReturnKeyStartsNewLine(true);
        bulkEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::kSurface));
        bulkEditor.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::kTextPrimary));
        bulkEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::kBorder));
        bulkEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(Theme::kAccentDim));
        bulkEditor.setColour(juce::TextEditor::highlightColourId, juce::Colour(Theme::kAccent).withAlpha((uint8_t)40));
        bulkEditor.setFont(juce::Font(juce::FontOptions(16.0f)));
        bulkEditor.setTextToShowWhenEmpty(
            "[Verse 1]\n"
            "[break: 2]\n"
            "First line [length: 4]\n"
            "Second line [length: 3]\n\n"
            "[Chorus]\n"
            "Chorus line one",
            juce::Colour(Theme::kTextFaded));
        bulkEditor.onTextChange = [this] { updateEditorJustification(); };
        addAndMakeVisible(bulkEditor);

        // Format hint label
        formatHintLabel.setText(
            "Sections: [Verse 1]   Break: [break: 2]   Length: Line text [length: 4]",
            juce::dontSendNotification);
        formatHintLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        formatHintLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::kTextFaded));
        formatHintLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(formatHintLabel);

        // Apply Lyrics — primary accent button
        parseBtn.setButtonText("APPLY LYRICS");
        parseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kAccent));
        parseBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kBackground));
        parseBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kBackground));
        parseBtn.onClick = [this] { parseBulkText(); };
        addAndMakeVisible(parseBtn);

        // Auto-distribute — success green
        autoDistBtn.setButtonText("AUTO-DISTRIBUTE");
        autoDistBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSuccess));
        autoDistBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kBackground));
        autoDistBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kBackground));
        autoDistBtn.onClick = [this] { autoDistribute(); };
        addAndMakeVisible(autoDistBtn);

        // Clear MIDI chords — desaturated danger
        clearChordsBtn.setButtonText("CLEAR CHORDS");
        clearChordsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kDanger));
        clearChordsBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextPrimary));
        clearChordsBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kTextPrimary));
        clearChordsBtn.onClick = [this] { songModel.clearMidiChords(); };
        addAndMakeVisible(clearChordsBtn);

        // Bar mode toggle button (Start/End vs Length) — secondary surface
        barModeBtn.setButtonText("LENGTH MODE");
        barModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSurfaceLight));
        barModeBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kTextSecondary));
        barModeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kTextSecondary));
        barModeBtn.onClick = [this] { toggleBarMode(); };
        addAndMakeVisible(barModeBtn);

        // Add Break button (visible only in Length mode) — accent outline style
        addBreakBtn.setButtonText("+ ADD BREAK");
        addBreakBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::kSurface));
        addBreakBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Theme::kAccent));
        addBreakBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::kAccent));
        addBreakBtn.onClick = [this] { addBreak(); };
        addBreakBtn.setVisible(false);
        addAndMakeVisible(addBreakBtn);

        // Default bars per line setting
        defaultBarsLabel.setText("Bars/line:", juce::dontSendNotification);
        defaultBarsLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        defaultBarsLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::kTextSecondary));
        defaultBarsLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(defaultBarsLabel);

        defaultBarsEditor.setText(juce::String(songModel.getDefaultBarsPerLine(), 1), false);
        defaultBarsEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::kSurfaceLight));
        defaultBarsEditor.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::kTextPrimary));
        defaultBarsEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::kBorder));
        defaultBarsEditor.setFont(juce::Font(juce::FontOptions(12.0f)));
        defaultBarsEditor.setJustification(juce::Justification::centred);
        defaultBarsEditor.setInputRestrictions(5, "0123456789.");
        defaultBarsEditor.onReturnKey = [this] {
            double val = defaultBarsEditor.getText().getDoubleValue();
            if (val > 0.0)
            {
                songModel.setDefaultBarsPerLine(val);
                defaultBarsEditor.setText(juce::String(songModel.getDefaultBarsPerLine(), 1), false);
            }
        };
        defaultBarsEditor.onFocusLost = [this] {
            double val = defaultBarsEditor.getText().getDoubleValue();
            if (val > 0.0)
                songModel.setDefaultBarsPerLine(val);
            defaultBarsEditor.setText(juce::String(songModel.getDefaultBarsPerLine(), 1), false);
        };
        addAndMakeVisible(defaultBarsEditor);

        // Bar mapping table — refined dark surface
        addAndMakeVisible(mappingTable);
        mappingTable.setModel(this);
        mappingTable.setColour(juce::ListBox::backgroundColourId, juce::Colour(Theme::kSurface));
        mappingTable.setRowHeight(28);

        auto& header = mappingTable.getHeader();
        header.addColumn("Line",      1, 250);
        header.addColumn("Start Bar", 2, 90);
        header.addColumn("End Bar",   3, 90);
        header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(Theme::kPanelBg));
        header.setColour(juce::TableHeaderComponent::textColourId, juce::Colour(Theme::kTextSecondary));

        refreshFromModel();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // ZONE B: Primary toolbar — Apply, Auto-distribute, Default bars/line
        auto primaryBar = area.removeFromTop(34).reduced(4, 2);
        parseBtn.setBounds(primaryBar.removeFromLeft(120));
        primaryBar.removeFromLeft(6);
        autoDistBtn.setBounds(primaryBar.removeFromLeft(140));
        primaryBar.removeFromLeft(12);
        defaultBarsLabel.setBounds(primaryBar.removeFromLeft(70));
        primaryBar.removeFromLeft(4);
        defaultBarsEditor.setBounds(primaryBar.removeFromLeft(50));

        // ZONE C: Lyrics editor — format hint + text area (~45% of remaining)
        auto editorHeight = (area.getHeight() * 45) / 100;
        auto editorArea = area.removeFromTop(editorHeight);
        formatHintLabel.setBounds(editorArea.removeFromTop(18).reduced(6, 2));
        bulkEditor.setBounds(editorArea.reduced(4, 2));

        // ZONE D: Table toolbar — Length Mode, + Add Break, Clear MIDI Chords
        auto tableBar = area.removeFromTop(32).reduced(4, 2);
        barModeBtn.setBounds(tableBar.removeFromLeft(120));
        tableBar.removeFromLeft(6);
        if (lengthMode)
        {
            addBreakBtn.setBounds(tableBar.removeFromLeft(110));
            tableBar.removeFromLeft(6);
        }
        clearChordsBtn.setBounds(tableBar.removeFromLeft(130));

        // ZONE E: Line table — fills remaining space
        area.removeFromTop(2);
        mappingTable.setBounds(area.reduced(4, 0));
        mappingTable.setBounds(area.reduced(2));
    }

    void refreshFromModel()
    {
        cachedLyrics = songModel.getLyrics();
        auto sections = songModel.getSections();

        defaultBarsEditor.setText(juce::String(songModel.getDefaultBarsPerLine(), 1), false);

        // Rebuild bulk text with section markers and break directives
        juce::String fullText;
        int lastSectionIdx = -1;
        for (const auto& line : cachedLyrics)
        {
            if (line.sectionIndex >= 0 && line.sectionIndex != lastSectionIdx
                && line.sectionIndex < (int)sections.size())
            {
                lastSectionIdx = line.sectionIndex;
                if (fullText.isNotEmpty())
                    fullText += "\n";
                fullText += "[" + sections[static_cast<size_t>(line.sectionIndex)].name + "]";
            }
            if (fullText.isNotEmpty())
                fullText += "\n";

            if (line.isBreak)
            {
                double breakLen = line.endBar - line.startBar;
                fullText += "[break: " + juce::String(breakLen, 1) + "]";
            }
            else
            {
                fullText += line.text;
            }
        }
        bulkEditor.setText(fullText, false);

        rebuildDisplayIndices();
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    // TableListBoxModel
    int getNumRows() override { return (int)displayIndices.size(); }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int, bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            // Selected row: accent tint + left accent bar
            g.fillAll(juce::Colour(Theme::kAccent).withAlpha(0.12f));
            g.setColour(juce::Colour(Theme::kAccent));
            g.fillRect(0, 0, 3, (int)mappingTable.getRowHeight());
        }
        else
        {
            auto baseCol = (rowNumber % 2 == 0) ? juce::Colour(Theme::kSurface)
                                                 : juce::Colour(Theme::kSurfaceLight);
            g.fillAll(baseCol);
        }

        // Subtle bottom border
        g.setColour(juce::Colour(Theme::kBorder).withAlpha(0.4f));
        g.drawHorizontalLine((int)mappingTable.getRowHeight() - 1, 0.0f, (float)width);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool) override
    {
        if (rowNumber < 0 || rowNumber >= (int)displayIndices.size())
            return;
        int modelIdx = displayIndices[static_cast<size_t>(rowNumber)];
        if (modelIdx < 0 || modelIdx >= (int)cachedLyrics.size())
            return;

        const auto& line = cachedLyrics[static_cast<size_t>(modelIdx)];
        g.setFont(juce::Font(juce::FontOptions(13.0f)));

        juce::String text;
        if (lengthMode)
        {
            switch (columnId)
            {
                case 1:
                    if (line.isBreak)
                    {
                        g.setColour(juce::Colour(Theme::kAccent));
                        text = "-- BREAK --";
                    }
                    else
                    {
                        g.setColour(juce::Colour(Theme::kTextPrimary));
                        g.setFont(juce::Font(juce::FontOptions(14.0f)));
                        text = line.text;
                    }
                    break;
                case 2:
                {
                    g.setColour(juce::Colour(Theme::kTextSecondary));
                    double len = line.endBar - line.startBar;
                    text = juce::String(len, 1) + " bars";
                    break;
                }
                case 4:
                    if (line.isBreak)
                    {
                        g.setColour(juce::Colour(Theme::kDanger));
                        text = "X";
                    }
                    break;
                default: break;
            }
        }
        else
        {
            switch (columnId)
            {
                case 1:
                    g.setColour(juce::Colour(Theme::kTextPrimary));
                    g.setFont(juce::Font(juce::FontOptions(14.0f)));
                    text = line.text;
                    break;
                case 2:
                    g.setColour(juce::Colour(Theme::kTextSecondary));
                    text = juce::String(line.startBar, 1);
                    break;
                case 3:
                    g.setColour(juce::Colour(Theme::kTextSecondary));
                    text = juce::String(line.endBar, 1);
                    break;
                default: break;
            }
        }

        g.drawText(text, 4, 0, width - 8, height,
                   (columnId == 1 && !line.isBreak && Theme::isRtlText(text))
                       ? juce::Justification::centredRight
                       : juce::Justification::centredLeft);
    }

    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        if (columnId == 4 && rowNumber >= 0 && rowNumber < (int)displayIndices.size())
        {
            int modelIdx = displayIndices[static_cast<size_t>(rowNumber)];
            if (modelIdx >= 0 && modelIdx < (int)cachedLyrics.size()
                && cachedLyrics[static_cast<size_t>(modelIdx)].isBreak)
            {
                double breakLen = cachedLyrics[static_cast<size_t>(modelIdx)].endBar
                                - cachedLyrics[static_cast<size_t>(modelIdx)].startBar;
                cachedLyrics.erase(cachedLyrics.begin() + modelIdx);

                for (size_t j = static_cast<size_t>(modelIdx); j < cachedLyrics.size(); ++j)
                {
                    cachedLyrics[j].startBar -= breakLen;
                    cachedLyrics[j].endBar -= breakLen;
                }

                songModel.setLyrics(cachedLyrics);
                rebuildDisplayIndices();
                mappingTable.updateContent();
                mappingTable.repaint();
            }
        }
    }

    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        if (rowNumber < 0 || rowNumber >= (int)displayIndices.size())
            return;
        int modelIdx = displayIndices[static_cast<size_t>(rowNumber)];
        if (modelIdx < 0 || modelIdx >= (int)cachedLyrics.size())
            return;

        bool canEdit = false;
        if (lengthMode)
            canEdit = (columnId == 2);  // Length column
        else
            canEdit = (columnId == 2 || columnId == 3);  // Start/End columns

        if (!canEdit)
            return;

        editingRow = modelIdx;
        editingCol = columnId;

        auto cellBounds = mappingTable.getCellPosition(columnId, rowNumber, true);

        inlineEditor = std::make_unique<juce::TextEditor>();
        inlineEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::kSurfaceLight));
        inlineEditor->setColour(juce::TextEditor::textColourId, juce::Colour(Theme::kTextPrimary));
        inlineEditor->setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::kAccent));
        inlineEditor->setFont(juce::Font(juce::FontOptions(13.0f)));

        const auto& line = cachedLyrics[static_cast<size_t>(modelIdx)];
        if (lengthMode)
        {
            double len = line.endBar - line.startBar;
            inlineEditor->setText(juce::String(len, 1));
        }
        else
        {
            inlineEditor->setText(columnId == 2 ? juce::String(line.startBar, 1)
                                                : juce::String(line.endBar, 1));
        }

        inlineEditor->setBounds(cellBounds);
        inlineEditor->onReturnKey = [this] { commitInlineEdit(); };
        inlineEditor->onFocusLost = [this] { commitInlineEdit(); };
        mappingTable.addAndMakeVisible(*inlineEditor);
        inlineEditor->grabKeyboardFocus();
    }

    // --- Static parsing helpers (public for testing) ---

    // Checks if a line is a section marker like [Verse 1] or [Chorus]
    static bool isSectionMarker(const juce::String& line, juce::String& outName)
    {
        auto trimmed = line.trim();
        if (trimmed.startsWith("[") && trimmed.endsWith("]") && trimmed.length() > 2)
        {
            outName = trimmed.substring(1, trimmed.length() - 1).trim();
            // Exclude break/length directives — those are timeline instructions
            if (outName.startsWithIgnoreCase("break:") || outName.startsWithIgnoreCase("length:"))
                return false;
            return outName.isNotEmpty();
        }
        return false;
    }

    // Checks if a line is a break directive like [break: 3]
    static bool isBreakDirective(const juce::String& line, double& outBars)
    {
        auto trimmed = line.trim();
        if (trimmed.startsWith("[") && trimmed.endsWith("]"))
        {
            auto inner = trimmed.substring(1, trimmed.length() - 1).trim();
            if (inner.startsWithIgnoreCase("break:"))
            {
                outBars = inner.fromFirstOccurrenceOf(":", false, false).trim().getDoubleValue();
                return outBars > 0.0;
            }
        }
        return false;
    }

    // Extracts [length: N] from end of a lyric line, returns the text without the directive
    static juce::String extractLengthDirective(const juce::String& line, double& outLength)
    {
        int bracketStart = line.lastIndexOfChar('[');
        if (bracketStart >= 0 && line.trimEnd().endsWithChar(']'))
        {
            auto directive = line.substring(bracketStart + 1, line.trimEnd().length() - 1).trim();
            if (directive.startsWithIgnoreCase("length:") || directive.startsWithIgnoreCase("length "))
            {
                auto numStr = directive.fromFirstOccurrenceOf(":", false, false).trim();
                if (numStr.isEmpty())
                    numStr = directive.fromFirstOccurrenceOf(" ", false, false).trim();
                double val = numStr.getDoubleValue();
                if (val > 0.0)
                {
                    outLength = val;
                    return line.substring(0, bracketStart).trimEnd();
                }
            }
        }
        outLength = 0.0;
        return line;
    }

private:
    void updateEditorJustification()
    {
        auto text = bulkEditor.getText();
        auto lines = juce::StringArray::fromLines(text);
        bool hasRtl = false;
        for (const auto& line : lines)
        {
            auto trimmed = line.trim();
            if (trimmed.isNotEmpty() && Theme::isRtlText(trimmed))
            {
                hasRtl = true;
                break;
            }
        }
        bulkEditor.setJustification(hasRtl ? juce::Justification::topRight
                                           : juce::Justification::topLeft);
    }

    void parseBulkText()
    {
        auto text = bulkEditor.getText();
        auto lines = juce::StringArray::fromLines(text);

        std::vector<LyricLine> newLyrics;
        std::vector<Section> newSections;
        double bar = 1.0;
        double defaultBarsPerLine = songModel.getDefaultBarsPerLine();
        int currentSectionIdx = -1;

        for (const auto& lineText : lines)
        {
            auto trimmed = lineText.trim();
            if (trimmed.isEmpty())
                continue;

            // Check for section markers like [Verse 1]
            juce::String sectionName;
            if (isSectionMarker(trimmed, sectionName))
            {
                Section sec;
                sec.name = sectionName;
                sec.startBar = static_cast<int>(bar);
                sec.endBar = sec.startBar;
                sec.colour = juce::Colour(
                    Theme::kSectionColours[newSections.size() % Theme::kNumSectionColours]);
                newSections.push_back(sec);
                currentSectionIdx = static_cast<int>(newSections.size()) - 1;
                continue;
            }

            // Check for break directives like [break: 3]
            double breakBars = 0.0;
            if (isBreakDirective(trimmed, breakBars))
            {
                LyricLine bl;
                bl.text = "";
                bl.startBar = bar;
                bl.endBar = bar + breakBars;
                bl.sectionIndex = currentSectionIdx;
                bl.isBreak = true;
                newLyrics.push_back(bl);

                bar += breakBars;
                continue;
            }

            // Check for length directive like "lyric text [length: 4]"
            double lineLength = 0.0;
            auto lyricText = extractLengthDirective(trimmed, lineLength);
            double barsForLine = (lineLength > 0.0) ? lineLength : defaultBarsPerLine;

            LyricLine ll;
            ll.text = lyricText;
            ll.startBar = bar;
            ll.endBar = bar + barsForLine;
            ll.sectionIndex = currentSectionIdx;
            newLyrics.push_back(ll);

            // Extend current section to cover this line
            if (currentSectionIdx >= 0)
                newSections[static_cast<size_t>(currentSectionIdx)].endBar =
                    static_cast<int>(bar + barsForLine);

            bar += barsForLine;
        }

        songModel.setLyrics(newLyrics);
        songModel.setSections(newSections);
        cachedLyrics = newLyrics;
        rebuildDisplayIndices();
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    void autoDistribute()
    {
        // Evenly distribute lyrics across sections
        auto sections = songModel.getSections();
        if (cachedLyrics.empty())
            return;

        if (sections.empty())
        {
            // No sections: distribute evenly, defaultBarsPerLine per line
            double bpl = songModel.getDefaultBarsPerLine();
            double totalBars = cachedLyrics.size() * bpl;
            double barsPerLine = totalBars / cachedLyrics.size();
            double bar = 1.0;
            for (size_t i = 0; i < cachedLyrics.size(); ++i)
            {
                cachedLyrics[i].startBar = bar;
                cachedLyrics[i].endBar = bar + barsPerLine;
                bar += barsPerLine;
            }
        }
        else
        {
            // Distribute evenly across all section bar ranges
            double totalBars = 0.0;
            for (const auto& sec : sections)
                totalBars += (sec.endBar - sec.startBar + 1);

            double barsPerLine = totalBars / cachedLyrics.size();
            double bar = sections.front().startBar;
            for (size_t i = 0; i < cachedLyrics.size(); ++i)
            {
                cachedLyrics[i].startBar = bar;
                cachedLyrics[i].endBar = bar + barsPerLine;
                bar += barsPerLine;
            }
        }

        songModel.setLyrics(cachedLyrics);
        rebuildDisplayIndices();
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    void commitInlineEdit()
    {
        if (!inlineEditor || editingRow < 0 || editingRow >= (int)cachedLyrics.size())
        {
            inlineEditor.reset();
            return;
        }

        double val = inlineEditor->getText().getDoubleValue();
        auto& line = cachedLyrics[static_cast<size_t>(editingRow)];

        if (lengthMode)
        {
            // Column 2 = Length: adjust endBar = startBar + length
            double newLen = juce::jmax(0.5, val);
            double oldLen = line.endBar - line.startBar;
            double delta = newLen - oldLen;
            line.endBar = line.startBar + newLen;

            // Shift all subsequent lines by the delta
            for (size_t j = static_cast<size_t>(editingRow) + 1; j < cachedLyrics.size(); ++j)
            {
                cachedLyrics[j].startBar += delta;
                cachedLyrics[j].endBar += delta;
            }
        }
        else
        {
            if (editingCol == 2)
                line.startBar = juce::jmax(1.0, val);
            else if (editingCol == 3)
                line.endBar = juce::jmax(line.startBar, val);
        }

        songModel.setLyrics(cachedLyrics);
        inlineEditor.reset();
        rebuildDisplayIndices();
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    SongModel& songModel;
    juce::TextEditor bulkEditor;
    juce::Label formatHintLabel;
    juce::Label defaultBarsLabel;
    juce::TextEditor defaultBarsEditor;
    juce::TextButton parseBtn;
    juce::TextButton autoDistBtn;
    juce::TextButton clearChordsBtn;
    juce::TextButton barModeBtn;
    juce::TextButton addBreakBtn;
    juce::TableListBox mappingTable { "Lyrics Mapping" };
    std::vector<LyricLine> cachedLyrics;
    std::vector<int> displayIndices;
    bool lengthMode = false;

    std::unique_ptr<juce::TextEditor> inlineEditor;
    int editingRow = -1;
    int editingCol = -1;

    void toggleBarMode()
    {
        lengthMode = !lengthMode;
        barModeBtn.setButtonText(lengthMode ? "START/END MODE" : "LENGTH MODE");
        barModeBtn.setColour(juce::TextButton::buttonColourId,
                             lengthMode ? juce::Colour(Theme::kAccentDim)
                                        : juce::Colour(Theme::kSurfaceLight));
        barModeBtn.setColour(juce::TextButton::textColourOnId,
                             lengthMode ? juce::Colour(Theme::kBackground)
                                        : juce::Colour(Theme::kTextSecondary));
        barModeBtn.setColour(juce::TextButton::textColourOffId,
                             lengthMode ? juce::Colour(Theme::kBackground)
                                        : juce::Colour(Theme::kTextSecondary));
        addBreakBtn.setVisible(lengthMode);

        // Rebuild table columns
        auto& header = mappingTable.getHeader();
        header.removeAllColumns();
        if (lengthMode)
        {
            header.addColumn("Line",   1, 250);
            header.addColumn("Length", 2, 100);
            header.addColumn("",       4, 36);
        }
        else
        {
            header.addColumn("Line",      1, 250);
            header.addColumn("Start Bar", 2, 90);
            header.addColumn("End Bar",   3, 90);
        }

        rebuildDisplayIndices();
        mappingTable.updateContent();
        mappingTable.repaint();
        resized();
    }

    void addBreak()
    {
        // Determine insertion point: after the last row, or after selected row
        int insertIdx = (int)cachedLyrics.size();
        auto selectedRow = mappingTable.getSelectedRow();
        if (selectedRow >= 0 && selectedRow < (int)cachedLyrics.size())
            insertIdx = selectedRow + 1;

        // Determine the bar position for the new break
        double bar = 1.0;
        if (insertIdx > 0)
            bar = cachedLyrics[static_cast<size_t>(insertIdx - 1)].endBar;

        double breakLen = 2.0;  // default break length

        LyricLine bl;
        bl.text = "";
        bl.startBar = bar;
        bl.endBar = bar + breakLen;
        bl.isBreak = true;

        // Inherit section index from previous line if available
        if (insertIdx > 0)
            bl.sectionIndex = cachedLyrics[static_cast<size_t>(insertIdx - 1)].sectionIndex;

        cachedLyrics.insert(cachedLyrics.begin() + insertIdx, bl);

        // Shift all subsequent lines by the break length
        for (size_t j = static_cast<size_t>(insertIdx) + 1; j < cachedLyrics.size(); ++j)
        {
            cachedLyrics[j].startBar += breakLen;
            cachedLyrics[j].endBar += breakLen;
        }

        songModel.setLyrics(cachedLyrics);
        rebuildDisplayIndices();
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    void rebuildDisplayIndices()
    {
        displayIndices.clear();
        for (int i = 0; i < (int)cachedLyrics.size(); ++i)
        {
            if (!lengthMode && cachedLyrics[static_cast<size_t>(i)].isBreak)
                continue;
            displayIndices.push_back(i);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LyricsEditor)
};
