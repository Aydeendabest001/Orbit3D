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

Colour lineCol (float alpha)
{
    const float a = alpha * Look::get().gridBrightness * (ui::light ? 2.2f : 1.0f);
    return ui::cyan.withAlpha (jlimit (0.0f, 1.0f, a));
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
            g.setColour ((ui::light ? ui::panel : ui::bg1.interpolatedWith (ui::bg0, 0.35f)).withAlpha (0.75f));
            g.fillPath (fp);
        }
    }

    // facing wedge
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
                line3 (g, cam, prev, cur, ui::violet.withAlpha (jlimit (0.0f, 1.0f, 0.15f * L.gridBrightness * (ui::light ? 2.0f : 1.0f))), 1.0f);
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
        Point<float> q; float s, d;
        if (cam.project (p, q, s, d))
        {
            g.setColour (ui::dim.withAlpha (alpha));
            g.setFont (ui::font (size, true));
            g.drawText (t, Rectangle<float> (q.x - 50, q.y - 8, 100, 16).toNearestInt(), Justification::centred);
        }
    };
    label ({ 0, fy, Z + 1.1f }, "FRONT", 11.0f, 0.9f);
    label ({ 0, fy, -Z - 1.1f }, "BACK", 11.0f, 0.9f);
    label ({ -X - 1.1f, fy, 0 }, "LEFT", 11.0f, 0.9f);
    label ({ X + 1.1f, fy, 0 }, "RIGHT", 11.0f, 0.9f);
    if (L.showRings)
        for (int r = 2; r <= 8; r += 2)
            label ({ (float) r + 0.45f, fy, 0.35f }, String (r) + " m", 9.5f, 0.55f);
    if (L.showWalls)
        label ({ -X, room::Y + 0.5f, -Z }, "UP", 9.5f, 0.45f);
}

void SceneView::drawListener (Graphics& g, const Camera& cam)
{
    const auto& L = Look::get();
    Point<float> c; float s, d;
    if (! cam.project ({ 0, 0, 0 }, c, s, d))
        return;

    const float r = s * (L.headStyle == 1 ? 0.28f : 0.55f);
    const Colour headLight = ui::light ? Colours::white : Colour (0xfff2f6ff);
    const Colour headDark  = ui::light ? ui::cyan.withMultipliedBrightness (0.8f) : Colour (0xff6b7cae);

    ColourGradient halo ((ui::light ? ui::cyan : Colours::white).withAlpha (0.16f), c.x, c.y,
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
        default: // Glow
        {
            glow (2.6f + 1.6f * v.lvl, 0.15f + 0.5f * v.lvl);
            g.setGradientFill (ColourGradient (col.brighter (0.8f).withAlpha (alpha), cx - r * 0.35f, cy - r * 0.4f,
                                               col.darker (0.55f).withAlpha (alpha), cx + r * 0.8f, cy + r * 0.85f, true));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            break;
        }
    }

    if (L.orbStyle != 2)
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
    {
        const auto t = v.name.isEmpty() ? String ("Track") : v.name;
        const Rectangle<int> tb ((int) cx - 90, (int) (cy + r + 5.0f), 180, (int) (16.0f * L.labelSize) + 2);
        g.setFont (ui::font (11.5f * L.labelSize, self || sel));
        g.setColour ((ui::light ? Colours::white : Colours::black).withAlpha (0.6f * alpha));
        g.drawText (t, tb.translated (1, 1), Justification::centred, true);
        g.setColour (ui::text.withAlpha (alpha));
        g.drawText (t, tb, Justification::centred, true);
    }
}

void SceneView::drawSpectrumOverlay (Graphics& g, Rectangle<float> r)
{
    const auto& L = Look::get();
    g.setColour (ui::panel.withAlpha (0.86f));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (ui::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

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

    g.setColour (ui::dim);
    g.setFont (ui::font (10.0f, true));
    g.drawText ("MASTER SPECTRUM", Rectangle<float> (plot.getX() + 2, plot.getY() - 4, 140, 14).toNearestInt(), Justification::left);

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
            g.setColour (ui::pink.interpolatedWith (ui::cyan, t).withAlpha (0.9f));
            g.fillRoundedRectangle (plot.getX() + (float) i * bw + 1.0f, y, bw - 2.0f, jmax (1.5f, plot.getBottom() - y), 2.0f);
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
            g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.50f), 0.0f, plot.getY(),
                                               ui::violet.withAlpha (0.04f), 0.0f, plot.getBottom(), false));
            g.fillPath (fillP);
        }
        g.setColour (ui::cyan);
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
        g.setColour (ui::panel.withAlpha (h ? 0.95f : 0.8f));
        g.fillRoundedRectangle (r, 13.0f);
        g.setColour (h ? ui::cyan : ui::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 13.0f, 1.0f);
        g.setColour (h ? ui::text : ui::dim);
        g.setFont (ui::font (10.5f, true));
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

    const int w1 = jmax (60, (int) textWidth (ui::font (13.0f, true), line1) + 28);
    Rectangle<float> pill (16.0f, 14.0f, (float) w1, 28.0f);
    g.setColour (ui::panel.withAlpha (0.8f));
    g.fillRoundedRectangle (pill, 14.0f);
    g.setColour (ui::border);
    g.drawRoundedRectangle (pill.reduced (0.5f), 14.0f, 1.0f);
    g.setColour (ui::text);
    g.setFont (ui::font (13.0f, true));
    g.drawText (line1, pill.toNearestInt(), Justification::centred);

    g.setFont (ui::font (11.0f));
    g.setColour (ui::dim);
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
            g.setColour (Colours::black.withAlpha ((ui::light ? 0.2f : 0.35f) * al));
            g.fillEllipse (fp.x - wr, fp.y - wr * 0.38f, wr * 2.0f, wr * 0.76f);
            g.setColour (col.withAlpha (0.18f * al));
            g.fillEllipse (fp.x - wr * 0.7f, fp.y - wr * 0.27f, wr * 1.4f, wr * 0.54f);
            g.setColour (ui::text.withAlpha (0.13f * al));
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
