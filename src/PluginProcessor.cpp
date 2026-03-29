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

    // Process MIDI messages — always scan for SysEx, notes only in Live Input mode
    bool notesChanged = false;
    constexpr int kLiveInputMode = static_cast<int>(LiveSourceMode::LiveInput);
    int liveMode = transportState.liveSourceMode.load(std::memory_order_relaxed);

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        // SysEx: always process regardless of live mode
        if (msg.isSysEx())
        {
            handleJimmySysEx(msg);
            continue;
        }

        // Note/CC messages: only in Live Input mode
        if (liveMode != kLiveInputMode)
            continue;

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
            transportState.lastProgramChange.store(msg.getProgramChangeNumber(), std::memory_order_relaxed);
            transportState.programChangeFlag.store(true, std::memory_order_relaxed);
        }
    }

    // Detect chord from held MIDI notes (only in Live Input mode, not when SysEx song is active)
    if (notesChanged && liveMode == kLiveInputMode
        && !sysExSongActive.load(std::memory_order_relaxed))
    {
        auto heldNotes = midiNoteState.getHeldNotes();
        if (!heldNotes.empty())
        {
            auto result = ChordParser::identify(heldNotes);
            if (result.name.isNotEmpty() && result.name != lastDetectedChord)
            {
                lastDetectedChord = result.name;

                // Push chord event to lock-free FIFO (drained by UI thread)
                double barPos = static_cast<double>(transportState.barCount.load(std::memory_order_relaxed)) + 1.0;
                chordEventFifo.push(barPos, result.name);
            }
        }
        else
        {
            // All notes released — reset so the same chord can be re-detected
            lastDetectedChord = "";
        }
    }
}

bool JimmyProcessor::hasEditor() const { return true; }

// ── SysEx handling (called from processBlock, audio thread) ──

void JimmyProcessor::handleJimmySysEx(const juce::MidiMessage& msg)
{
    auto* data = msg.getSysExData();
    int   size = msg.getSysExDataSize();

    if (!JimmySysEx::isJimmySysEx(data, size))
        return;

    auto msgType = data[JimmySysEx::kMsgTypeOffset];

    if (msgType == JimmySysEx::kSongEnd)
    {
        sysExSongActive.store(false, std::memory_order_relaxed);
        lastPushedSongHash = 0;
        lastPushedStartBar = -1.0;
        songDataFifo.push(nullptr, 0, 0.0, true);
        return;
    }

    if (msgType == JimmySysEx::kSongHeader && size > JimmySysEx::kPayloadOffset)
    {
        int relBar = JimmySysEx::decodeRelBar(data[JimmySysEx::kRelBarHiOffset],
                                              data[JimmySysEx::kRelBarLoOffset]);
        double currentBar = static_cast<double>(transportState.barCount.load(std::memory_order_relaxed)) + 1.0;
        double absoluteStartBar = currentBar - static_cast<double>(relBar);

        // Deduplicate: skip if same payload hash at the same start bar
        const auto* payload = data + JimmySysEx::kPayloadOffset;
        int payloadSize = size - JimmySysEx::kPayloadOffset;

        // Simple hash: FNV-1a over the first 64 bytes of payload (enough for dedup)
        uint32_t hash = 2166136261u;
        int hashLen = std::min(payloadSize, 64);
        for (int i = 0; i < hashLen; ++i)
        {
            hash ^= payload[i];
            hash *= 16777619u;
        }

        if (hash == lastPushedSongHash && std::abs(absoluteStartBar - lastPushedStartBar) < 0.5)
            return; // same song, same position — skip

        lastPushedSongHash = hash;
        lastPushedStartBar = absoluteStartBar;
        sysExSongActive.store(true, std::memory_order_relaxed);

        // Store absoluteStartBar in the FIFO so the UI can compute relative position
        songDataFifo.push(payload, payloadSize, absoluteStartBar, false);
    }
}

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

    // Sync live source mode to audio thread
    transportState.liveSourceMode.store(
        songModel.getLiveSourceMode() == LiveSourceMode::FromEditor ? 1 : 0,
        std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JimmyProcessor();
}
