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
        // Bulk text editor
        bulkEditor.setMultiLine(true, true);
        bulkEditor.setReturnKeyStartsNewLine(true);
        bulkEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        bulkEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        bulkEditor.setFont(juce::Font(juce::FontOptions(15.0f)));
        bulkEditor.setTextToShowWhenEmpty(
            "[Verse 1]\n"
            "[break: 2]\n"
            "First line [length: 4]\n"
            "Second line [length: 3]\n\n"
            "[Chorus]\n"
            "Chorus line one",
            juce::Colour(0xff555555));
        bulkEditor.onTextChange = [this] { updateEditorJustification(); };
        addAndMakeVisible(bulkEditor);

        // Format hint label shown above the text editor
        formatHintLabel.setText(
            "Sections: [Verse 1]   Break: [break: 2]   Custom length: Line text [length: 4]   Default: 2 bars/line",
            juce::dontSendNotification);
        formatHintLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        formatHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff777777));
        formatHintLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(formatHintLabel);

        // Parse button
        parseBtn.setButtonText("Apply Lyrics");
        parseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00bcd4));
        parseBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        parseBtn.onClick = [this] { parseBulkText(); };
        addAndMakeVisible(parseBtn);

        // Auto-distribute button
        autoDistBtn.setButtonText("Auto-distribute");
        autoDistBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4caf50));
        autoDistBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        autoDistBtn.onClick = [this] { autoDistribute(); };
        addAndMakeVisible(autoDistBtn);

        // Clear MIDI chords button
        clearChordsBtn.setButtonText("Clear MIDI Chords");
        clearChordsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff44336));
        clearChordsBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        clearChordsBtn.onClick = [this] { songModel.clearMidiChords(); };
        addAndMakeVisible(clearChordsBtn);

        // Bar mode toggle button (Start/End vs Length)
        barModeBtn.setButtonText("Length Mode");
        barModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
        barModeBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        barModeBtn.onClick = [this] { toggleBarMode(); };
        addAndMakeVisible(barModeBtn);

        // Add Break button (visible only in Length mode)
        addBreakBtn.setButtonText("+ Add Break");
        addBreakBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff9800));
        addBreakBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        addBreakBtn.onClick = [this] { addBreak(); };
        addBreakBtn.setVisible(false);
        addAndMakeVisible(addBreakBtn);

        // Bar mapping table
        addAndMakeVisible(mappingTable);
        mappingTable.setModel(this);
        mappingTable.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
        mappingTable.setRowHeight(28);

        auto& header = mappingTable.getHeader();
        header.addColumn("Line",      1, 250);
        header.addColumn("Start Bar", 2, 80);
        header.addColumn("End Bar",   3, 80);
        header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff333333));
        header.setColour(juce::TableHeaderComponent::textColourId, juce::Colours::white);

        refreshFromModel();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto topHalf = area.removeFromTop(area.getHeight() / 2);

        // Format hint above the text editor
        formatHintLabel.setBounds(topHalf.removeFromTop(18).reduced(4, 2));
        bulkEditor.setBounds(topHalf.reduced(2));

        auto btnRow = area.removeFromTop(32).reduced(2);
        parseBtn.setBounds(btnRow.removeFromLeft(120));
        btnRow.removeFromLeft(8);
        autoDistBtn.setBounds(btnRow.removeFromLeft(140));
        btnRow.removeFromLeft(8);
        clearChordsBtn.setBounds(btnRow.removeFromLeft(150));
        btnRow.removeFromLeft(8);
        barModeBtn.setBounds(btnRow.removeFromLeft(120));

        auto btnRow2 = area.removeFromTop(32).reduced(2);
        if (lengthMode)
            addBreakBtn.setBounds(btnRow2.removeFromLeft(120));

        area.removeFromTop(4);
        mappingTable.setBounds(area.reduced(2));
    }

    void refreshFromModel()
    {
        cachedLyrics = songModel.getLyrics();
        auto sections = songModel.getSections();

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

        mappingTable.updateContent();
        mappingTable.repaint();
    }

    // TableListBoxModel
    int getNumRows() override { return (int)cachedLyrics.size(); }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int, int, bool rowIsSelected) override
    {
        auto baseCol = rowIsSelected ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2a2a2a);
        if (rowNumber % 2 == 0)
            baseCol = baseCol.brighter(0.03f);
        g.fillAll(baseCol);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool) override
    {
        if (rowNumber < 0 || rowNumber >= (int)cachedLyrics.size())
            return;

        const auto& line = cachedLyrics[static_cast<size_t>(rowNumber)];
        g.setFont(juce::Font(juce::FontOptions(14.0f)));

        juce::String text;
        if (lengthMode)
        {
            switch (columnId)
            {
                case 1:
                    if (line.isBreak)
                    {
                        g.setColour(juce::Colour(0xffff9800));
                        text = "-- BREAK --";
                    }
                    else
                    {
                        g.setColour(juce::Colours::white);
                        text = line.text;
                    }
                    break;
                case 2:
                {
                    g.setColour(juce::Colours::white);
                    double len = line.endBar - line.startBar;
                    text = juce::String(len, 1) + " bars";
                    break;
                }
                default: break;
            }
        }
        else
        {
            g.setColour(line.isBreak ? juce::Colour(0xffff9800) : juce::Colours::white);
            switch (columnId)
            {
                case 1: text = line.isBreak ? "-- BREAK --" : line.text; break;
                case 2: text = juce::String(line.startBar, 1); break;
                case 3: text = juce::String(line.endBar, 1); break;
                default: break;
            }
        }

        g.drawText(text, 4, 0, width - 8, height,
                   (columnId == 1 && !line.isBreak && Theme::isRtlText(text))
                       ? juce::Justification::centredRight
                       : juce::Justification::centredLeft);
    }

    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        if (rowNumber < 0 || rowNumber >= (int)cachedLyrics.size())
            return;

        bool canEdit = false;
        if (lengthMode)
            canEdit = (columnId == 2);  // Length column
        else
            canEdit = (columnId == 2 || columnId == 3);  // Start/End columns

        if (!canEdit)
            return;

        editingRow = rowNumber;
        editingCol = columnId;

        auto cellBounds = mappingTable.getCellPosition(columnId, rowNumber, true);

        inlineEditor = std::make_unique<juce::TextEditor>();
        inlineEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff444444));
        inlineEditor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
        inlineEditor->setFont(juce::Font(juce::FontOptions(14.0f)));

        const auto& line = cachedLyrics[static_cast<size_t>(rowNumber)];
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

    void parseBulkText()
    {
        auto text = bulkEditor.getText();
        auto lines = juce::StringArray::fromLines(text);

        std::vector<LyricLine> newLyrics;
        std::vector<Section> newSections;
        double bar = 1.0;
        double defaultBarsPerLine = 2.0;
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
            // No sections: distribute evenly from bar 1 to bar (2 * numLines)
            double totalBars = cachedLyrics.size() * 2.0;
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
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    SongModel& songModel;
    juce::TextEditor bulkEditor;
    juce::Label formatHintLabel;
    juce::TextButton parseBtn;
    juce::TextButton autoDistBtn;
    juce::TextButton clearChordsBtn;
    juce::TextButton barModeBtn;
    juce::TextButton addBreakBtn;
    juce::TableListBox mappingTable { "Lyrics Mapping" };
    std::vector<LyricLine> cachedLyrics;
    bool lengthMode = false;

    std::unique_ptr<juce::TextEditor> inlineEditor;
    int editingRow = -1;
    int editingCol = -1;

    void toggleBarMode()
    {
        lengthMode = !lengthMode;
        barModeBtn.setButtonText(lengthMode ? "Start/End Mode" : "Length Mode");
        barModeBtn.setColour(juce::TextButton::buttonColourId,
                             lengthMode ? juce::Colour(0xff7c4dff) : juce::Colour(0xff555555));
        addBreakBtn.setVisible(lengthMode);

        // Rebuild table columns
        auto& header = mappingTable.getHeader();
        header.removeAllColumns();
        if (lengthMode)
        {
            header.addColumn("Line",   1, 280);
            header.addColumn("Length", 2, 100);
        }
        else
        {
            header.addColumn("Line",      1, 250);
            header.addColumn("Start Bar", 2, 80);
            header.addColumn("End Bar",   3, 80);
        }

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
        mappingTable.updateContent();
        mappingTable.repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LyricsEditor)
};
