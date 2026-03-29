#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SharedState.h"
#include "SongModel.h"
#include "ChordParser.h"

class JimmyProcessor : public juce::AudioProcessor
{
public:
    JimmyProcessor();
    ~JimmyProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Shared state — written by audio thread, read by UI thread
    TransportState transportState;
    MidiNoteState  midiNoteState;

    // Song data model — accessed from UI thread (with internal mutex)
    SongModel songModel;

    // Lock-free FIFO for chord events (audio thread -> UI thread)
    ChordEventFifo chordEventFifo;

    // Lock-free FIFO for SysEx song data (audio thread -> UI thread)
    SongDataFifo songDataFifo;

    // True when a SysEx-encoded song is active (suppresses live chord detection)
    std::atomic<bool> sysExSongActive { false };

private:
    void handleJimmySysEx(const juce::MidiMessage& msg);

    juce::String lastDetectedChord;

    // SysEx deduplication: avoid reloading same song on repeated SysEx
    uint32_t lastPushedSongHash = 0;
    double   lastPushedStartBar = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JimmyProcessor)
};
