#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::uint32 palette[] = {
    0xff4de8ff, 0xffff6b9a, 0xffffd166, 0xff9b8cff, 0xff6bffb0, 0xffff9e5e,
    0xff5ea8ff, 0xffe66bff, 0xffb6ff5e, 0xff5effe6, 0xffff5e5e, 0xffc4a8ff
};
}

juce::AudioProcessorValueTreeState::ParameterLayout Orbit3DProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                         StringArray { "Track", "Master" }, 0));

    auto pos = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f), def,
            AudioParameterFloatAttributes().withLabel ("m")));
    };
    pos ("x", "X (Left/Right)", -8.0f, 8.0f, 0.0f);
    pos ("y", "Y (Down/Up)",    -5.0f, 5.0f, 0.0f);
    pos ("z", "Z (Back/Front)", -8.0f, 8.0f, 2.5f);

    auto pct = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; })));
    };
    pct ("width",    "Width",    0.35f);
    pct ("distance", "Distance", 0.60f);
    pct ("room",     "Room",     0.25f);
    pct ("air",      "Air",      0.50f);
    pct ("backCue",  "Rear Cue", 0.80f);
    pct ("heightCue","Height Cue", 0.80f);
    pct ("roomSize", "Room Size", 0.55f);
    pct ("damp",     "Damping",  0.55f);
    pct ("mdepth",   "Motion Depth", 0.70f);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "motion", 1 }, "Motion",
                                                         StringArray { "Off", "Circle", "Pendulum", "Figure-8", "Elevator" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mrate", 1 }, "Motion Rate", NormalisableRange<float> (0.02f, 5.0f, 0.001f, 0.4f), 0.25f,
        AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "msync", 1 }, "Motion Sync", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mdiv", 1 }, "Motion Length",
                                                         StringArray { "4 bars", "2 bars", "1 bar", "1/2 bar", "1 beat" }, 2));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mrev", 1 }, "Motion Reverse", false));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "trim", 1 }, "Trim", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "3D Active", true));

    return { p.begin(), p.end() };
}

Orbit3DProcessor::Orbit3DProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ORBIT3D", createLayout())
{
    slotIndex = Registry::get().claim();
    publishIdentity();
    startTimerHz (30);
}

Orbit3DProcessor::~Orbit3DProcessor()
{
    stopTimer();
    Registry::get().release (slotIndex);
}

bool Orbit3DProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Orbit3DProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    sampleRateAtomic = (float) sampleRate;
    srcL.prepare (sampleRate);
    srcR.prepare (sampleRate);
    srcM.prepare (sampleRate);
    pdBuf.assign (32768, 0.0f);
    pdPos = 0; pdCur = 200.0f; hpState = 0.0f;
    wet.setSize (2, juce::jmax (samplesPerBlock, 64));
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    work.setSize (3, juce::jmax (samplesPerBlock, 64));
    gainSmooth.reset (sampleRate, 0.03);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("trim")->load()));
}

