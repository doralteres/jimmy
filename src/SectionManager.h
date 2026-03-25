#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongModel.h"

// UI component for managing song sections (Verse, Chorus, etc.)
class SectionManager : public juce::Component,
                       public juce::TableListBoxModel
{
public:
    explicit SectionManager(SongModel& model)
        : songModel(model)
    {
        addAndMakeVisible(table);
        table.setModel(this);
        table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
        table.setColour(juce::ListBox::textColourId, juce::Colours::white);
        table.setRowHeight(28);

        auto& header = table.getHeader();
        header.addColumn("Name",      1, 150);
        header.addColumn("Start Bar", 2, 80);
        header.addColumn("End Bar",   3, 80);
        header.addColumn("",          4, 30); // delete button column
        header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff333333));
        header.setColour(juce::TableHeaderComponent::textColourId, juce::Colours::white);

        addBtn.setButtonText("+  Add Section");
        addBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00bcd4));
        addBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        addBtn.onClick = [this] { addNewSection(); };
        addAndMakeVisible(addBtn);

        refreshData();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        addBtn.setBounds(area.removeFromBottom(30).reduced(2));
        table.setBounds(area);
    }

    void refreshData()
    {
        cachedSections = songModel.getSections();
        table.updateContent();
        table.repaint();
    }

    // TableListBoxModel
    int getNumRows() override { return (int)cachedSections.size(); }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int, int, bool rowIsSelected) override
    {
        auto baseCol = rowIsSelected ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2a2a2a);
        if (rowNumber % 2 == 0)
            baseCol = baseCol.brighter(0.03f);
        g.fillAll(baseCol);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool) override
    {
        if (rowNumber < 0 || rowNumber >= (int)cachedSections.size())
            return;

        const auto& sec = cachedSections[static_cast<size_t>(rowNumber)];
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));

        juce::String text;
        switch (columnId)
        {
            case 1: text = sec.name; break;
            case 2: text = juce::String(sec.startBar); break;
            case 3: text = juce::String(sec.endBar); break;
            case 4: text = "X"; g.setColour(juce::Colour(0xfff44336)); break;
            default: break;
        }
        g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft);
    }

    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        if (columnId == 4 && rowNumber >= 0 && rowNumber < (int)cachedSections.size())
        {
            songModel.removeSection(rowNumber);
            refreshData();
        }
    }

    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        if (columnId >= 1 && columnId <= 3 && rowNumber >= 0 && rowNumber < (int)cachedSections.size())
        {
            editingRow = rowNumber;
            editingCol = columnId;

            auto cellBounds = table.getCellPosition(columnId, rowNumber, true);

            inlineEditor = std::make_unique<juce::TextEditor>();
            inlineEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff444444));
            inlineEditor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
            inlineEditor->setFont(juce::Font(juce::FontOptions(14.0f)));

            const auto& sec = cachedSections[static_cast<size_t>(rowNumber)];
            switch (columnId)
            {
                case 1: inlineEditor->setText(sec.name); break;
                case 2: inlineEditor->setText(juce::String(sec.startBar)); break;
                case 3: inlineEditor->setText(juce::String(sec.endBar)); break;
                default: break;
            }

            inlineEditor->setBounds(cellBounds);
            inlineEditor->onReturnKey = [this] { commitInlineEdit(); };
            inlineEditor->onFocusLost = [this] { commitInlineEdit(); };
            table.addAndMakeVisible(*inlineEditor);
            inlineEditor->grabKeyboardFocus();
        }
    }

private:
    void addNewSection()
    {
        int startBar = 1;
        if (!cachedSections.empty())
            startBar = cachedSections.back().endBar + 1;

        Section sec;
        sec.name = "New Section";
        sec.startBar = startBar;
        sec.endBar = startBar + 7;  // default 8 bars
        songModel.addSection(sec);
        refreshData();
    }

    void commitInlineEdit()
    {
        if (!inlineEditor || editingRow < 0 || editingRow >= (int)cachedSections.size())
        {
            inlineEditor.reset();
            return;
        }

        auto text = inlineEditor->getText();
        auto sec = cachedSections[static_cast<size_t>(editingRow)];

        switch (editingCol)
        {
            case 1: sec.name = text; break;
            case 2: sec.startBar = juce::jmax(1, text.getIntValue()); break;
            case 3: sec.endBar = juce::jmax(sec.startBar, text.getIntValue()); break;
            default: break;
        }

        songModel.updateSection(editingRow, sec);
        inlineEditor.reset();
        refreshData();
    }

    SongModel& songModel;
    juce::TableListBox table { "Sections" };
    juce::TextButton addBtn;
    std::vector<Section> cachedSections;

    std::unique_ptr<juce::TextEditor> inlineEditor;
    int editingRow = -1;
    int editingCol = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SectionManager)
};
