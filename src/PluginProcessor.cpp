#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

JimmyProcessor::JimmyProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

JimmyProcessor::~JimmyProcessor() {}

const juce::String JimmyProcessor::getName() const { return JucePlugin_Name; }
bool JimmyProcessor::acceptsMidi() const { return true; }
bool JimmyProcessor::producesMidi() const { return false; }
bool JimmyProcessor::isMidiEffect() const { return false; }
double JimmyProcessor::getTailLengthSeconds() const { return 0.0; }

int JimmyProcessor::getNumPrograms() { return 1; }
int JimmyProcessor::getCurrentProgram() { return 0; }
void JimmyProcessor::setCurrentProgram(int) {}
const juce::String JimmyProcessor::getProgramName(int) { return {}; }
void JimmyProcessor::changeProgramName(int, const juce::String&) {}

void JimmyProcessor::prepareToPlay(double, int) {}
void JimmyProcessor::releaseResources() {}

void JimmyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear all output channels — this plugin produces no audio
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // Read transport state from host
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            transportState.isPlaying.store(pos->getIsPlaying(), std::memory_order_relaxed);
            transportState.isRecording.store(pos->getIsRecording(), std::memory_order_relaxed);

            if (auto bpm = pos->getBpm())
                transportState.bpm.store(*bpm, std::memory_order_relaxed);

            if (auto timeSig = pos->getTimeSignature())
            {
                transportState.timeSigNum.store(timeSig->numerator, std::memory_order_relaxed);
                transportState.timeSigDen.store(timeSig->denominator, std::memory_order_relaxed);
            }

            if (auto ppq = pos->getPpqPosition())
            {
                transportState.ppqPosition.store(*ppq, std::memory_order_relaxed);

                // Compute bar count from PPQ position — more reliable than
                // getBarCount() which some hosts (e.g. Cubase) don't update.
                int tsNum = transportState.timeSigNum.load(std::memory_order_relaxed);
                int tsDen = transportState.timeSigDen.load(std::memory_order_relaxed);
                double ppqPerBar = tsNum * (4.0 / tsDen);
                if (ppqPerBar > 0.0)
                {
                    auto computedBar = static_cast<int64_t>(std::floor(*ppq / ppqPerBar));
                    transportState.barCount.store(computedBar, std::memory_order_relaxed);
                    transportState.barStartPpq.store(computedBar * ppqPerBar, std::memory_order_relaxed);
                }
            }
        }
    }

    // Process MIDI messages
    bool notesChanged = false;
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            midiNoteState.noteOn(msg.getNoteNumber());
            notesChanged = true;
        }
        else if (msg.isNoteOff())
        {
            midiNoteState.noteOff(msg.getNoteNumber());
            notesChanged = true;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            midiNoteState.clear();
            notesChanged = true;
        }
        else if (msg.isProgramChange())
        {
            // Program changes can be used for section markers
            // Store the program number for the UI to pick up
            transportState.lastProgramChange.store(msg.getProgramChangeNumber(), std::memory_order_relaxed);
            transportState.programChangeFlag.store(true, std::memory_order_relaxed);
        }
    }

    // Detect chord from held MIDI notes
    if (notesChanged)
    {
        auto heldNotes = midiNoteState.getHeldNotes();
        if (!heldNotes.empty())
        {
            auto result = ChordParser::identify(heldNotes);
            if (result.name.isNotEmpty() && result.name != lastDetectedChord)
            {
                lastDetectedChord = result.name;
                currentDetectedChord = result.name;
                currentChordHash.fetch_add(1, std::memory_order_relaxed);

                // Push chord event to lock-free FIFO (drained by UI thread)
                double barPos = static_cast<double>(transportState.barCount.load(std::memory_order_relaxed)) + 1.0;
                chordEventFifo.push(barPos, result.name);
            }
        }
    }
}

bool JimmyProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* JimmyProcessor::createEditor()
{
    return new JimmyEditor(*this);
}

void JimmyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml(songModel.toXml());
    copyXmlToBinary(*xml, destData);
}

void JimmyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    songModel.fromXml(xml.get());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JimmyProcessor();
}