void Orbit3DProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // ---------------- analysis of the incoming signal ----------------
    const float aLow = 1.0f - std::exp (-2.0f * orbit::kPi * 200.0f  / (float) sr);
    const float aMid = 1.0f - std::exp (-2.0f * orbit::kPi * 2500.0f / (float) sr);

    float accLL = 0, accRR = 0, accLR = 0, accLow = 0, accMid = 0, accHigh = 0, pkL = 0, pkR = 0;
    int w = ringPos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i];
        const float m = 0.5f * (l + r);
        lpLowL += aLow * (m - lpLowL);
        lpMidL += aMid * (m - lpMidL);
        const float lo = lpLowL, mi = lpMidL - lpLowL, hi = m - lpMidL;
        accLow += lo * lo; accMid += mi * mi; accHigh += hi * hi;
        accLL += l * l; accRR += r * r; accLR += l * r;
        pkL = juce::jmax (pkL, std::abs (l));
        pkR = juce::jmax (pkR, std::abs (r));
        ring[(size_t) (w & (ringSize - 1))] = m;
        ++w;
    }
    ringPos.store (w & (ringSize - 1), std::memory_order_release);

    const float inv = 1.0f / (float) n;
    const float c   = 1.0f - std::exp (-(float) n / ((float) sr * 0.25f));
    const float dec = std::exp (-(float) n / ((float) sr * 0.18f));
    sLL += c * (accLL * inv - sLL);
    sRR += c * (accRR * inv - sRR);
    sLR += c * (accLR * inv - sLR);
    sLow  += c * (accLow  * inv - sLow);
    sMid  += c * (accMid  * inv - sMid);
    sHigh += c * (accHigh * inv - sHigh);
    envL = juce::jmax (std::sqrt (accLL * inv), envL * dec);
    envR = juce::jmax (std::sqrt (accRR * inv), envR * dec);

    const float denom = std::sqrt (sLL * sRR);
    const float corr  = denom > 1.0e-9f ? juce::jlimit (-1.0f, 1.0f, sLR / denom) : 1.0f;

    masterPeakL = juce::jmax (pkL, masterPeakL.load() * dec);
    masterPeakR = juce::jmax (pkR, masterPeakR.load() * dec);
    ownCorr = corr; ownLevelL = envL; ownLevelR = envR;

    const bool master = isMasterMode();
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const Pos base { P ("x"), P ("y"), P ("z") };
    const float width = P ("width");
    const int   motionMode = (int) P ("motion");
    const float motionDepth = P ("mdepth");
    const bool  reverse = P ("mrev") > 0.5f;
    const bool  doMotion = motionMode != 0 && ! master;

    // host timeline (for tempo-synced motion)
    double ppq0 = 0.0, bpm = 120.0;
    bool havePpq = false, playing = true;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition())
        {
            playing = info->getIsPlaying();
            if (auto q = info->getPpqPosition()) { ppq0 = *q; havePpq = true; }
            if (auto b = info->getBpm()) bpm = *b;
        }
    static const double beatsPerCycle[] = { 16.0, 8.0, 4.0, 2.0, 1.0 };
    const double cycleBeats = beatsPerCycle[juce::jlimit (0, 4, (int) P ("mdiv"))];
    const bool sync = P ("msync") > 0.5f && havePpq;
    const double rate = (double) P ("mrate");

    const bool bypassed = master || P ("active") < 0.5f;
    Pos eff = base;

    if (! bypassed)
    {
        const float room   = P ("room");
        const float trimDb = P ("trim");
        orbit::SpatialSettings st;
        st.distance = P ("distance"); st.air = P ("air");
        st.backCue = P ("backCue");   st.heightCue = P ("heightCue");

        if (work.getNumSamples() < n)
            work.setSize (3, n, false, false, true);
        if (wet.getNumSamples() < n)
            wet.setSize (2, n, false, false, true);

        // Mid goes to ONE point (so centred / mono material stays phase-coherent and keeps its top end);
        // only the side signal is spread to the two virtual speakers around it.
        const float k = juce::jmin (1.0f, width * 2.0f);
        {
            float* m = work.getWritePointer (0);
            float* sd = work.getWritePointer (1);
            float* sr2 = work.getWritePointer (2);
            for (int i = 0; i < n; ++i)
            {
                const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]) * k;
                m[i] = mid; sd[i] = side; sr2[i] = -side;
            }
            // diffuse reverb feed = the mono sum (not shaped by head shadow)
            juce::FloatVectorOperations::copy (wet.getWritePointer (0), m, n);
        }
        juce::FloatVectorOperations::clear (L, n);
        juce::FloatVectorOperations::clear (R, n);

        const float spread = width * 2.0f; // metres each side
        constexpr int sub = 32;
        for (int start = 0; start < n; start += sub)
        {
            const int len = juce::jmin (sub, n - start);
            if (doMotion)
            {
                if (sync)
                {
                    if (playing)
                    {
                        const double ppq = ppq0 + ((double) start / sr) * bpm / 60.0;
                        motionPhase = std::fmod (ppq / cycleBeats, 1.0);
                        if (motionPhase < 0.0) motionPhase += 1.0;
                    }
                }
                else
                {
                    motionPhase += rate * (double) len / sr;
                    motionPhase -= std::floor (motionPhase);
                }
                eff = applyMotion (base, motionMode, (float) motionPhase, motionDepth, reverse);
            }
            srcM.process (work.getReadPointer (0) + start, L + start, R + start, len, eff.x, eff.y, eff.z, st);
            if (k > 0.001f)
            {
                srcL.process (work.getReadPointer (1) + start, L + start, R + start, len, eff.x - spread, eff.y, eff.z, st);
                srcR.process (work.getReadPointer (2) + start, L + start, R + start, len, eff.x + spread, eff.y, eff.z, st);
            }
        }

        if (room > 0.001f)
        {
            // Atmosphere: the farther the source, the later and the larger its reverb is relative to the direct sound.
            const float d = std::sqrt (eff.x * eff.x + eff.y * eff.y + eff.z * eff.z);
            const float distMix = juce::jlimit (0.0f, 1.0f, d / 8.0f);
            const float mix = 0.30f + 0.70f * distMix;
            const float preTarget = (float) sr * (0.004f + 0.0022f * juce::jmin (d, 12.0f));   // 4..30 ms
            float* w0 = wet.getWritePointer (0);
            float* w1 = wet.getWritePointer (1);
            const int pm = (int) pdBuf.size() - 1;
            const float hpA = 1.0f - std::exp (-2.0f * 3.14159265f * 160.0f / (float) sr);
            for (int i = 0; i < n; ++i)
            {
                pdBuf[(size_t) (pdPos & pm)] = w0[i];
                pdCur += (preTarget - pdCur) * 0.0008f;
                const float rp0 = (float) pdPos - pdCur;
                const int i0 = (int) std::floor (rp0);
                const float fr = rp0 - (float) i0;
                float x = pdBuf[(size_t) (i0 & pm)] * (1.0f - fr) + pdBuf[(size_t) ((i0 + 1) & pm)] * fr;
                hpState += hpA * (x - hpState);
                x -= hpState;
                w0[i] = w1[i] = x;
                ++pdPos;
            }
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.3f + 0.65f * P ("roomSize");
            rp.damping = P ("damp"); rp.width = 1.0f; rp.freezeMode = 0.0f;
            rp.dryLevel = 0.0f;
            rp.wetLevel = room * mix * 0.30f;
            reverb.setParameters (rp);
            reverb.processStereo (w0, w1, n);
            juce::FloatVectorOperations::add (L, w0, n);
            juce::FloatVectorOperations::add (R, w1, n);
        }

        gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (trimDb));
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSmooth.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }
    }

    // ---------------- publish to the shared registry ----------------
    if (slotIndex >= 0)
    {
        auto& s = Registry::get().slots[(size_t) slotIndex];
        s.isTrack = ! master;
        s.x = eff.x; s.y = eff.y; s.z = eff.z; s.width = width;
        s.levelL = envL; s.levelR = envR;
        s.low = std::sqrt (sLow); s.mid = std::sqrt (sMid); s.high = std::sqrt (sHigh);
        s.corr = corr;
        s.heartbeat.store (++beat, std::memory_order_relaxed);
    }
}

