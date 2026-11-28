#include "PluginEditor.h"

using namespace juce;

namespace
{
const String kDeg = String::fromUTF8 ("\xc2\xb0");
const String kDot = String::fromUTF8 ("  \xc2\xb7  ");

float textWidth (const Font& f, const String& t)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

float toDb (float lin) { return 20.0f * std::log10 (jmax (lin, 1.0e-6f)); }

void positionGestures (Orbit3DProcessor& p, bool begin)
{
    for (auto* id : { "x", "y", "z" })
        if (auto* prm = p.apvts.getParameter (id))
        {
            if (begin) prm->beginChangeGesture(); else prm->endChangeGesture();
        }
}

String azText (float x, float z)
{
    const float az = radiansToDegrees (std::atan2 (x, z));
    const int a = roundToInt (std::abs (az));
    if (a < 2)   return "0" + kDeg + " front";
    if (a > 178) return "180" + kDeg + " behind";
    return String (a) + kDeg + (az > 0 ? " right" : " left");
}

Colour spectrumColourFor (const SlotView& v)
{
    const float wl = v.low * 0.7f, wm = v.mid * 1.8f, wh = v.high * 5.0f;
    const float sum = wl + wm + wh;
    if (sum < 1.0e-4f)
        return v.trackColour.withMultipliedBrightness (0.6f);
    const auto lo = ui::pink, mi = ui::amber, hi = ui::cyan;
    const float a = wl / sum, b = wm / sum, c = wh / sum;
    return Colour::fromFloatRGBA (a * lo.getFloatRed()   + b * mi.getFloatRed()   + c * hi.getFloatRed(),
                                  a * lo.getFloatGreen() + b * mi.getFloatGreen() + c * hi.getFloatGreen(),
                                  a * lo.getFloatBlue()  + b * mi.getFloatBlue()  + c * hi.getFloatBlue(), 1.0f);
}

Colour lineBase()
{
    switch (ui::design) { case ui::Studio: return ui::dim; case ui::Swiss: return ui::text; default: return ui::cyan; }
}

float lineMul()
{
    switch (ui::design) { case ui::Vintage: return 1.5f; case ui::Blueprint: return 2.8f; case ui::Studio: return 1.6f;
                          case ui::Swiss: return 1.5f; case ui::Radar: return 1.7f; default: return 1.0f; }
}

Colour lineCol (float alpha)
{
    const float a = alpha * Look::get().gridBrightness * lineMul();
    return lineBase().withAlpha (jlimit (0.0f, 1.0f, a));
}

Colour ringCol()
{
    return ui::design == ui::Classic ? ui::violet : lineBase();
}

// Small panels (preset buttons, HUD) are drawn as a different kind of object per design.
struct Plate { Colour text; };

Plate drawPlate (Graphics& g, Rectangle<float> r, bool hover, bool pillShape)
{
    switch (ui::design)
    {
    case ui::Vintage:
        g.setGradientFill (ColourGradient (Colour (hover ? 0xffe8d690 : 0xffd9c67f), r.getX(), r.getY(),
                                           Colour (hover ? 0xffb09c58 : 0xff9c8845), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colour (0xff5b4a1c)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.2f);
        g.setColour (Colour (0x55ffffff)); g.drawLine (r.getX() + 3, r.getY() + 1.5f, r.getRight() - 3, r.getY() + 1.5f, 1.0f);
        return { Colour (0xff2a1f0a) };
    case ui::Blueprint:
        g.setColour (ui::bg0.withAlpha (0.9f)); g.fillRect (r);
        g.setColour (hover ? ui::cyan : ui::text); g.drawRect (r.reduced (0.5f), 1.0f);
        return { ui::text };
    case ui::Studio:
        g.setColour (ui::panel.withAlpha (hover ? 1.0f : 0.88f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (hover ? ui::cyan : ui::border); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        return { ui::text };
    case ui::Swiss:
        g.setColour (hover ? ui::text : ui::panel); g.fillRect (r);
        g.setColour (ui::text); g.drawRect (r.reduced (0.8f), 1.6f);
        return { hover ? ui::panel : ui::text };
    case ui::Radar:
    {
        g.setColour (ui::bg0.withAlpha (0.85f)); g.fillRect (r);
        g.setColour (hover ? ui::text : ui::cyan);
        const float len = 5.0f;
        for (int sx = 0; sx < 2; ++sx)
            for (int sy = 0; sy < 2; ++sy)
            {
                const float x = sx ? r.getRight() : r.getX(), y = sy ? r.getBottom() : r.getY();
                g.drawLine (x, y, x + (sx ? -len : len), y, 1.3f);
                g.drawLine (x, y, x, y + (sy ? -len : len), 1.3f);
            }
        return { hover ? ui::text : ui::cyan };
    }
    default:
    {
        const float cr = pillShape ? r.getHeight() * 0.5f : 12.0f;
        g.setColour (ui::panel.withAlpha (hover ? 0.95f : 0.8f)); g.fillRoundedRectangle (r, cr);
        g.setColour (hover ? ui::cyan : ui::border); g.drawRoundedRectangle (r.reduced (0.5f), cr, 1.0f);
        return { hover ? ui::text : ui::text };
    }
    }
}
}

//==============================================================================
bool SceneView::Camera::project (V3 p, Point<float>& out, float& scale, float& depth) const
{
    const V3 v = p - pos;
    const float zc = v.dot (f);
    if (zc < 0.2f)
        return false;
    scale = focal / zc;
    out = { cx + v.dot (r) * scale, cy - v.dot (u) * scale };
    depth = zc;
    return true;
}

SceneView::SceneView (Orbit3DProcessor& p) : proc (p)
{
    setOpaque (true);
    specDb.fill (-120.0f);
    Random rnd (42);
    for (int i = 0; i < 170; ++i)
        stars.push_back ({ rnd.nextFloat(), rnd.nextFloat(), 0.5f + rnd.nextFloat() * 1.3f, rnd.nextFloat() * 6.28f });
    startTimerHz (50);
}

void SceneView::resetCamera() { setCameraPreset (0); camDist = 21.0f; }

void SceneView::setCameraPreset (int i)
{
    static const float yp[4][2] = { { 0.45f, 0.6f }, { 0.0f, 1.5f }, { 1.5708f, 0.06f }, { 0.0f, 0.2f } };
    i = jlimit (0, 3, i);
    tYaw = yp[i][0];
    tPitch = yp[i][1];
    camAnimating = true;
}

SceneView::Camera SceneView::makeCamera() const
{
    const auto& L = Look::get();
    const auto b = getLocalBounds().toFloat();
    const bool overlay = isMaster() && L.showSpectrum;
    const float usableH = b.getHeight() - (overlay ? 140.0f : 0.0f);
    const float persp = jlimit (0.5f, 2.0f, L.perspective);

    Camera c;
    const V3 target { 0.0f, -0.6f, 0.3f };
    const float dist = camDist / persp;
    c.pos = target + V3 { dist * std::cos (pitch) * std::sin (yaw),
                          dist * std::sin (pitch),
                          -dist * std::cos (pitch) * std::cos (yaw) };
    c.f = (target - c.pos).normalised();
    c.r = V3 { 0, 1, 0 }.cross (c.f).normalised();
    c.u = c.f.cross (c.r);
    c.focal = jmin (b.getWidth() * 1.05f, usableH * 1.08f) / persp;
    c.cx = b.getCentreX();
    c.cy = b.getY() + usableH * 0.5f;
    return c;
}

bool SceneView::projLine (const Camera& c, V3 a, V3 b, Point<float>& pa, Point<float>& pb) const
{
    constexpr float nearZ = 0.35f;
    const float za = (a - c.pos).dot (c.f), zb = (b - c.pos).dot (c.f);
    if (za < nearZ && zb < nearZ)
        return false;
    if (za < nearZ || zb < nearZ)
    {
        const float t = (nearZ - za) / (zb - za);
        const V3 m = a + (b - a) * t;
        if (za < nearZ) a = m; else b = m;
    }
    float s, d;
    return c.project (a, pa, s, d) && c.project (b, pb, s, d);
}

void SceneView::line3 (Graphics& g, const Camera& c, V3 a, V3 b, Colour col, float thick)
{
    Point<float> pa, pb;
    if (projLine (c, a, b, pa, pb))
    {
        g.setColour (col);
        g.drawLine (pa.x, pa.y, pb.x, pb.y, thick);
    }
}

Colour SceneView::orbColour (int slot) const
{
    const auto& v = views[(size_t) slot];
    switch (Look::get().colourMode)
    {
        case 1:  return v.trackColour;
        case 2:  return ui::accent (slot % 4);
        default: return spectrumColourFor (v);
    }
}

//==============================================================================
void SceneView::timerCallback()
{
    ++tick;
    updateData();
    if (isMaster() && Look::get().showSpectrum)
        updateSpectrum();

    if (camAnimating)
    {
        const float dy = std::remainder (tYaw - yaw, MathConstants<float>::twoPi);
        const float dp = tPitch - pitch;
        yaw += dy * 0.18f;
        pitch += dp * 0.18f;
        if (std::abs (dy) < 0.003f && std::abs (dp) < 0.003f)
            camAnimating = false;
    }
    else if (Look::get().autoOrbit && dragMode != Drag::orbit)
        yaw += 0.0035f * Look::get().orbitSpeed;

    repaint();
}

void SceneView::updateData()
{
    auto& reg = Registry::get();
    const bool master = isMaster();
    const int self = proc.getSlotIndex();
    const int motionMode = (int) proc.apvts.getRawParameterValue ("motion")->load();
    const size_t maxTrail = (size_t) jmax (0.0f, 60.0f * Look::get().trailLen);

    for (int i = 0; i < Registry::maxSlots; ++i)
    {
        auto& s = reg.slots[(size_t) i];
        auto& v = views[(size_t) i];
        const bool isSelf = (! master && i == self);
        const bool act = s.used.load() && s.isTrack.load() && (! master || i != self);

        if (! act && ! (isSelf && s.used.load()))
        {
            v.active = false;
            v.trail.clear();
            if (selectedSlot == i) selectedSlot = -1;
            continue;
        }

        V3 target { s.x.load(), s.y.load(), s.z.load() };
        if (isSelf)
        {
            v.base = { proc.apvts.getRawParameterValue ("x")->load(),
                       proc.apvts.getRawParameterValue ("y")->load(),
                       proc.apvts.getRawParameterValue ("z")->load() };
            if (motionMode == 0)
                target = v.base;
        }

        if (! v.active)
        {
            v.active = true;
            v.pos = v.dispPos = target;
            if (! isSelf) v.base = target;
            v.lvl = 0.0f;
            v.name = isSelf ? proc.getDisplayName() : s.getName();
            v.lastBeat = s.heartbeat.load();
            v.staleTicks = 0;
            v.trail.clear();
        }

        const bool beingDragged = (dragMode == Drag::move && dragSlot == i && master);
        if (! beingDragged)
            v.pos = target;
        if (! isSelf) v.base = v.pos;
        v.dispPos = v.dispPos + (v.pos - v.dispPos) * 0.45f;

        v.width = s.width.load();
        v.corr  = s.corr.load();
        if ((tick + i) % 10 == 0)
            v.name = isSelf ? proc.getDisplayName() : s.getName();
        v.trackColour = Colour ((uint32) s.colour.load());

        const uint32_t beat = s.heartbeat.load();
        if (beat != v.lastBeat) { v.lastBeat = beat; v.staleTicks = 0; }
        else                     ++v.staleTicks;
        v.stale = v.staleTicks > 40;

        const float lin = 0.5f * (s.levelL.load() + s.levelR.load());
        const float tgt = v.stale ? 0.0f : jlimit (0.0f, 1.0f, (toDb (lin) + 66.0f) / 60.0f);
        v.lvl += (tgt - v.lvl) * (tgt > v.lvl ? 0.55f : 0.12f);
        v.low  += (s.low.load()  - v.low)  * 0.2f;
        v.mid  += (s.mid.load()  - v.mid)  * 0.2f;
        v.high += (s.high.load() - v.high) * 0.2f;

        if (v.trail.empty() || (v.dispPos - v.trail.back()).length() > 0.05f)
        {
            v.trail.push_back (v.dispPos);
            v.trailTick = 0;
        }
        else if (++v.trailTick % 3 == 0 && ! v.trail.empty())
            v.trail.pop_front();
        while (v.trail.size() > maxTrail)
            v.trail.pop_front();
    }
}

void SceneView::updateSpectrum()
{
    proc.copyLatestMono (fftData.data(), kFFT);
    window.multiplyWithWindowingTable (fftData.data(), kFFT);
    std::fill (fftData.begin() + kFFT, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    for (size_t k = 0; k < specDb.size(); ++k)
    {
        const float db = toDb (fftData[k] / ((float) kFFT * 0.25f));
        specDb[k] = db > specDb[k] ? db : specDb[k] * 0.82f + db * 0.18f;
    }

    const float l = toDb (proc.masterPeakL.load()), r = toDb (proc.masterPeakR.load());
    meterL = jmax (l, meterL - 1.5f);
    meterR = jmax (r, meterR - 1.5f);
}

//==============================================================================
void SceneView::drawBackground (Graphics& g)
{
    const auto& L = Look::get();
    const auto b = getLocalBounds().toFloat();

    switch (ui::design)
    {
    case ui::Vintage:
    {
        g.setGradientFill (ColourGradient (ui::bg1.brighter (0.15f), b.getCentreX(), b.getCentreY() * 0.9f,
                                           ui::bg0.darker (0.5f), b.getRight(), b.getBottom(), true));
        g.fillRect (b);
        return;
    }
    case ui::Blueprint:
    {
        g.fillAll (ui::bg1);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), b.getCentreX(), b.getCentreY(),
                                           Colours::transparentBlack, b.getRight(), b.getBottom(), true));
        g.fillRect (b);
        for (float x = 0; x < b.getWidth(); x += 20.0f)
        {
            g.setColour (ui::border.withAlpha (std::fmod (x, 100.0f) < 1.0f ? 0.34f : 0.14f));
            g.drawVerticalLine ((int) x, 0.0f, b.getHeight());
        }
        for (float y = 0; y < b.getHeight(); y += 20.0f)
        {
            g.setColour (ui::border.withAlpha (std::fmod (y, 100.0f) < 1.0f ? 0.34f : 0.14f));
            g.drawHorizontalLine ((int) y, 0.0f, b.getWidth());
        }
        return;
    }
    case ui::Studio:
    case ui::Swiss:
        g.fillAll (ui::bg0);
        return;
    case ui::Radar:
    {
        g.fillAll (ui::bg0);
        g.setGradientFill (ColourGradient (ui::bg1.brighter (0.3f), b.getCentreX(), b.getCentreY(),
                                           ui::bg0, b.getCentreX() + b.getWidth() * 0.6f, b.getCentreY(), true));
        g.fillRect (b);
        return;
    }
    default: break;
    }

    if (L.background == 1)
    {
        g.fillAll (ui::bg0);
        return;
    }

    g.setGradientFill (ColourGradient::vertical (ui::light ? ui::bg1 : ui::bg1.darker (0.3f), 0.0f, ui::bg0, b.getHeight()));
    g.fillRect (b);

    if (L.background == 2)
    {
        for (auto& s : stars)
        {
            const float tw = 0.5f + 0.5f * std::sin (s.ph + (float) tick * 0.03f);
            g.setColour ((ui::light ? ui::dim : ui::text).withAlpha (0.10f + 0.35f * tw * (ui::light ? 0.5f : 1.0f)));
            g.fillEllipse (s.x * b.getWidth(), s.y * b.getHeight(), s.r, s.r);
        }
    }
    else if (L.background == 3)
    {
        const float t = (float) tick * 0.004f;
        const Colour cs[3] { ui::violet, ui::cyan, ui::pink };
        for (int i = 0; i < 3; ++i)
        {
            const float cx = b.getWidth() * (0.25f + 0.5f * (float) i * 0.5f + 0.08f * std::sin (t + (float) i * 2.0f));
            const float cy = b.getHeight() * (0.3f + 0.2f * (float) i + 0.06f * std::cos (t * 1.3f + (float) i));
            const float rad = b.getWidth() * 0.45f;
            g.setGradientFill (ColourGradient (cs[i].withAlpha (ui::light ? 0.16f : 0.13f), cx, cy,
                                               Colours::transparentBlack, cx + rad, cy, true));
            g.fillRect (b);
        }
    }
    else
    {
        ColourGradient rg (ui::violet.withAlpha (ui::light ? 0.10f : 0.11f), b.getCentreX(), b.getCentreY() * 0.9f,
                           Colours::transparentBlack, b.getCentreX() + b.getWidth() * 0.65f, b.getCentreY() * 0.9f, true);
        g.setGradientFill (rg);
        g.fillRect (b);
        ColourGradient rg2 (ui::cyan.withAlpha (0.05f), b.getRight(), b.getBottom(),
                            Colours::transparentBlack, b.getRight() - b.getWidth() * 0.6f, b.getBottom(), true);
        g.setGradientFill (rg2);
        g.fillRect (b);
    }
}

void SceneView::drawRoom (Graphics& g, const Camera& cam)
{
    const auto& L = Look::get();
    const float fy = -room::Y;
    const float X = room::X, Z = room::Z;

    if (L.floorStyle != 3)
    {
        Point<float> p[4]; float s, d; bool ok = true;
        const V3 corners[4] { { -X, fy, -Z }, { X, fy, -Z }, { X, fy, Z }, { -X, fy, Z } };
        for (int i = 0; i < 4; ++i) ok = ok && cam.project (corners[i], p[i], s, d);
        if (ok)
        {
            Path fp;
            fp.startNewSubPath (p[0]); fp.lineTo (p[1]); fp.lineTo (p[2]); fp.lineTo (p[3]); fp.closeSubPath();
            if (ui::design == ui::Classic || ui::design == ui::Vintage || ui::design == ui::Radar)
            {
                g.setColour ((ui::light ? ui::panel : ui::bg1.interpolatedWith (ui::bg0, 0.35f)).withAlpha (0.75f));
                g.fillPath (fp);
            }
            else if (ui::design == ui::Blueprint)
            {
                g.setColour (Colours::white.withAlpha (0.045f));
                g.fillPath (fp);
            }
        }
    }

    // radar sweep
    if (ui::design == ui::Radar)
    {
        Point<float> pt; float s, d;
        if (cam.project ({ 0, fy, 0 }, pt, s, d))
        {
            const float sw = sweepAngle();
            for (int k = 0; k < 22; ++k)
            {
                Path wedge;
                wedge.startNewSubPath (pt);
                bool ok = true;
                const float a0 = sw - (float) k * 0.045f, a1 = sw - (float) (k + 1) * 0.045f;
                for (int j = 0; j <= 4; ++j)
                {
                    const float a = a0 + (a1 - a0) * (float) j / 4.0f;
                    Point<float> q;
                    ok = ok && cam.project ({ std::sin (a) * Z, fy, std::cos (a) * Z }, q, s, d);
                    if (ok) wedge.lineTo (q);
                }
                if (ok)
                {
                    wedge.closeSubPath();
                    g.setColour (ui::cyan.withAlpha (0.20f * (1.0f - (float) k / 22.0f) * L.gridBrightness));
                    g.fillPath (wedge);
                }
            }
            Point<float> q;
            if (cam.project ({ std::sin (sw) * Z, fy, std::cos (sw) * Z }, q, s, d))
            {
                g.setColour (ui::cyan.withAlpha (0.8f));
                g.drawLine (pt.x, pt.y, q.x, q.y, 1.6f);
            }
        }
    }

    // facing wedge
    if (ui::design == ui::Classic)
    {
        Path wedge; Point<float> pt; float s, d;
        if (cam.project ({ 0, fy, 0 }, pt, s, d))
        {
            wedge.startNewSubPath (pt);
            bool ok = true;
            for (int i = -10; i <= 10; ++i)
            {
                const float a = degreesToRadians ((float) i * 3.5f);
                Point<float> q;
                ok = ok && cam.project ({ std::sin (a) * Z, fy, std::cos (a) * Z }, q, s, d);
                if (ok) wedge.lineTo (q);
            }
            if (ok)
            {
                wedge.closeSubPath();
                g.setColour (ui::violet.withAlpha (0.06f * L.gridBrightness));
                g.fillPath (wedge);
            }
        }
    }

    // floor pattern
    if (L.floorStyle == 0)
    {
        for (int i = -8; i <= 8; i += 2)
        {
            const float a = (i == 0) ? 0.24f : 0.09f;
            line3 (g, cam, { (float) i, fy, -Z }, { (float) i, fy, Z }, lineCol (a), 1.0f);
            line3 (g, cam, { -X, fy, (float) i }, { X, fy, (float) i }, lineCol (a), 1.0f);
        }
    }
    else if (L.floorStyle == 1)
    {
        for (int i = -8; i <= 8; i += 2)
            for (int j = -8; j <= 8; j += 2)
            {
                Point<float> q; float s, d;
                if (cam.project ({ (float) i, fy, (float) j }, q, s, d))
                {
                    const float r = jmax (1.1f, s * 0.045f);
                    g.setColour (lineCol ((i == 0 || j == 0) ? 0.55f : 0.30f));
                    g.fillEllipse (q.x - r, q.y - r, r * 2.0f, r * 2.0f);
                }
            }
    }
    else if (L.floorStyle == 2)
    {
        for (int k = 0; k < 12; ++k)
        {
            const float a = (float) k / 12.0f * MathConstants<float>::twoPi;
            line3 (g, cam, { 0, fy, 0 }, { std::sin (a) * Z, fy, std::cos (a) * Z }, lineCol (k % 3 == 0 ? 0.22f : 0.09f), 1.0f);
        }
    }

    // distance rings
    if (L.showRings || L.floorStyle == 2)
        for (int r = 2; r <= 8; r += 2)
        {
            V3 prev { 0, fy, (float) r };
            for (int i = 1; i <= 72; ++i)
            {
                const float a = (float) i / 72.0f * MathConstants<float>::twoPi;
                const V3 cur { std::sin (a) * (float) r, fy, std::cos (a) * (float) r };
                if (ui::design != ui::Blueprint || ((i / 2) % 2) == 0)
                    line3 (g, cam, prev, cur, ringCol().withAlpha (jlimit (0.0f, 1.0f, (ui::design == ui::Classic ? 0.15f : 0.18f) * L.gridBrightness * lineMul())), 1.0f);
                prev = cur;
            }
        }

    // walls
    if (L.showWalls)
    {
        const V3 cs[4] { { -X, 0, -Z }, { X, 0, -Z }, { X, 0, Z }, { -X, 0, Z } };
        for (int i = 0; i < 4; ++i)
        {
            const V3 lo { cs[i].x, fy, cs[i].z }, hi { cs[i].x, room::Y, cs[i].z };
            line3 (g, cam, lo, hi, lineCol (0.11f), 1.0f);
            const V3 hi2 { cs[(i + 1) % 4].x, room::Y, cs[(i + 1) % 4].z };
            line3 (g, cam, hi, hi2, lineCol (0.11f), 1.0f);
        }
        line3 (g, cam, { 0, fy, 0 }, { 0, room::Y, 0 }, lineCol (0.07f), 1.0f);
    }

    auto label = [&] (V3 p, const String& t, float size, float alpha)
    {
        Point<float> q; float sc, d;
        if (cam.project (p, q, sc, d))
        {
            g.setColour ((ui::design == ui::Swiss ? ui::text : ui::dim).withAlpha (alpha));
            g.setFont (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (size, true)
                                                                              : ui::font (size, ui::design == ui::Swiss));
            g.drawText (t, Rectangle<float> (q.x - 50, q.y - 8, 100, 16).toNearestInt(), Justification::centred);
        }
    };
    const bool deg = ui::design == ui::Radar;
    const bool bp = ui::design == ui::Blueprint;
    const bool vin = ui::design == ui::Vintage;
    const float ls = ui::design == ui::Swiss ? 13.0f : 11.0f;
    label ({ 0, fy, Z + 1.1f }, deg ? "000" + kDeg : (bp ? "FRONT +Z" : (vin ? "Front" : "FRONT")), ls, 0.9f);
    label ({ 0, fy, -Z - 1.1f }, deg ? "180" + kDeg : (bp ? "BACK -Z" : (vin ? "Back" : "BACK")), ls, 0.9f);
    label ({ -X - 1.1f, fy, 0 }, deg ? "270" + kDeg : (bp ? "LEFT -X" : (vin ? "Left" : "LEFT")), ls, 0.9f);
    label ({ X + 1.1f, fy, 0 }, deg ? "090" + kDeg : (bp ? "RIGHT +X" : (vin ? "Right" : "RIGHT")), ls, 0.9f);
    if (L.showRings)
        for (int r = 2; r <= 8; r += 2)
            label ({ (float) r + 0.45f, fy, 0.35f }, String (r) + (deg ? "M" : " m"), 9.5f, 0.6f);
    if (L.showWalls)
        label ({ -X, room::Y + 0.5f, -Z }, vin ? "Up" : "UP", 9.5f, 0.45f);

    // radar bearing ticks around the scope
    if (ui::design == ui::Radar)
        for (int k = 0; k < 72; ++k)
        {
            const float a = (float) k / 72.0f * MathConstants<float>::twoPi;
            const float len = (k % 6 == 0) ? 0.55f : 0.28f;
            line3 (g, cam, { std::sin (a) * (Z + 0.1f), fy, std::cos (a) * (Z + 0.1f) },
                   { std::sin (a) * (Z + 0.1f + len), fy, std::cos (a) * (Z + 0.1f + len) }, lineCol ((k % 6 == 0) ? 0.55f : 0.3f), 1.0f);
        }

    // blueprint dimension lines
    if (bp)
    {
        const float off = Z + 2.6f;
        auto dim = [&] (V3 a, V3 b, V3 tickDir, const String& t)
        {
            line3 (g, cam, a, b, ui::text.withAlpha (0.85f), 1.0f);
            for (V3 e : { a, b })
                line3 (g, cam, e - tickDir * 0.35f, e + tickDir * 0.35f, ui::text.withAlpha (0.85f), 1.2f);
            Point<float> q; float sc, d;
            if (cam.project ((a + b) * 0.5f, q, sc, d))
            {
                const float tw = textWidth (ui::mono (10.0f, true), t) + 10.0f;
                g.setColour (ui::bg1);
                g.fillRect (q.x - tw * 0.5f, q.y - 7.0f, tw, 14.0f);
                g.setColour (ui::text);
                g.setFont (ui::mono (10.0f, true));
                g.drawText (t, Rectangle<float> (q.x - tw * 0.5f, q.y - 7.0f, tw, 14.0f).toNearestInt(), Justification::centred);
            }
        };
        dim ({ -X, fy, -off }, { X, fy, -off }, { 0, 0, 1 }, String (X * 2.0f, 2) + " m");
        dim ({ X + 2.6f, fy, -Z }, { X + 2.6f, fy, Z }, { 1, 0, 0 }, String (Z * 2.0f, 2) + " m");
        dim ({ -X - 2.0f, fy, -Z }, { -X - 2.0f, room::Y, -Z }, { 1, 0, 0 }, String (room::Y * 2.0f, 2) + " m");
    }
}

void SceneView::drawListener (Graphics& g, const Camera& cam)
{
    const auto& L = Look::get();
    Point<float> c; float s, d;
    if (! cam.project ({ 0, 0, 0 }, c, s, d))
        return;

    const float r = s * (L.headStyle == 1 ? 0.28f : 0.55f);
    Colour headLight = ui::light ? Colours::white : Colour (0xfff2f6ff);
    Colour headDark  = ui::light ? ui::cyan.withMultipliedBrightness (0.8f) : Colour (0xff6b7cae);
    if (ui::design == ui::Vintage) { headLight = Colour (0xfff0e2c0); headDark = Colour (0xff8a6a3a); }

    // Flat / technical designs draw the listener as a symbol rather than a lit 3D head.
    if (ui::design >= ui::Blueprint)
    {
        const auto ink = ui::design == ui::Radar ? ui::cyan : ui::text;
        auto nose = [&] (Colour col, bool fill)
        {
            Point<float> n0, n1, n2; float a, b, e2;
            if (cam.project ({ 0, -0.05f, 0.78f }, n0, a, b) && cam.project ({ -0.14f, -0.05f, 0.42f }, n1, a, b)
                && cam.project ({ 0.14f, -0.05f, 0.42f }, n2, a, e2))
            {
                Path p; p.startNewSubPath (n0); p.lineTo (n1); p.lineTo (n2); p.closeSubPath();
                g.setColour (col);
                if (fill) g.fillPath (p); else g.strokePath (p, PathStrokeType (1.4f));
            }
        };
        if (ui::design == ui::Blueprint)
        {
            g.setColour (ui::bg1); g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            g.setColour (ink); g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.4f);
            g.drawLine (c.x - r * 1.5f, c.y, c.x + r * 1.5f, c.y, 1.0f);
            g.drawLine (c.x, c.y - r * 1.5f, c.x, c.y + r * 1.5f, 1.0f);
            nose (ink, false);
        }
        else if (ui::design == ui::Studio)
        {
            g.setColour (ui::text.withAlpha (0.9f)); g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            nose (ui::cyan, true);
        }
        else if (ui::design == ui::Swiss)
        {
            g.setColour (ui::text); g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            nose (ui::pink, true);
        }
        else
        {
            g.setColour (ink.withAlpha (0.5f));
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);
            g.drawEllipse (c.x - r * 0.5f, c.y - r * 0.5f, r, r, 1.0f);
            g.setColour (ink); g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
            nose (ink, true);
        }
        g.setColour (ui::design == ui::Swiss ? ui::text : ui::dim);
        g.setFont (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (9.5f, true) : ui::font (9.5f, true));
        g.drawText (ui::design == ui::Radar ? "YOU / 0,0" : "YOU", Rectangle<float> (c.x - 40, c.y + r + 3.0f, 80, 12).toNearestInt(), Justification::centred);
        return;
    }

    ColourGradient halo ((ui::light ? ui::cyan : (ui::design == ui::Vintage ? ui::cyan : Colours::white)).withAlpha (0.16f), c.x, c.y,
                         Colours::transparentBlack, c.x + r * 3.2f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);

    if (L.headStyle == 1)
    {
        g.setColour (ui::cyan);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (ui::text.withAlpha (0.7f));
        g.drawEllipse (c.x - r * 1.8f, c.y - r * 1.8f, r * 3.6f, r * 3.6f, 1.0f);
    }
    else
    {
        for (float side : { -1.0f, 1.0f })
        {
            Point<float> e; float es, ed;
            if (cam.project ({ side * 0.52f, 0, 0 }, e, es, ed))
            {
                g.setColour (L.headStyle == 2 ? ui::cyan : ui::text.withAlpha (0.8f));
                const float w = L.headStyle == 2 ? 0.2f : 0.11f, h = L.headStyle == 2 ? 0.42f : 0.16f;
                g.fillRoundedRectangle (e.x - es * w * 0.5f * 2.0f, e.y - es * h, es * w * 2.0f, es * h * 2.0f, es * 0.08f);
            }
        }

        g.setGradientFill (ColourGradient (headLight, c.x - r * 0.35f, c.y - r * 0.45f,
                                           headDark, c.x + r * 0.7f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (ui::text.withAlpha (0.45f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);

        if (L.headStyle == 2)
        {
            Path band; bool started = false;
            for (int i = 0; i <= 24; ++i)
            {
                const float a = MathConstants<float>::pi * (float) i / 24.0f;
                Point<float> q; float qs, qd;
                if (cam.project ({ std::cos (a) * 0.6f, std::sin (a) * 0.72f, 0.0f }, q, qs, qd))
                {
                    if (! started) { band.startNewSubPath (q); started = true; } else band.lineTo (q);
                }
            }
            g.setColour (ui::cyan);
            g.strokePath (band, PathStrokeType (3.0f));
        }

        Point<float> n0, n1, n2; float a, b, e2;
        if (cam.project ({ 0, -0.05f, 0.78f }, n0, a, b)
            && cam.project ({ -0.14f, -0.05f, 0.42f }, n1, a, b)
            && cam.project ({ 0.14f, -0.05f, 0.42f }, n2, a, e2))
        {
            Path nose;
            nose.startNewSubPath (n0); nose.lineTo (n1); nose.lineTo (n2); nose.closeSubPath();
            g.setColour (ui::cyan);
            g.fillPath (nose);
        }
    }

    g.setColour (ui::dim);
    g.setFont (ui::font (9.5f, true));
    g.drawText ("YOU", Rectangle<float> (c.x - 30, c.y + r + 3.0f, 60, 12).toNearestInt(), Justification::centred);
}

void SceneView::drawOrb (Graphics& g, const Camera& cam, int slot)
{
    const auto& L = Look::get();
    const auto& v = views[(size_t) slot];
    const bool master = isMaster();
    const bool self = ! master && slot == proc.getSlotIndex();
    const bool ghost = ! master && ! self;
    const bool sel = master && slot == selectedSlot;
    const bool hov = slot == hovered;

    float alpha = ghost ? 0.5f : 1.0f;
    if (v.stale) alpha *= 0.45f;

    const auto col = orbColour (slot);
    const float r = v.sr;
    const float cx = v.sx, cy = v.sy;
    const float glowK = L.glow;

    // stereo spread: halo + two satellites for the virtual L/R sources
    const float spreadW = v.width * 2.0f;
    if (L.showSpread && spreadW > 0.03f)
    {
        Point<float> pl, pr; float s1, d1;
        const bool okL = cam.project ({ v.dispPos.x - spreadW, v.dispPos.y, v.dispPos.z }, pl, s1, d1);
        const bool okR = cam.project ({ v.dispPos.x + spreadW, v.dispPos.y, v.dispPos.z }, pr, s1, d1);
        if (okL && okR)
        {
            g.setColour (v.trackColour.withAlpha (0.35f * alpha));
            g.drawLine (pl.x, pl.y, pr.x, pr.y, 1.2f);
            const float sat = jmax (2.5f, r * 0.34f);
            g.setColour (v.trackColour.withAlpha (0.85f * alpha));
            g.fillEllipse (pl.x - sat, pl.y - sat, sat * 2.0f, sat * 2.0f);
            g.fillEllipse (pr.x - sat, pr.y - sat, sat * 2.0f, sat * 2.0f);
            const float widthMul = 1.0f + (1.0f - v.corr) * 0.5f;
            const float hw = (pr.x - pl.x) * 0.5f * widthMul + r * 1.4f;
            g.setColour (col.withAlpha ((0.05f + 0.10f * v.lvl) * alpha * glowK));
            g.fillEllipse (cx - hw, cy - r * 1.7f, hw * 2.0f, r * 3.4f);
        }
    }

    auto glow = [&] (float radiusMul, float strength)
    {
        const float gr = r * radiusMul;
        ColourGradient gg (col.withAlpha (jlimit (0.0f, 1.0f, strength * glowK) * alpha), cx, cy,
                           Colours::transparentBlack, cx + gr, cy, true);
        g.setGradientFill (gg);
        g.fillEllipse (cx - gr, cy - gr, gr * 2.0f, gr * 2.0f);
    };

    switch (L.orbStyle)
    {
        case 1: // Sphere
        {
            glow (1.8f, 0.18f + 0.2f * v.lvl);
            g.setGradientFill (ColourGradient (col.brighter (0.9f).withAlpha (alpha), cx - r * 0.38f, cy - r * 0.45f,
                                               col.darker (0.75f).withAlpha (alpha), cx + r * 0.85f, cy + r * 0.9f, true));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour (Colours::white.withAlpha (0.55f * alpha));
            g.fillEllipse (cx - r * 0.55f, cy - r * 0.62f, r * 0.45f, r * 0.3f);
            break;
        }
        case 2: // Ring
        {
            glow (2.2f, 0.12f + 0.25f * v.lvl);
            g.setColour (col.withAlpha (0.16f * alpha));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour (col.withAlpha (alpha));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 2.6f);
            const float cr = r * (0.22f + 0.2f * v.lvl);
            g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
            break;
        }
        case 3: // Pulse
        {
            for (int k = 0; k < 3; ++k)
            {
                const float ph = std::fmod ((float) tick * (0.012f + 0.02f * v.lvl) + (float) k / 3.0f, 1.0f);
                const float rr = r * (0.9f + ph * (1.2f + 2.2f * v.lvl));
                g.setColour (col.withAlpha ((1.0f - ph) * (0.15f + 0.5f * v.lvl) * alpha * jmin (1.5f, glowK)));
                g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 1.6f);
            }
            glow (1.9f, 0.25f + 0.35f * v.lvl);
            const float cr = r * 0.72f;
            g.setGradientFill (ColourGradient (col.brighter (0.7f).withAlpha (alpha), cx - cr * 0.3f, cy - cr * 0.4f,
                                               col.darker (0.4f).withAlpha (alpha), cx + cr * 0.8f, cy + cr * 0.8f, true));
            g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
            break;
        }
        case 4: // Lamp: brass-collared pilot bulb
        {
            glow (1.9f + 0.8f * v.lvl, 0.12f + 0.45f * v.lvl);
            g.setColour (Colour (0xffb8a46a).withAlpha (alpha));
            g.fillEllipse (cx - r * 1.12f, cy - r * 1.12f, r * 2.24f, r * 2.24f);
            g.setColour (Colour (0xff1c120b).withAlpha (alpha));
            g.fillEllipse (cx - r * 0.95f, cy - r * 0.95f, r * 1.9f, r * 1.9f);
            const float k = 0.25f + 0.75f * jlimit (0.0f, 1.0f, v.lvl * 1.4f);
            g.setGradientFill (ColourGradient (col.brighter (1.0f).withAlpha (alpha), cx - r * 0.2f, cy - r * 0.25f,
                                               col.darker (0.5f + (1.0f - k)).withAlpha (alpha), cx + r * 0.8f, cy + r * 0.8f, true));
            g.fillEllipse (cx - r * 0.78f, cy - r * 0.78f, r * 1.56f, r * 1.56f);
            g.setColour (Colours::white.withAlpha (0.5f * alpha));
            g.fillEllipse (cx - r * 0.5f, cy - r * 0.55f, r * 0.4f, r * 0.26f);
            break;
        }
        case 5: // Target: technical crosshair marker
        {
            g.setColour (col.withAlpha (alpha));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f);
            const float ir = r * (0.18f + 0.5f * v.lvl);
            g.drawEllipse (cx - ir, cy - ir, ir * 2.0f, ir * 2.0f, 1.0f);
            g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
            g.drawLine (cx - r * 1.7f, cy, cx - r * 0.55f, cy, 1.2f);
            g.drawLine (cx + r * 0.55f, cy, cx + r * 1.7f, cy, 1.2f);
            g.drawLine (cx, cy - r * 1.7f, cx, cy - r * 0.55f, 1.2f);
            g.drawLine (cx, cy + r * 0.55f, cx, cy + r * 1.7f, 1.2f);
            break;
        }
        case 6: // Flat
        {
            g.setColour (col.withAlpha (0.28f * alpha));
            const float rr = r * (1.2f + 0.7f * v.lvl);
            g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 1.2f);
            g.setColour (col.withAlpha (alpha));
            g.fillEllipse (cx - r * 0.9f, cy - r * 0.9f, r * 1.8f, r * 1.8f);
            if (ui::design == ui::Swiss)
            {
                g.setColour (ui::text.withAlpha (alpha));
                g.drawEllipse (cx - r * 0.9f, cy - r * 0.9f, r * 1.8f, r * 1.8f, 2.0f);
            }
            break;
        }
        case 7: // Blip: lights up when the radar sweep passes
        {
            const float az = std::atan2 (v.dispPos.x, v.dispPos.z);
            float diff = std::fmod (sweepAngle() - az, MathConstants<float>::twoPi);
            if (diff < 0) diff += MathConstants<float>::twoPi;
            const float hit = std::exp (-diff * 1.6f);
            const float b = 0.35f + 0.65f * hit;
            glow (1.6f + 1.4f * hit, (0.10f + 0.45f * v.lvl) * b);
            g.setColour (col.withAlpha (b * alpha));
            Path dia;
            const float br = r * 0.75f;
            dia.addQuadrilateral (cx, cy - br, cx + br, cy, cx, cy + br, cx - br, cy);
            g.fillPath (dia);
            g.setColour (col.withAlpha (0.5f * alpha));
            g.drawEllipse (cx - r * 1.3f, cy - r * 1.3f, r * 2.6f, r * 2.6f, 1.0f);
            break;
        }
        default: // Glow
        {
            glow (2.6f + 1.6f * v.lvl, 0.15f + 0.5f * v.lvl);
            g.setGradientFill (ColourGradient (col.brighter (0.8f).withAlpha (alpha), cx - r * 0.35f, cy - r * 0.4f,
                                               col.darker (0.55f).withAlpha (alpha), cx + r * 0.8f, cy + r * 0.85f, true));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            break;
        }
    }

    if (L.orbStyle <= 3 && L.orbStyle != 2)
    {
        g.setColour (v.trackColour.withAlpha (alpha));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, self ? 2.2f : 1.6f);
    }

    if (sel || hov || self)
    {
        g.setColour ((ui::light ? ui::text : Colours::white).withAlpha (sel ? 0.95f : (self ? 0.55f : 0.7f)));
        g.drawEllipse (cx - r - 4.0f, cy - r - 4.0f, r * 2.0f + 8.0f, r * 2.0f + 8.0f, sel ? 2.0f : 1.2f);
    }

    const bool wantLabel = L.showLabels ? (! ghost || hov) : (sel || hov || self);
    if (wantLabel)
        drawLabel (g, v, cx, cy, r, self, sel, alpha);
}

