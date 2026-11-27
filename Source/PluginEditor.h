#pragma once
#include "PluginProcessor.h"
#include "Look.h"
#include <deque>
#include <array>
#include <functional>

struct V3
{
    float x = 0, y = 0, z = 0;
    V3 operator+ (V3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    V3 operator- (V3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    V3 operator* (float s) const { return { x * s, y * s, z * s }; }
    float dot (V3 o) const { return x * o.x + y * o.y + z * o.z; }
    V3 cross (V3 o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
    float length() const { return std::sqrt (dot (*this)); }
    V3 normalised() const { const float l = length(); return l > 1.0e-6f ? *this * (1.0f / l) : V3{}; }
};

namespace room
{
constexpr float X = 8.0f, Y = 5.0f, Z = 8.0f;
}

//==============================================================================
class OrbitLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OrbitLookAndFeel();
    void refresh();   // re-read the global theme colours

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return ui::font (12.0f, true); }
    juce::Font getLabelFont (juce::Label&) override { return ui::font (12.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (12.5f); }
    juce::Font getPopupMenuFont() override { return ui::font (13.0f); }
};

//==============================================================================
struct SlotView
{
    bool active = false, stale = false;
    V3 pos, base, dispPos;      // pos = effective (with motion), base = the knob position
    float width = 0.3f, corr = 1.0f;
    float lvl = 0.0f, low = 0, mid = 0, high = 0;
    juce::Colour trackColour { 0xff4de8ff };
    juce::String name;
    uint32_t lastBeat = 0;
    int staleTicks = 0, trailTick = 0;
    std::deque<V3> trail;
    // screen cache
    float sx = 0, sy = 0, sr = 0, sc = 0, depth = 0;
    bool visible = false;
};

class SceneView final : public juce::Component, private juce::Timer
{
public:
    explicit SceneView (Orbit3DProcessor&);
    ~SceneView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; hoverPreset = -1; }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void resetCamera();
    void setCameraPreset (int index);   // 0 = 3/4, 1 = top, 2 = side, 3 = behind
    const std::array<SlotView, Registry::maxSlots>& getViews() const { return views; }

    int selectedSlot = -1;

private:
    struct Camera
    {
        V3 pos, f, r, u;
        float focal = 600, cx = 0, cy = 0;
        bool project (V3 p, juce::Point<float>& out, float& scale, float& depth) const;
    };
    struct Star { float x, y, r, ph; };

    void timerCallback() override;
    void updateData();
    void updateSpectrum();
    Camera makeCamera() const;
    bool projLine (const Camera&, V3 a, V3 b, juce::Point<float>& pa, juce::Point<float>& pb) const;
    void line3 (juce::Graphics&, const Camera&, V3 a, V3 b, juce::Colour c, float thick);
    int  pickOrb (juce::Point<float>) const;
    int  pickPreset (juce::Point<float>) const;
    bool canDrag (int slot) const;
    bool rayPlaneY (juce::Point<float>, float y0, V3& hit) const;
    bool rayVertical (juce::Point<float>, V3 through, V3& hit) const;
    void computeGrab (juce::Point<float>, bool shift);
    void applyMove (int slot, V3 desiredVisiblePos);
    juce::Colour orbColour (int slot) const;

    void drawBackground (juce::Graphics&);
    void drawRoom (juce::Graphics&, const Camera&);
    void drawListener (juce::Graphics&, const Camera&);
    void drawOrb (juce::Graphics&, const Camera&, int slot);
    void drawSpectrumOverlay (juce::Graphics&, juce::Rectangle<float>);
    void drawHud (juce::Graphics&);
    void drawPresetButtons (juce::Graphics&);

    Orbit3DProcessor& proc;
    std::array<SlotView, Registry::maxSlots> views;
    std::vector<Star> stars;

    float yaw = 0.45f, pitch = 0.6f, camDist = 21.0f;
    float tYaw = 0.45f, tPitch = 0.6f;
    bool camAnimating = false;
    int hovered = -1, hoverPreset = -1, dragSlot = -1, tick = 0;
    enum class Drag { none, orbit, move } dragMode = Drag::none;
    juce::Point<float> lastMouse, downPos;
    V3 grabOffset;
    bool lastShift = false, movedFar = false;
    std::array<juce::Rectangle<float>, 4> presetRects;

    static constexpr int kFFT = 4096;
    juce::dsp::FFT fft { 12 };
    juce::dsp::WindowingFunction<float> window { (size_t) kFFT, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * kFFT> fftData {};
    std::array<float, kFFT / 2> specDb {};
    float meterL = -90, meterR = -90;

    bool isMaster() const { return proc.isMasterMode(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SceneView)
};

//==============================================================================
class Knob final : public juce::Component
{
public:
    // attached to a plugin parameter
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
          int accentIndex, bool bipolar);
    // free-standing
    Knob (const juce::String& title, int accentIndex, bool bipolar,
          double lo, double hi, double def, double step, std::function<juce::String (double)> textFn);
    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    juce::Slider slider;
private:
    void setup (int accentIndex, bool bipolar);
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::String titleText;
};

class MovePage final : public juce::Component, private juce::Timer
{
public:
    explicit MovePage (Orbit3DProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void setSpherical (bool sph);
    void applySpherical();
    void setPositionWithGesture (float x, float y, float z);

    Orbit3DProcessor& proc;
    juce::TextEditor nameEditor;
    juce::TextButton xyzBtn { "X  Y  Z" }, sphBtn { "AZ  EL  DIST" };
    Knob kx, ky, kz, kAz, kEl, kDist, kRate, kDepth;
    juce::OwnedArray<juce::TextButton> presets;
    juce::ComboBox motionBox, divBox;
    juce::ToggleButton syncToggle { "Tempo sync" }, revToggle { "Reverse" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> motionAttach, divAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach, revAttach;
    bool spherical = false, updatingSph = false;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MovePage)
};

class SoundPage final : public juce::Component, private juce::Timer
{
public:
    explicit SoundPage (Orbit3DProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { repaint (readoutRect); }
    Orbit3DProcessor& proc;
    Knob kWidth, kDist, kAir, kRoom, kSize, kDamp, kBack, kHeight, kTrim;
    juce::ToggleButton activeToggle { "3D processing" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttach;
    juce::Rectangle<int> readoutRect;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundPage)
};

class MasterPage final : public juce::Component, private juce::Timer
{
public:
    MasterPage (Orbit3DProcessor&, SceneView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override;
    Orbit3DProcessor& proc;
    SceneView& scene;
    juce::ToggleButton orbitToggle { "Auto-orbit camera" }, trailToggle { "Motion trails" }, labelToggle { "Track names" };
    juce::Rectangle<int> listRect, detailRect, headerRect;
    int listScroll = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterPage)
};

class LookPage final : public juce::Component
{
public:
    explicit LookPage (std::function<void()> onThemeChanged);
    ~LookPage() override;
    void resized() override;
    int getContentHeight() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookPage)
};

class ControlPanel final : public juce::Component
{
public:
    ControlPanel (Orbit3DProcessor&, SceneView&, std::function<void()> onThemeChanged);
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int index);   // index within the current mode's tabs
    void refreshMode();

private:
    void applyMode (bool master);
    void updateTabs();

    Orbit3DProcessor& proc;
    SceneView& scene;
    bool master = false;
    int page = 0;

    juce::TextButton trackBtn { "TRACK" }, masterBtn { "MASTER" };
    juce::ParameterAttachment modeAttach;
    std::array<juce::TextButton, 3> tabBtns;

    MovePage move;
    SoundPage sound;
    MasterPage masterPage;
    LookPage look;
    juce::Viewport lookViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPanel)
};

class Orbit3DEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Orbit3DEditor (Orbit3DProcessor&);
    ~Orbit3DEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int index) { side.showPage (index); }

private:
    void timerCallback() override;
    void themeChanged();

    Orbit3DProcessor& proc;
    OrbitLookAndFeel laf;
    SceneView scene;
    ControlPanel side;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Orbit3DEditor)
};
