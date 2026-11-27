#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

//==============================================================================
// Themes + user-customisable look. These are global (shared by every Orbit 3D
// instance in the process) and are remembered between sessions in
// %APPDATA%/Orbit3D/look.xml.
namespace ui
{
struct Theme
{
    const char* name;
    juce::uint32 bg0, bg1, panel, border, text, dim, a1, a2, a3, a4;
    bool light;
};

inline const std::array<Theme, 12>& themes()
{
    static const std::array<Theme, 12> t { {
        { "Nebula",     0xff05070e, 0xff0d1322, 0xff0a0e1a, 0xff1b2542, 0xffd5ddf2, 0xff67759a, 0xff4de8ff, 0xff9b8cff, 0xffff6b9a, 0xffffd166, false },
        { "Aurora",     0xff03100f, 0xff0a1f1d, 0xff071513, 0xff14423d, 0xffd9fff3, 0xff5f9a8f, 0xff5effb0, 0xff4de8ff, 0xffc4ff5e, 0xffffd166, false },
        { "Sunset",     0xff120810, 0xff22101c, 0xff0e060c, 0xff3a1a2e, 0xffffe8dc, 0xff9a6a6a, 0xffff7a59, 0xffff4d8d, 0xffffb347, 0xffffd166, false },
        { "Synthwave",  0xff0a0420, 0xff160a38, 0xff0c0524, 0xff3a1a6e, 0xfff2e6ff, 0xff8a6ac0, 0xff29f0ff, 0xffff2fd6, 0xffb967ff, 0xffffe14d, false },
        { "Matrix",     0xff000a03, 0xff03160a, 0xff000d04, 0xff0d3b1a, 0xffc8ffd8, 0xff3f8f5a, 0xff00ff6a, 0xff7dff9a, 0xff00c853, 0xffd8ff5e, false },
        { "Ember",      0xff0c0705, 0xff1c100a, 0xff120a07, 0xff3d2214, 0xffffe9d6, 0xffa07a5a, 0xffff9500, 0xffff3b30, 0xffffd60a, 0xffffcc66, false },
        { "Ocean",      0xff020b1a, 0xff06203a, 0xff051528, 0xff12406b, 0xffd6ecff, 0xff5f8fb5, 0xff2ee6d6, 0xff4da3ff, 0xff8a7dff, 0xff9dffcf, false },
        { "Graphite",   0xff0b0b0d, 0xff161618, 0xff101012, 0xff2a2a2e, 0xffececf0, 0xff7b7b86, 0xffe8e8ee, 0xffb0b0bd, 0xff8a8a96, 0xffd0d0d8, false },
        { "Blueprint",  0xff0a2540, 0xff0f3460, 0xff0b2a4a, 0xff2d6ea8, 0xffe8f4ff, 0xff7fb0d8, 0xffffffff, 0xff8fd3ff, 0xffffd84d, 0xffff8fa3, false },
        { "Gold",       0xff0a0806, 0xff17120a, 0xff0e0b07, 0xff4a3b1a, 0xfff6ead0, 0xffa28d5c, 0xffe6c15a, 0xffc9a227, 0xffff9d4d, 0xfffff2b8, false },
        { "Ice (light)",    0xffeef2fa, 0xffdfe6f4, 0xfff7f9fe, 0xffc3cde3, 0xff1a2238, 0xff6a7798, 0xff0a84ff, 0xff6e56ff, 0xffff3d7f, 0xffe69500, true },
        { "Sakura (light)", 0xfffbeef2, 0xfff4dde6, 0xfffff7fa, 0xffe5b9c9, 0xff3a1f2b, 0xffa07585, 0xffd6336c, 0xff9c36b5, 0xfff06595, 0xffe8590c, true },
    } };
    return t;
}

// live theme colours (updated by applyTheme)
inline juce::Colour bg0 { 0xff05070e }, bg1 { 0xff0d1322 }, panel { 0xff0a0e1a }, border { 0xff1b2542 },
                    text { 0xffd5ddf2 }, dim { 0xff67759a },
                    cyan { 0xff4de8ff }, violet { 0xff9b8cff }, pink { 0xffff6b9a }, amber { 0xffffd166 };
inline bool light = false;

inline void applyTheme (int i)
{
    const auto& t = themes()[(size_t) juce::jlimit (0, (int) themes().size() - 1, i)];
    bg0 = juce::Colour (t.bg0); bg1 = juce::Colour (t.bg1); panel = juce::Colour (t.panel);
    border = juce::Colour (t.border); text = juce::Colour (t.text); dim = juce::Colour (t.dim);
    cyan = juce::Colour (t.a1); violet = juce::Colour (t.a2); pink = juce::Colour (t.a3); amber = juce::Colour (t.a4);
    light = t.light;
}

inline juce::Colour accent (int i)
{
    switch (i) { case 1: return violet; case 2: return pink; case 3: return amber; default: return cyan; }
}

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
}