void SceneView::drawLabel (Graphics& g, const SlotView& v, float cx, float cy, float r, bool self, bool sel, float alpha)
{
    const auto& L = Look::get();
    const auto t = v.name.isEmpty() ? String ("Track") : v.name;
    const float fs = 11.5f * L.labelSize;
    switch (ui::design)
    {
    case ui::Vintage:   // paper tag on a string
    {
        const auto f = ui::font (fs, self || sel);
        const float w = textWidth (f, t) + 16.0f, h = 17.0f * L.labelSize;
        Rectangle<float> tag (cx - w * 0.5f, cy + r + 8.0f, w, h);
        g.setColour (Colour (0xff6b5a3a).withAlpha (0.7f * alpha));
        g.drawLine (cx, cy + r, cx, tag.getY(), 1.0f);
        g.setColour (Colour (0xfff0e2c0).withAlpha (0.95f * alpha));
        g.fillRoundedRectangle (tag, 2.0f);
        g.setColour (Colour (0xffb8a46a).withAlpha (alpha));
        g.drawRoundedRectangle (tag.reduced (0.5f), 2.0f, 1.0f);
        g.setColour (Colour (0xff2a1f0a).withAlpha (alpha));
        g.setFont (f);
        g.drawText (t, tag.toNearestInt(), Justification::centred, true);
        return;
    }
    case ui::Blueprint: // callout with leader line
    {
        const auto f = ui::mono (fs - 1.0f, self || sel);
        const float w = textWidth (f, t) + 10.0f;
        const Point<float> a (cx + r * 0.75f, cy - r * 0.75f), b (cx + r + 16.0f, cy - r - 14.0f);
        g.setColour (ui::text.withAlpha (0.9f * alpha));
        g.drawLine (a.x, a.y, b.x, b.y, 1.0f);
        g.drawLine (b.x, b.y, b.x + w, b.y, 1.0f);
        g.setFont (f);
        g.drawText (t, Rectangle<float> (b.x + 3.0f, b.y - 15.0f, w, 14.0f).toNearestInt(), Justification::centredLeft, true);
        return;
    }
    case ui::Studio:
        g.setFont (ui::font (fs - 0.5f, self || sel));
        g.setColour (ui::text.withAlpha (0.9f * alpha));
        g.drawText (t, Rectangle<float> (cx + r + 6.0f, cy - 9.0f, 160.0f, 18.0f).toNearestInt(), Justification::centredLeft, true);
        return;
    case ui::Swiss:
    {
        const auto f = ui::font (fs, true);
        const float w = textWidth (f, t) + 14.0f, h = 17.0f * L.labelSize;
        Rectangle<float> box (cx - w * 0.5f, cy + r + 6.0f, w, h);
        g.setColour (ui::text.withAlpha (alpha));
        g.fillRect (box);
        g.setColour (ui::panel.withAlpha (alpha));
        g.setFont (f);
        g.drawText (t, box.toNearestInt(), Justification::centred, true);
        return;
    }
    case ui::Radar:
    {
        g.setFont (ui::mono (fs - 1.0f, self || sel));
        g.setColour (ui::text.withAlpha (alpha));
        g.drawText ("[" + t.toUpperCase() + "]", Rectangle<float> (cx + r + 6.0f, cy - 9.0f, 200.0f, 18.0f).toNearestInt(), Justification::centredLeft, true);
        return;
    }
    default: break;
    }
    const Rectangle<int> tb ((int) cx - 90, (int) (cy + r + 5.0f), 180, (int) (16.0f * L.labelSize) + 2);
    g.setFont (ui::font (fs, self || sel));
    g.setColour ((ui::light ? Colours::white : Colours::black).withAlpha (0.6f * alpha));
    g.drawText (t, tb.translated (1, 1), Justification::centred, true);
    g.setColour (ui::text.withAlpha (alpha));
    g.drawText (t, tb, Justification::centred, true);
}

