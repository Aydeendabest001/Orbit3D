#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>

// Process-wide registry. Every Orbit 3D instance in the DAW claims a slot and
// publishes its position/levels here; instances in Master mode read all slots.
// (Works because all instances of a VST3 live in the same process / same module.)
struct TrackSlot
{
    std::atomic<bool>     used   { false };
    std::atomic<bool>     isTrack{ false };
    std::atomic<uint32_t> heartbeat { 0 };

    std::atomic<float> x { 0.0f }, y { 0.0f }, z { 2.5f }, width { 0.5f };
    std::atomic<float> levelL { 0.0f }, levelR { 0.0f };
    std::atomic<float> low { 0.0f }, mid { 0.0f }, high { 0.0f };
    std::atomic<float> corr { 1.0f };
    std::atomic<uint32_t> colour { 0xff4de8ff };

    // Move request written by a Master-mode view, applied by the owning Track instance
    std::atomic<bool>  hasPending { false };
    std::atomic<float> pendX { 0.0f }, pendY { 0.0f }, pendZ { 0.0f };

    void setName (const juce::String& s)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        std::memset (name, 0, sizeof (name));
        s.copyToUTF8 (name, sizeof (name) - 1);
    }

    juce::String getName() const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return juce::String::fromUTF8 (name);
    }

private:
    mutable juce::SpinLock lock;
    char name[96] {};
};

class Registry
{
public:
    static constexpr int maxSlots = 64;

    static Registry& get()
    {
        static Registry r;
        return r;
    }

    int claim()
    {
        for (int i = 0; i < maxSlots; ++i)
        {
            bool expected = false;
            if (slots[(size_t) i].used.compare_exchange_strong (expected, true))
            {
                auto& s = slots[(size_t) i];
                s.isTrack = false;
                s.hasPending = false;
                s.levelL = s.levelR = s.low = s.mid = s.high = 0.0f;
                s.corr = 1.0f;
                return i;
            }
        }
        return -1;
    }

    void release (int i)
    {
        if (i >= 0 && i < maxSlots)
        {
            slots[(size_t) i].isTrack = false;
            slots[(size_t) i].used = false;
        }
    }

    std::array<TrackSlot, maxSlots> slots;
};
