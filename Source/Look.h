#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

//==============================================================================
// DESIGNS. A design is not just a palette: it changes how knobs, buttons, panels,
// the 3D room, orbs, labels and text are drawn.  Settings are global (shared by
// every Orbit 3D instance) and remembered in %APPDATA%/Orbit3D/look.xml.
namespace ui
{
enum Design { Classic = 0, Vintage, Blueprint, Studio, Swiss, Radar, NumDesigns };

struct Pal { juce::uint32 bg0, bg1, panel, border, text, dim, a1, a2, a3, a4; };

struct DesignInfo
{
    const char* name;
    const char* blurb;
    Pal scene, panel;
    bool sceneLight, panelLight;
    const char* sans;   // font family ("" = JUCE default)
    const char* mono;
    // defaults applied when the design is chosen
    int orbStyle, floorStyle, headStyle, colourMode, spectrumStyle;
    bool rings, walls;
};

inline const std::array<DesignInfo, NumDesigns>& designs()
{
    static const Pal nebula { 0xff05070e, 0xff0d1322, 0xff0a0e1a, 0xff1b2542, 0xffd5ddf2, 0xff67759a,
                              0xff4de8ff, 0xff9b8cff, 0xffff6b9a, 0xffffd166 };
    static const std::array<DesignInfo, NumDesigns> d { {
        { "Classic Blue", "Glowing orbs, soft glass panels",
          nebula, nebula, false, false, "", "", 0, 0, 0, 0, 0, true, true },

        { "Vintage", "Ivory faceplate, bakelite knobs, pilot lamps, brass bezel",
          { 0xff120d08, 0xff1e150d, 0xff17100a, 0xff4a3a22, 0xfff0e2c0, 0xff9a8760,
            0xffffb347, 0xffe8d9a8, 0xffd9603b, 0xffb8a46a },
          { 0xffd3c9ac, 0xffe6ddc3, 0xffd8ceb1, 0xff7d7050, 0xff231c12, 0xff6d6145,
            0xffb5532a, 0xff3b5b73, 0xff8a3a2a, 0xff8a6a1e },
          false, true, "Georgia", "Courier New", 4, 2, 0, 0, 2, true, true },

        { "Blueprint", "Technical drawing: dimension lines, callouts, title block",
          { 0xff0c3a6e, 0xff0f4a8a, 0xff0b3563, 0xff5b94d1, 0xffe6f1ff, 0xff8fb8e6,
            0xffffffff, 0xffbfe0ff, 0xffffe27a, 0xffff9ab0 },
          { 0xff0c3a6e, 0xff0f4a8a, 0xff0b3563, 0xff5b94d1, 0xffe6f1ff, 0xff8fb8e6,
            0xffffffff, 0xffbfe0ff, 0xffffe27a, 0xffff9ab0 },
          false, false, "Consolas", "Consolas", 5, 0, 0, 2, 1, true, true },

        { "Studio Dark", "Flat, quiet, pro-audio. No glow, hairlines only",
          { 0xff101216, 0xff181b21, 0xff14171c, 0xff2a2f38, 0xffdfe5ee, 0xff7a8494,
            0xff4da3ff, 0xff7aa7ff, 0xffff7a7a, 0xffffc65a },
          { 0xff101216, 0xff1b1f26, 0xff14171c, 0xff2a2f38, 0xffdfe5ee, 0xff7a8494,
            0xff4da3ff, 0xff7aa7ff, 0xffff7a7a, 0xffffc65a },
          false, false, "Segoe UI", "Consolas", 6, 0, 0, 1, 0, false, false },

        { "Swiss Light", "Bold grotesque type, hard edges, black and white",
          { 0xfff2f2ef, 0xffe4e4df, 0xfffafaf8, 0xff1a1a1a, 0xff111111, 0xff6b6b6b,
            0xff0b57ff, 0xff111111, 0xffff3b1f, 0xffffb800 },
          { 0xfff2f2ef, 0xffe4e4df, 0xfffafaf8, 0xff1a1a1a, 0xff111111, 0xff6b6b6b,
            0xff0b57ff, 0xff111111, 0xffff3b1f, 0xffffb800 },
          true, true, "Arial", "Consolas", 6, 0, 0, 2, 2, false, false },

        { "Radar", "Phosphor scope with a rotating sweep and scanlines",
          { 0xff020805, 0xff06140b, 0xff030c06, 0xff0f4a24, 0xff8dffb0, 0xff3f9a5e,
            0xff3dff7a, 0xff9dffb8, 0xffffb02e, 0xff7dffd8 },
          { 0xff020805, 0xff06140b, 0xff030c06, 0xff0f4a24, 0xff8dffb0, 0xff3f9a5e,
            0xff3dff7a, 0xff9dffb8, 0xffffb02e, 0xff7dffd8 },
          false, false, "Consolas", "Consolas", 7, 2, 0, 0, 1, true, false },
    } };
    return d;
}

inline int design = Classic;

// SCENE palette (the 3D view)
inline juce::Colour bg0, bg1, panel, border, text, dim, cyan, violet, pink, amber;
inline bool light = false;

inline juce::Colour accent (int i)
{
    switch (i) { case 1: return violet; case 2: return pink; case 3: return amber; default: return cyan; }
}

inline juce::Font font (float h, bool bold = false)
{
    const char* fam = designs()[(size_t) design].sans;
    const int style = bold ? juce::Font::bold : juce::Font::plain;
    return fam[0] ? juce::Font (juce::FontOptions (juce::String (fam), h, style))
                  : juce::Font (juce::FontOptions (h, style));
}

inline juce::Font mono (float h, bool bold = false)
{
    const char* fam = designs()[(size_t) design].mono;
    const int style = bold ? juce::Font::bold : juce::Font::plain;
    return fam[0] ? juce::Font (juce::FontOptions (juce::String (fam), h, style))
                  : juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, style));
}
}

