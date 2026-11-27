#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Registry.h"
#include "Spatializer.h"

class Orbit3DProcessor final : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    Orbit3DProcessor();
    ~Orbit3DProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Orbit 3D"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void updateTrackProperties (const TrackProperties&) override;

    // ---- shared with editor -------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    bool isMasterMode() const { return apvts.getRawParameterValue ("mode")->load() > 0.5f; }
    int  getSlotIndex() const { return slotIndex; }

    juce::String getDisplayName() const;
    juce::String getLabel() const { const juce::ScopedLock sl (labelLock); return userLabel; }
    void setLabel (const juce::String& s) { const juce::ScopedLock sl (labelLock); userLabel = s; publishIdentity(); }
    juce::Colour getHostColour() const { return hostColour; }

    // master-mode spectrum source (mono, last 2048 samples)
    static constexpr int ringSize = 8192;
    void copyLatestMono (float* dest, int num) const;
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<float> sampleRateAtomic { 44100.0f };

    // own analysis (for track-mode HUD)
    std::atomic<float> ownCorr { 1.0f }, ownLevelL { 0.0f }, ownLevelR { 0.0f };

    // request to move this instance's position (message thread only)
    void setPositionNotifying (float x, float y, float z);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Pos { float x, y, z; };
    // Motion around the listener. mode: 0 off, 1 circle, 2 pendulum, 3 figure-8, 4 elevator
    static Pos applyMotion (Pos p, int mode, float phase01, float depth, bool reverse)
    {
        if (mode == 0)
            return p;
        const float a = (reverse ? -1.0f : 1.0f) * juce::MathConstants<float>::twoPi * phase01;
        const float halfPi = juce::MathConstants<float>::halfPi;
        float az = std::atan2 (p.x, p.z);
        const float rh = std::sqrt (p.x * p.x + p.z * p.z);
        float y = p.y;
        switch (mode)
        {
            case 1: az += a; break;
            case 2: az += std::sin (a) * depth * halfPi; break;
            case 3: az += std::sin (a) * depth * halfPi; y += std::sin (2.0f * a) * depth * 2.0f; break;
            case 4: y += std::sin (a) * depth * 3.0f; break;
            default: break;
        }
        return { rh * std::sin (az), juce::jlimit (-5.0f, 5.0f, y), rh * std::cos (az) };
    }

private:
    void timerCallback() override;
    void publishIdentity();

    juce::String userLabel;
    juce::CriticalSection labelLock;
    juce::String hostName;
    juce::Colour hostColour { juce::Colours::transparentBlack };
    bool hasHostColour = false;

    int slotIndex = -1;
    double sr = 44100.0;

    orbit::BinauralSource srcL, srcR;
    juce::Reverb reverb;
    juce::AudioBuffer<float> work;
    juce::SmoothedValue<float> gainSmooth;

    // analysis state
    float lpLowL = 0, lpMidL = 0;
    float sLow = 0, sMid = 0, sHigh = 0, sLL = 0, sRR = 0, sLR = 0;
    float envL = 0, envR = 0;
    uint32_t beat = 0;
    double motionPhase = 0.0;

    std::array<float, ringSize> ring {};
    std::atomic<int> ringPos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Orbit3DProcessor)
};