//==============================================================================
struct LookSettings
{
    int   theme = 0;
    int   orbStyle = 0;       // 0 Glow, 1 Sphere, 2 Ring, 3 Pulse
    int   colourMode = 0;     // 0 Spectrum, 1 Track colour, 2 Theme accents
    int   background = 0;     // 0 Gradient, 1 Solid, 2 Stars, 3 Nebula
    int   floorStyle = 0;     // 0 Grid, 1 Dots, 2 Polar, 3 None
    int   spectrumStyle = 0;  // 0 Filled, 1 Line, 2 Bars
    int   headStyle = 0;      // 0 Head, 1 Dot, 2 Headphones

    float orbSize = 1.0f, glow = 1.0f, trailLen = 1.0f, labelSize = 1.0f;
    float gridBrightness = 1.0f, perspective = 1.0f, orbitSpeed = 1.0f;

    bool showWalls = true, showRings = true, showTrails = true, showLabels = true;
    bool showSpread = true, showHead = true, showHud = true, showSpectrum = true;
    bool autoOrbit = false;
};

namespace Look
{
inline juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Orbit3D").getChildFile ("look.xml");
}

inline int& themeVersion() { static int v = 0; return v; }
inline bool& dirty() { static bool d = false; return d; }

inline LookSettings& get();

inline void save()
{
    auto& s = get();
    juce::XmlElement x ("LOOK");
    x.setAttribute ("theme", s.theme);       x.setAttribute ("orbStyle", s.orbStyle);
    x.setAttribute ("colourMode", s.colourMode); x.setAttribute ("background", s.background);
    x.setAttribute ("floorStyle", s.floorStyle); x.setAttribute ("spectrumStyle", s.spectrumStyle);
    x.setAttribute ("headStyle", s.headStyle);
    x.setAttribute ("orbSize", (double) s.orbSize);   x.setAttribute ("glow", (double) s.glow);
    x.setAttribute ("trailLen", (double) s.trailLen); x.setAttribute ("labelSize", (double) s.labelSize);
    x.setAttribute ("gridBrightness", (double) s.gridBrightness);
    x.setAttribute ("perspective", (double) s.perspective);
    x.setAttribute ("orbitSpeed", (double) s.orbitSpeed);
    x.setAttribute ("showWalls", s.showWalls);   x.setAttribute ("showRings", s.showRings);
    x.setAttribute ("showTrails", s.showTrails); x.setAttribute ("showLabels", s.showLabels);
    x.setAttribute ("showSpread", s.showSpread); x.setAttribute ("showHead", s.showHead);
    x.setAttribute ("showHud", s.showHud);       x.setAttribute ("showSpectrum", s.showSpectrum);
    x.setAttribute ("autoOrbit", s.autoOrbit);
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    x.writeTo (f);
}

inline void touch() { dirty() = true; }                 // cheap: mark for saving
inline void flush()  { if (dirty()) { dirty() = false; save(); } }
inline void changed() { touch(); flush(); }
inline void themeChanged() { ++themeVersion(); changed(); }

inline LookSettings& get()
{
    static LookSettings s;
    static bool loaded = false;
    if (! loaded)
    {
        loaded = true;
        if (auto x = juce::XmlDocument::parse (settingsFile()))
        {
            s.theme = x->getIntAttribute ("theme", s.theme);
            s.orbStyle = x->getIntAttribute ("orbStyle", s.orbStyle);
            s.colourMode = x->getIntAttribute ("colourMode", s.colourMode);
            s.background = x->getIntAttribute ("background", s.background);
            s.floorStyle = x->getIntAttribute ("floorStyle", s.floorStyle);
            s.spectrumStyle = x->getIntAttribute ("spectrumStyle", s.spectrumStyle);
            s.headStyle = x->getIntAttribute ("headStyle", s.headStyle);
            s.orbSize = (float) x->getDoubleAttribute ("orbSize", s.orbSize);
            s.glow = (float) x->getDoubleAttribute ("glow", s.glow);
            s.trailLen = (float) x->getDoubleAttribute ("trailLen", s.trailLen);
            s.labelSize = (float) x->getDoubleAttribute ("labelSize", s.labelSize);
            s.gridBrightness = (float) x->getDoubleAttribute ("gridBrightness", s.gridBrightness);
            s.perspective = (float) x->getDoubleAttribute ("perspective", s.perspective);
            s.orbitSpeed = (float) x->getDoubleAttribute ("orbitSpeed", s.orbitSpeed);
            s.showWalls = x->getBoolAttribute ("showWalls", s.showWalls);
            s.showRings = x->getBoolAttribute ("showRings", s.showRings);
            s.showTrails = x->getBoolAttribute ("showTrails", s.showTrails);
            s.showLabels = x->getBoolAttribute ("showLabels", s.showLabels);
            s.showSpread = x->getBoolAttribute ("showSpread", s.showSpread);
            s.showHead = x->getBoolAttribute ("showHead", s.showHead);
            s.showHud = x->getBoolAttribute ("showHud", s.showHud);
            s.showSpectrum = x->getBoolAttribute ("showSpectrum", s.showSpectrum);
            s.autoOrbit = x->getBoolAttribute ("autoOrbit", s.autoOrbit);
        }
        ui::applyTheme (s.theme);
    }
    return s;
}
}