// PANEL palette (the control panel). Same names as ui::, so widget code reads naturally.
namespace pn
{
inline juce::Colour bg0, bg1, panel, border, text, dim, cyan, violet, pink, amber;
inline bool light = false;
inline juce::Colour accent (int i)
{
    switch (i) { case 1: return violet; case 2: return pink; case 3: return amber; default: return cyan; }
}
}

namespace ui
{
inline void applyDesign (int i)
{
    i = juce::jlimit (0, (int) NumDesigns - 1, i);
    design = i;
    const auto& d = designs()[(size_t) i];
    auto set = [] (const Pal& p, juce::Colour& a0, juce::Colour& a1, juce::Colour& ap, juce::Colour& ab, juce::Colour& at,
                   juce::Colour& ad, juce::Colour& c1, juce::Colour& c2, juce::Colour& c3, juce::Colour& c4)
    {
        a0 = juce::Colour (p.bg0); a1 = juce::Colour (p.bg1); ap = juce::Colour (p.panel); ab = juce::Colour (p.border);
        at = juce::Colour (p.text); ad = juce::Colour (p.dim);
        c1 = juce::Colour (p.a1); c2 = juce::Colour (p.a2); c3 = juce::Colour (p.a3); c4 = juce::Colour (p.a4);
    };
    set (d.scene, bg0, bg1, panel, border, text, dim, cyan, violet, pink, amber);
    set (d.panel, pn::bg0, pn::bg1, pn::panel, pn::border, pn::text, pn::dim, pn::cyan, pn::violet, pn::pink, pn::amber);
    light = d.sceneLight;
    pn::light = d.panelLight;
}
}

//==============================================================================
struct LookSettings
{
    int   design = 1;         // Vintage is the default
    int   orbStyle = 4;       // 0 Glow, 1 Sphere, 2 Ring, 3 Pulse, 4 Lamp, 5 Target, 6 Flat, 7 Blip
    int   colourMode = 0;     // 0 Spectrum, 1 Track colour, 2 Design accents
    int   background = 0;     // Classic only: 0 Gradient, 1 Solid, 2 Stars, 3 Nebula
    int   floorStyle = 2;     // 0 Grid, 1 Dots, 2 Polar, 3 None
    int   spectrumStyle = 2;  // 0 Filled, 1 Line, 2 Bars
    int   headStyle = 0;      // 0 Head, 1 Dot, 2 Headphones

    float orbSize = 1.0f, glow = 1.0f, trailLen = 1.0f, labelSize = 1.0f;
    float gridBrightness = 1.0f, perspective = 1.0f, orbitSpeed = 1.0f;

    bool showWalls = true, showRings = true, showTrails = true, showLabels = true;
    bool showSpread = true, showHead = true, showHud = true, showSpectrum = true;
    bool autoOrbit = false;

    void applyDesignDefaults (int i)
    {
        const auto& d = ui::designs()[(size_t) juce::jlimit (0, (int) ui::NumDesigns - 1, i)];
        design = i;
        orbStyle = d.orbStyle; floorStyle = d.floorStyle; headStyle = d.headStyle;
        colourMode = d.colourMode; spectrumStyle = d.spectrumStyle;
        showRings = d.rings; showWalls = d.walls;
        background = 0;
    }
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
    x.setAttribute ("ver", 3);
    x.setAttribute ("design", s.design);
    x.setAttribute ("orbStyle", s.orbStyle);
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

inline void touch() { dirty() = true; }
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
        if (auto x = juce::XmlDocument::parse (settingsFile()); x != nullptr && x->getIntAttribute ("ver", 0) >= 3)
        {
            s.design = x->getIntAttribute ("design", s.design);
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
        ui::applyDesign (s.design);
    }
    return s;
}
}