void Orbit3DProcessor::copyLatestMono (float* dest, int num) const
{
    const int end = ringPos.load (std::memory_order_acquire);
    for (int i = 0; i < num; ++i)
        dest[i] = ring[(size_t) ((end - num + i + ringSize * 2) & (ringSize - 1))];
}

void Orbit3DProcessor::updateTrackProperties (const TrackProperties& p)
{
    if (p.name.has_value())
        hostName = *p.name;
    if (p.colourARGB.has_value() && *p.colourARGB != 0)
    {
        hostColour = juce::Colour (*p.colourARGB).withAlpha (1.0f);
        hasHostColour = true;
    }
    publishIdentity();
}

juce::String Orbit3DProcessor::getDisplayName() const
{
    const auto lbl = getLabel();
    if (lbl.isNotEmpty())      return lbl;
    if (hostName.isNotEmpty()) return hostName;
    return "Track " + juce::String (slotIndex + 1);
}

void Orbit3DProcessor::publishIdentity()
{
    if (slotIndex < 0)
        return;
    auto& s = Registry::get().slots[(size_t) slotIndex];
    s.setName (getDisplayName());
    const auto col = hasHostColour ? hostColour
                                   : juce::Colour (palette[(size_t) slotIndex % (sizeof (palette) / sizeof (palette[0]))]);
    s.colour = col.getARGB();
}

void Orbit3DProcessor::setPositionNotifying (float x, float y, float z)
{
    const float v[3] = { x, y, z };
    const char* ids[3] = { "x", "y", "z" };
    for (int i = 0; i < 3; ++i)
        if (auto* p = apvts.getParameter (ids[i]))
            p->setValueNotifyingHost (p->convertTo0to1 (v[i]));
}

void Orbit3DProcessor::timerCallback()
{
    if (slotIndex < 0)
        return;
    auto& s = Registry::get().slots[(size_t) slotIndex];
    if (s.hasPending.exchange (false))
    {
        const char* ids[3] = { "x", "y", "z" };
        for (auto* id : ids)
            if (auto* p = apvts.getParameter (id)) p->beginChangeGesture();
        setPositionNotifying (s.pendX, s.pendY, s.pendZ);
        for (auto* id : ids)
            if (auto* p = apvts.getParameter (id)) p->endChangeGesture();
    }
}

void Orbit3DProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("label", getLabel(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void Orbit3DProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            setLabel (vt.getProperty ("label").toString());
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* Orbit3DProcessor::createEditor()
{
    return new Orbit3DEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Orbit3DProcessor();
}