void SceneView::drawSpectrumOverlay (Graphics& g, Rectangle<float> r)
{
    const auto& L = Look::get();
    switch (ui::design)
    {
    case ui::Vintage:
        g.setColour (Colour (0xff0e0905).withAlpha (0.92f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colour (0xffb8a46a)); g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
        break;
    case ui::Blueprint:
        g.setColour (ui::bg0.withAlpha (0.88f)); g.fillRect (r);
        g.setColour (ui::text); g.drawRect (r.reduced (0.5f), 1.2f);
        break;
    case ui::Studio:
        g.setColour (ui::panel.withAlpha (0.95f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ui::border); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        break;
    case ui::Swiss:
        g.setColour (ui::panel); g.fillRect (r);
        g.setColour (ui::text); g.drawRect (r.reduced (1.0f), 2.0f);
        break;
    case ui::Radar:
        g.setColour (ui::bg0.withAlpha (0.92f)); g.fillRect (r);
        g.setColour (ui::border); g.drawRect (r.reduced (0.5f), 1.0f);
        break;
    default:
        g.setColour (ui::panel.withAlpha (0.86f));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (ui::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        break;
    }

    auto plot = r.reduced (16.0f, 14.0f);
    auto meters = plot.removeFromRight (44.0f);
    plot.removeFromRight (12.0f);

    const float W = plot.getWidth();
    auto xForFreq = [&] (float f) { return plot.getX() + W * std::log (f / 20.0f) / std::log (1000.0f); };

    g.setFont (ui::font (9.5f));
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xForFreq (f);
        g.setColour (ui::border.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        if (f == 100.f || f == 1000.f || f == 10000.f)
        {
            g.setColour (ui::dim);
            g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f),
                        Rectangle<float> (x + 2, plot.getBottom() - 12, 30, 12).toNearestInt(), Justification::left);
        }
    }
    for (float db : { -60.f, -40.f, -20.f })
    {
        const float y = plot.getBottom() - (db + 80.0f) / 75.0f * plot.getHeight();
        g.setColour (ui::border.withAlpha (0.55f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    g.setColour (ui::design == ui::Swiss ? ui::text : ui::dim);
    g.setFont (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (10.0f, true) : ui::font (10.0f, ui::design == ui::Swiss));
    g.drawText (ui::design == ui::Blueprint ? "FIG. 2 - MASTER SPECTRUM" : (ui::design == ui::Radar ? "> MASTER SPECTRUM" : (ui::design == ui::Vintage ? "Master spectrum" : "MASTER SPECTRUM")), Rectangle<float> (plot.getX() + 2, plot.getY() - 4, 200, 14).toNearestInt(), Justification::left);

    const float sr = proc.sampleRateAtomic.load();
    const float binHz = sr / (float) kFFT;
    const int last = (int) specDb.size() - 2;

    auto dbAt = [&] (float f0, float f1)
    {
        const int b0 = jlimit (1, last, (int) (f0 / binHz));
        const int b1 = jlimit (b0, last, (int) std::ceil (f1 / binHz));
        float db = -120.0f;
        if (b1 - b0 < 2)
        {
            const float fb = jlimit (1.0f, (float) last, f1 / binHz);
            const int i0 = (int) fb;
            db = specDb[(size_t) i0] + (specDb[(size_t) i0 + 1] - specDb[(size_t) i0]) * (fb - (float) i0);
        }
        else
            for (int b = b0; b <= b1; ++b) db = jmax (db, specDb[(size_t) b]);
        return db + 4.5f * std::log2 (f1 / 1000.0f);
    };
    auto yFor = [&] (float db) { return plot.getBottom() - jlimit (0.0f, 1.0f, (db + 80.0f) / 75.0f) * plot.getHeight(); };

    if (L.spectrumStyle == 2)
    {
        const int bars = jmax (16, (int) (W / 9.0f));
        const float bw = W / (float) bars;
        for (int i = 0; i < bars; ++i)
        {
            const float f0 = 20.0f * std::pow (1000.0f, (float) i / (float) bars);
            const float f1 = 20.0f * std::pow (1000.0f, (float) (i + 1) / (float) bars);
            const float y = yFor (dbAt (f0, f1));
            const float t = (float) i / (float) bars;
            const bool mono1 = ui::design == ui::Swiss || ui::design == ui::Studio || ui::design == ui::Radar || ui::design == ui::Blueprint;
            const auto bc = mono1 ? ui::cyan : ui::pink.interpolatedWith (ui::cyan, t);
            if (ui::design == ui::Radar)   // LED segments
            {
                for (float yy = plot.getBottom() - 3.0f; yy > y; yy -= 4.0f)
                {
                    g.setColour (bc.withAlpha (0.9f));
                    g.fillRect (plot.getX() + (float) i * bw + 1.0f, yy, bw - 2.0f, 2.5f);
                }
            }
            else
            {
                g.setColour (bc.withAlpha (0.9f));
                const float cr = ui::design == ui::Classic ? 2.0f : (ui::design == ui::Vintage ? 1.0f : 0.0f);
                g.fillRoundedRectangle (plot.getX() + (float) i * bw + 1.0f, y, bw - 2.0f, jmax (1.5f, plot.getBottom() - y), cr);
            }
        }
    }
    else
    {
        Path p;
        p.startNewSubPath (plot.getX(), plot.getBottom());
        float prevX = 0.0f;
        for (int px = 0; px <= (int) W; px += 2)
        {
            const float f1 = 20.0f * std::pow (1000.0f, (float) px / W);
            const float f0 = 20.0f * std::pow (1000.0f, prevX / W);
            prevX = (float) px;
            p.lineTo (plot.getX() + (float) px, yFor (dbAt (f0, f1)));
        }
        if (L.spectrumStyle == 0)
        {
            Path fillP (p);
            fillP.lineTo (plot.getRight(), plot.getBottom());
            fillP.closeSubPath();
            if (ui::design == ui::Blueprint)
            {
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (fillP.createPathWithRoundedCorners (0.0f).getBounds().toNearestInt());
                g.setColour (ui::text.withAlpha (0.45f));
                for (float hx = plot.getX() - plot.getHeight(); hx < plot.getRight(); hx += 6.0f)
                {
                    Path hl; hl.startNewSubPath (hx, plot.getBottom()); hl.lineTo (hx + plot.getHeight(), plot.getY());
                    Path clipped;
                    g.saveState();
                    g.reduceClipRegion (fillP);
                    g.strokePath (hl, PathStrokeType (1.0f));
                    g.restoreState();
                }
            }
            else
            {
                g.setGradientFill (ColourGradient (ui::cyan.withAlpha (ui::design == ui::Swiss ? 0.25f : 0.50f), 0.0f, plot.getY(),
                                                   (ui::design == ui::Classic ? ui::violet : ui::cyan).withAlpha (0.04f), 0.0f, plot.getBottom(), false));
                g.fillPath (fillP);
            }
        }
        g.setColour (ui::design == ui::Blueprint ? ui::text : ui::cyan);
        g.strokePath (p, PathStrokeType (L.spectrumStyle == 1 ? 2.0f : 1.5f));
    }

    const float mw = 12.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = meters.getX() + 4.0f + (float) ch * (mw + 6.0f);
        Rectangle<float> bar (x, plot.getY(), mw, plot.getHeight() - 14.0f);
        g.setColour (ui::bg1);
        g.fillRoundedRectangle (bar, 3.0f);
        const float db = ch == 0 ? meterL : meterR;
        const float h = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * bar.getHeight();
        if (h > 0.5f)
        {
            Rectangle<float> fill (bar.getX(), bar.getBottom() - h, mw, h);
            g.setGradientFill (ColourGradient (ui::pink, 0.0f, bar.getY(), ui::cyan, 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (fill, 3.0f);
        }
        g.setColour (ui::dim);
        g.setFont (ui::font (9.5f, true));
        g.drawText (ch == 0 ? "L" : "R", Rectangle<float> (x - 2, bar.getBottom() + 1, mw + 4, 12).toNearestInt(), Justification::centred);
    }
}

void SceneView::drawPresetButtons (Graphics& g)
{
    static const char* names[4] { "3/4", "TOP", "SIDE", "BEHIND" };
    float x = (float) getWidth() - 16.0f;
    for (int i = 3; i >= 0; --i)
    {
        const float w = i == 3 ? 62.0f : 46.0f;
        x -= w;
        presetRects[(size_t) i] = { x, 14.0f, w, 26.0f };
        x -= 6.0f;
    }
    for (int i = 0; i < 4; ++i)
    {
        const auto r = presetRects[(size_t) i];
        const bool h = i == hoverPreset;
        const auto pl = drawPlate (g, r, h, true);
        g.setColour (ui::design == ui::Classic ? (h ? ui::text : ui::dim) : pl.text);
        g.setFont (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (10.5f, true) : ui::font (10.5f, true));
        g.drawText (names[i], r.toNearestInt(), Justification::centred);
    }
}

void SceneView::drawHud (Graphics& g)
{
    drawPresetButtons (g);
    if (! Look::get().showHud)
        return;

    const bool master = isMaster();
    String line1, line2;
    if (master)
    {
        int n = 0;
        for (auto& v : views) n += v.active ? 1 : 0;
        line1 = String (n) + (n == 1 ? " track in the room" : " tracks in the room");
        line2 = "Drag orbs to reposition tracks" + kDot + "Drag empty space to orbit" + kDot + "Wheel to zoom" + kDot + "Double-click resets view";
        if (selectedSlot >= 0 && views[(size_t) selectedSlot].active)
        {
            const auto& v = views[(size_t) selectedSlot];
            line1 = v.name + "   " + azText (v.pos.x, v.pos.z)
                  + "   height " + String (v.pos.y, 1) + " m"
                  + "   dist " + String (v.pos.length(), 1) + " m";
        }
    }
    else
    {
        const int self = proc.getSlotIndex();
        if (self >= 0 && views[(size_t) self].active)
        {
            const auto& v = views[(size_t) self];
            const float d = v.pos.length();
            const float el = d > 0.001f ? radiansToDegrees (std::asin (jlimit (-1.0f, 1.0f, v.pos.y / d))) : 0.0f;
            line1 = azText (v.pos.x, v.pos.z) + "   elevation " + (el >= 0 ? "+" : "") + String (roundToInt (el)) + kDeg
                  + "   distance " + String (d, 1) + " m";
        }
        line2 = "Drag the orb to move" + kDot + "Shift-drag = height" + kDot + "Drag empty space to orbit" + kDot + "Wheel to zoom";
    }

    const int w1 = jmax (60, (int) textWidth (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (13.0f, true) : ui::font (13.0f, true), line1) + 28);
    Rectangle<float> pill (16.0f, 14.0f, (float) w1, 28.0f);
    Colour hudText = ui::text;
    if (ui::design == ui::Studio)
        g.setColour (Colours::transparentBlack);   // Studio: bare text, no plate
    else
        hudText = drawPlate (g, pill, false, true).text;
    g.setColour (hudText);
    g.setFont (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (13.0f, true) : ui::font (13.0f, true));
    g.drawText (line1, ui::design == ui::Studio ? pill.withX (6.0f).toNearestInt() : pill.toNearestInt(),
                ui::design == ui::Studio ? Justification::centredLeft : Justification::centred);

    g.setFont (ui::design == ui::Radar || ui::design == ui::Blueprint ? ui::mono (10.5f) : ui::font (11.0f));
    g.setColour (ui::design == ui::Swiss ? ui::text : ui::dim);
    g.drawText (line2, Rectangle<int> (18, 48, getWidth() - 36, 16), Justification::left);
}

void SceneView::paint (Graphics& g)
{
    const auto& L = Look::get();
    const auto cam = makeCamera();
    const bool master = isMaster();
    const int self = proc.getSlotIndex();

    drawBackground (g);
    drawRoom (g, cam);

    struct Item { float depth; int id; };
    std::vector<Item> items;
    items.reserve (Registry::maxSlots + 1);

    if (L.showHead)
    {
        Point<float> p; float s, d;
        if (cam.project ({ 0, 0, 0 }, p, s, d)) items.push_back ({ d, -1 });
    }

    for (int i = 0; i < Registry::maxSlots; ++i)
    {
        auto& v = views[(size_t) i];
        v.visible = false;
        if (! v.active) continue;
        Point<float> p; float s, d;
        if (! cam.project (v.dispPos, p, s, d)) continue;
        const bool isSelf = ! master && i == self;
        const float wr = (0.20f + 0.30f * v.lvl + (isSelf ? 0.07f : 0.0f)) * L.orbSize;
        v.sx = p.x; v.sy = p.y; v.sc = s; v.sr = jmax (4.0f, s * wr); v.depth = d; v.visible = true;
        items.push_back ({ d, i });
    }

    // motion path preview + anchor (own orb, Track mode)
    if (! master && self >= 0 && views[(size_t) self].active)
    {
        const int mode = (int) proc.apvts.getRawParameterValue ("motion")->load();
        if (mode != 0)
        {
            const auto& v = views[(size_t) self];
            const float depth = proc.apvts.getRawParameterValue ("mdepth")->load();
            const bool rev = proc.apvts.getRawParameterValue ("mrev")->load() > 0.5f;
            Orbit3DProcessor::Pos prev = Orbit3DProcessor::applyMotion ({ v.base.x, v.base.y, v.base.z }, mode, 0.0f, depth, rev);
            for (int i = 1; i <= 96; ++i)
            {
                auto cur = Orbit3DProcessor::applyMotion ({ v.base.x, v.base.y, v.base.z }, mode, (float) i / 96.0f, depth, rev);
                line3 (g, cam, { prev.x, prev.y, prev.z }, { cur.x, cur.y, cur.z }, ui::amber.withAlpha (0.45f), 1.4f);
                prev = cur;
            }
            Point<float> bp; float bs, bd;
            if (cam.project (v.base, bp, bs, bd))
            {
                g.setColour (ui::amber.withAlpha (0.8f));
                g.drawEllipse (bp.x - 5.0f, bp.y - 5.0f, 10.0f, 10.0f, 1.5f);
            }
        }
    }

    // floor shadows, drop lines, trails, beams
    for (auto& it : items)
    {
        if (it.id < 0) continue;
        const auto& v = views[(size_t) it.id];
        const float al = v.stale ? 0.4f : (master || it.id == self ? 1.0f : 0.5f);
        const auto col = orbColour (it.id);

        Point<float> fp; float fs, fd;
        if (L.floorStyle != 3 && cam.project ({ v.dispPos.x, -room::Y, v.dispPos.z }, fp, fs, fd))
        {
            const float wr = (0.35f + 0.35f * v.lvl) * fs * L.orbSize;
            if (ui::design == ui::Classic || ui::design == ui::Vintage)
            {
                g.setColour (Colours::black.withAlpha ((ui::light ? 0.2f : 0.35f) * al));
                g.fillEllipse (fp.x - wr, fp.y - wr * 0.38f, wr * 2.0f, wr * 0.76f);
                g.setColour (col.withAlpha (0.18f * al));
                g.fillEllipse (fp.x - wr * 0.7f, fp.y - wr * 0.27f, wr * 1.4f, wr * 0.54f);
            }
            else
            {
                g.setColour (col.withAlpha (0.6f * al));
                g.drawLine (fp.x - 4.0f, fp.y, fp.x + 4.0f, fp.y, 1.2f);
                g.drawLine (fp.x, fp.y - 2.5f, fp.x, fp.y + 2.5f, 1.2f);
            }
            g.setColour ((ui::design == ui::Swiss ? ui::text : ui::text).withAlpha ((ui::design == ui::Classic ? 0.13f : 0.3f) * al));
            g.drawLine (v.sx, v.sy, fp.x, fp.y, 1.0f);
        }

        if (L.showTrails && v.trail.size() > 1)
        {
            const float n = (float) v.trail.size();
            for (size_t k = 1; k < v.trail.size(); ++k)
            {
                const float t = (float) k / n;
                line3 (g, cam, v.trail[k - 1], v.trail[k], v.trackColour.withAlpha (0.55f * t * al), 1.0f + 2.2f * t);
            }
        }

        const bool beam = (! master && it.id == self) || (master && it.id == selectedSlot);
        if (beam && L.showHead)
            line3 (g, cam, { 0, 0, 0 }, v.dispPos, col.withAlpha (0.35f), 1.4f);
    }

    std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.depth > b.depth; });
    for (auto& it : items)
    {
        if (it.id < 0) drawListener (g, cam);
        else           drawOrb (g, cam, it.id);
    }

    if (master && L.showSpectrum)
        drawSpectrumOverlay (g, Rectangle<float> (14.0f, (float) getHeight() - 134.0f, (float) getWidth() - 28.0f, 120.0f));

    drawHud (g);
    drawFrameOverlay (g);
}

void SceneView::drawFrameOverlay (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    switch (ui::design)
    {
    case ui::Vintage:   // walnut frame with a brass liner and screws
    {
        const float t = 9.0f;
        g.setColour (Colour (0xff2a170a));
        g.fillRect (b.getX(), b.getY(), b.getWidth(), t);
        g.fillRect (b.getX(), b.getBottom() - t, b.getWidth(), t);
        g.fillRect (b.getX(), b.getY(), t, b.getHeight());
        g.fillRect (b.getRight() - t, b.getY(), t, b.getHeight());
        g.setColour (Colour (0xffb8a46a));
        g.drawRect (b.reduced (t), 1.8f);
        g.setColour (Colour (0xff4a2c17));
        for (float x = 0; x < b.getWidth(); x += 7.0f)
        {
            g.drawLine (x, 0.0f, x + 5.0f, t - 2.0f, 0.8f);
            g.drawLine (x, b.getBottom(), x + 5.0f, b.getBottom() - t + 2.0f, 0.8f);
        }
        for (auto p : { Point<float> (t * 0.5f, t * 0.5f), Point<float> (b.getRight() - t * 0.5f, t * 0.5f),
                        Point<float> (t * 0.5f, b.getBottom() - t * 0.5f), Point<float> (b.getRight() - t * 0.5f, b.getBottom() - t * 0.5f) })
        {
            g.setColour (Colour (0xffb8a46a)); g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
            g.setColour (Colour (0xff3a2a10)); g.drawLine (p.x - 2.0f, p.y + 1.0f, p.x + 2.0f, p.y - 1.0f, 1.0f);
        }
        break;
    }
    case ui::Blueprint:
    {
        g.setColour (ui::text.withAlpha (0.9f));
        g.drawRect (b.reduced (8.0f), 1.0f);
        g.drawRect (b.reduced (11.0f), 1.0f);
        if (! isMaster())
        {
            Rectangle<float> tb (b.getRight() - 236.0f, b.getBottom() - 56.0f, 225.0f, 45.0f);
            g.setColour (ui::bg1); g.fillRect (tb);
            g.setColour (ui::text); g.drawRect (tb, 1.0f);
            g.drawHorizontalLine ((int) (tb.getY() + 22.0f), tb.getX(), tb.getRight());
            g.drawVerticalLine ((int) (tb.getX() + 120.0f), tb.getY() + 22.0f, tb.getBottom());
            g.setFont (ui::mono (11.0f, true));
            g.drawText ("LISTENER PLAN - BINAURAL", tb.withHeight (22.0f).toNearestInt(), Justification::centred);
            g.setFont (ui::mono (9.5f));
            g.drawText ("SCALE 1:1 m", tb.withTrimmedTop (22.0f).withWidth (120.0f).toNearestInt(), Justification::centred);
            g.drawText ("SHEET 1/1", tb.withTrimmedTop (22.0f).withTrimmedLeft (120.0f).toNearestInt(), Justification::centred);
        }
        break;
    }
    case ui::Radar:
    {
        g.setColour (Colours::black.withAlpha (0.16f));
        for (float y = 0; y < b.getHeight(); y += 3.0f) g.drawHorizontalLine ((int) y, 0.0f, b.getWidth());
        ColourGradient vg (Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                           Colours::black.withAlpha (0.55f), b.getCentreX() + b.getWidth() * 0.62f, b.getCentreY() + b.getHeight() * 0.7f, true);
        g.setGradientFill (vg); g.fillRect (b);
        g.setColour (ui::border); g.drawRect (b.reduced (5.0f), 1.0f);
        break;
    }
    case ui::Swiss:
        g.setColour (ui::text);
        g.fillRect (b.getX(), b.getBottom() - 3.0f, b.getWidth(), 3.0f);
        break;
    default: break;
    }
}

//==============================================================================
bool SceneView::canDrag (int slot) const
{
    if (slot < 0 || ! views[(size_t) slot].active) return false;
    return isMaster() ? true : slot == proc.getSlotIndex();
}

int SceneView::pickOrb (Point<float> p) const
{
    int best = -1;
    float bestDepth = 1.0e9f;
    for (int i = 0; i < Registry::maxSlots; ++i)
    {
        const auto& v = views[(size_t) i];
        if (! v.active || ! v.visible) continue;
        const float rr = jmax (v.sr, 10.0f) + 5.0f;
        const float dx = p.x - v.sx, dy = p.y - v.sy;
        if (dx * dx + dy * dy <= rr * rr && v.depth < bestDepth)
        {
            best = i;
            bestDepth = v.depth;
        }
    }
    return best;
}

int SceneView::pickPreset (Point<float> p) const
{
    for (int i = 0; i < 4; ++i)
        if (presetRects[(size_t) i].contains (p))
            return i;
    return -1;
}

bool SceneView::rayPlaneY (Point<float> sp, float y0, V3& hit) const
{
    const auto cam = makeCamera();
    const V3 dir = cam.f + cam.r * ((sp.x - cam.cx) / cam.focal) + cam.u * (-(sp.y - cam.cy) / cam.focal);
    if (std::abs (dir.y) < 1.0e-4f) return false;
    const float t = (y0 - cam.pos.y) / dir.y;
    if (t <= 0.0f) return false;
    hit = cam.pos + dir * t;
    return true;
}

bool SceneView::rayVertical (Point<float> sp, V3 through, V3& hit) const
{
    const auto cam = makeCamera();
    const V3 dir = cam.f + cam.r * ((sp.x - cam.cx) / cam.focal) + cam.u * (-(sp.y - cam.cy) / cam.focal);
    const V3 n = V3 { cam.f.x, 0.0f, cam.f.z }.normalised();
    const float denom = dir.dot (n);
    if (std::abs (denom) < 1.0e-4f) return false;
    const float t = (through - cam.pos).dot (n) / denom;
    if (t <= 0.0f) return false;
    hit = cam.pos + dir * t;
    return true;
}

void SceneView::computeGrab (Point<float> p, bool shift)
{
    if (dragSlot < 0) return;
    const auto& v = views[(size_t) dragSlot];
    V3 hit;
    const bool ok = shift ? rayVertical (p, v.dispPos, hit) : rayPlaneY (p, v.dispPos.y, hit);
    grabOffset = ok ? v.dispPos - hit : V3 {};
}

// np = where the orb should appear; verticalOnly = only the Y axis is being edited
void SceneView::applyMove (int slot, V3 np)
{
    auto& v = views[(size_t) slot];
    const bool vertical = lastShift;
    if (! isMaster())
    {
        const bool motionOn = (int) proc.apvts.getRawParameterValue ("motion")->load() != 0;
        const V3 offset = motionOn ? (v.pos - v.base) : V3 {};
        V3 nb = v.base;
        if (vertical) nb.y = jlimit (-room::Y, room::Y, np.y - offset.y);
        else
        {
            nb.x = jlimit (-room::X, room::X, np.x - offset.x);
            nb.z = jlimit (-room::Z, room::Z, np.z - offset.z);
        }
        v.base = nb;
        proc.setPositionNotifying (nb.x, nb.y, nb.z);
    }
    else
    {
        V3 p = v.pos;
        if (vertical) p.y = np.y; else { p.x = np.x; p.z = np.z; }
        v.pos = p;
        auto& s = Registry::get().slots[(size_t) slot];
        s.pendX = p.x; s.pendY = p.y; s.pendZ = p.z;
        s.hasPending = true;
    }
}

void SceneView::mouseDown (const MouseEvent& e)
{
    downPos = lastMouse = e.position;
    movedFar = false;

    if (const int pb = pickPreset (e.position); pb >= 0)
    {
        setCameraPreset (pb);
        dragMode = Drag::none;
        return;
    }

    const int hit = pickOrb (e.position);
    const bool orbitBtn = e.mods.isRightButtonDown() || e.mods.isMiddleButtonDown();

    if (hit >= 0 && ! orbitBtn && canDrag (hit))
    {
        dragMode = Drag::move;
        dragSlot = hit;
        if (isMaster()) selectedSlot = hit;
        lastShift = e.mods.isShiftDown();
        computeGrab (e.position, lastShift);
        if (! isMaster()) positionGestures (proc, true);
    }
    else
    {
        dragMode = Drag::orbit;
        camAnimating = false;
        if (hit >= 0 && isMaster()) selectedSlot = hit;
    }
}

void SceneView::mouseDrag (const MouseEvent& e)
{
    if (e.position.getDistanceFrom (downPos) > 3.0f) movedFar = true;

    if (dragMode == Drag::orbit)
    {
        const auto d = e.position - lastMouse;
        yaw -= d.x * 0.008f;
        pitch = jlimit (0.05f, 1.52f, pitch + d.y * 0.006f);
    }
    else if (dragMode == Drag::move && dragSlot >= 0)
    {
        const bool shift = e.mods.isShiftDown();
        if (shift != lastShift) { lastShift = shift; computeGrab (e.position, shift); }

        const V3 cur = views[(size_t) dragSlot].dispPos;
        V3 hit;
        const bool ok = shift ? rayVertical (e.position, cur, hit) : rayPlaneY (e.position, cur.y, hit);
        if (ok)
        {
            V3 np = cur;
            if (shift) np.y = hit.y + grabOffset.y;
            else { np.x = hit.x + grabOffset.x; np.z = hit.z + grabOffset.z; }
            applyMove (dragSlot, np);
        }
    }
    lastMouse = e.position;
}

void SceneView::mouseUp (const MouseEvent&)
{
    if (dragMode == Drag::move && ! isMaster())
        positionGestures (proc, false);
    if (dragMode == Drag::orbit && isMaster() && ! movedFar && pickOrb (downPos) < 0)
        selectedSlot = -1;
    dragMode = Drag::none;
    dragSlot = -1;
}

void SceneView::mouseMove (const MouseEvent& e)
{
    hovered = pickOrb (e.position);
    hoverPreset = pickPreset (e.position);
    setMouseCursor ((hovered >= 0 && canDrag (hovered)) || hoverPreset >= 0 ? MouseCursor::PointingHandCursor
                                                                           : MouseCursor::NormalCursor);
}

void SceneView::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    camDist = jlimit (9.0f, 40.0f, camDist * std::exp (-w.deltaY * 0.6f));
}

void SceneView::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = pickOrb (e.position);
    if (hit >= 0 && ! isMaster() && hit == proc.getSlotIndex())
    {
        positionGestures (proc, true);
        proc.setPositionNotifying (0.0f, 0.0f, 2.5f);
        positionGestures (proc, false);
    }
    else if (hit < 0 && pickPreset (e.position) < 0)
        resetCamera();
}
