#include "PluginEditor.h"

using namespace juce;

namespace
{
const String kDeg = String::fromUTF8 ("\xc2\xb0");

float toDb (float lin) { return 20.0f * std::log10 (jmax (lin, 1.0e-6f)); }

void positionGestures (Orbit3DProcessor& p, bool begin)
{
    for (auto* id : { "x", "y", "z" })
        if (auto* prm = p.apvts.getParameter (id))
        {
            if (begin) prm->beginChangeGesture(); else prm->endChangeGesture();
        }
}

float textWidth (const Font& f, const String& t)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void paintSections (Graphics& g, const std::vector<std::pair<String, Rectangle<int>>>& sections)
{
    const auto f = ui::design == ui::Radar ? ui::mono (10.5f, true) : ui::font (10.5f, true);
    g.setFont (f);
    for (auto& s : sections)
    {
        String t = s.first;
        if (ui::design == ui::Vintage) t = t.toLowerCase();
        if (ui::design == ui::Radar) t = "> " + t;
        g.setColour (ui::design == ui::Swiss ? pn::text : pn::dim);
        g.drawText (t, s.second, Justification::centredLeft);
        const int tw = (int) textWidth (f, t) + 10;
        const float ly = (float) s.second.getCentreY();
        if (ui::design == ui::Swiss)
        {
            g.setColour (pn::border);
            g.fillRect ((float) (s.second.getX() + tw), ly - 1.0f, (float) (s.second.getRight() - s.second.getX() - tw), 2.0f);
        }
        else if (ui::design == ui::Vintage)
        {
            g.setColour (pn::border);
            g.drawHorizontalLine ((int) ly - 1, (float) (s.second.getX() + tw), (float) s.second.getRight());
            g.setColour (Colours::white.withAlpha (0.5f));
            g.drawHorizontalLine ((int) ly, (float) (s.second.getX() + tw), (float) s.second.getRight());
        }
        else if (ui::design == ui::Blueprint)
        {
            const float dl[] = { 4.0f, 3.0f };
            g.setColour (pn::border);
            g.drawDashedLine (Line<float> ((float) (s.second.getX() + tw), ly, (float) s.second.getRight(), ly), dl, 2, 1.0f);
        }
        else
        {
            g.setColour (pn::border);
            g.drawHorizontalLine ((int) ly, (float) (s.second.getX() + tw), (float) s.second.getRight());
        }
    }
}

void paintCard (Graphics& g, Rectangle<float> r)
{
    switch (ui::design)
    {
    case ui::Vintage:
        g.setColour (Colour (0xff2a1d10));            // recessed dark glass window
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colour (0xffb8a46a));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 2.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (3.0f), 2.0f, 1.0f);
        break;
    case ui::Blueprint:
        g.setColour (pn::bg0.withAlpha (0.6f)); g.fillRect (r);
        g.setColour (pn::text); g.drawRect (r.reduced (0.5f), 1.0f);
        break;
    case ui::Swiss:
        g.setColour (pn::bg0); g.fillRect (r);
        g.setColour (pn::border); g.drawRect (r.reduced (1.0f), 2.0f);
        break;
    case ui::Radar:
        g.setColour (pn::bg0); g.fillRect (r);
        g.setColour (pn::border); g.drawRect (r.reduced (0.5f), 1.0f);
        break;
    case ui::Studio:
        g.setColour (pn::bg1); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (pn::border); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        break;
    default:
        g.setColour (pn::bg1);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (pn::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        break;
    }
}

// Cards that sit on a window: text must be legible on the *card* colour, not the panel.
Colour cardText() { return ui::design == ui::Vintage ? Colour (0xfff0e2c0) : pn::text; }
Colour cardBar() { return ui::design == ui::Vintage ? Colour (0xff3d2d1b) : (ui::design == ui::Swiss ? pn::border.withAlpha (0.15f) : pn::panel); }
Colour cardDim()  { return ui::design == ui::Vintage ? Colour (0xff9a8760) : pn::dim; }

void paintRow (Graphics& g, Rectangle<float> rr, bool sel)
{
    const float cr = ui::design == ui::Classic ? 7.0f : (ui::design == ui::Studio ? 3.0f : (ui::design == ui::Vintage ? 2.0f : 0.0f));
    g.setColour (sel ? pn::cyan.withAlpha (ui::design == ui::Swiss ? 1.0f : 0.14f) : (ui::design == ui::Blueprint ? Colours::transparentBlack : pn::bg1));
    g.fillRoundedRectangle (rr, cr);
    g.setColour (sel ? (ui::design == ui::Swiss ? pn::border : pn::cyan) : pn::border);
    g.drawRoundedRectangle (rr.reduced (0.5f), cr, ui::design == ui::Swiss ? 1.6f : 1.0f);
}

Colour spectrumColourFor (const SlotView& v)
{
    const float wl = v.low * 0.7f, wm = v.mid * 1.8f, wh = v.high * 5.0f;
    const float sum = wl + wm + wh;
    if (sum < 1.0e-4f)
        return v.trackColour.withMultipliedBrightness (0.6f);
    const auto lo = pn::pink, mi = pn::amber, hi = pn::cyan;
    const float a = wl / sum, b = wm / sum, c = wh / sum;
    return Colour::fromFloatRGBA (a * lo.getFloatRed()   + b * mi.getFloatRed()   + c * hi.getFloatRed(),
                                  a * lo.getFloatGreen() + b * mi.getFloatGreen() + c * hi.getFloatGreen(),
                                  a * lo.getFloatBlue()  + b * mi.getFloatBlue()  + c * hi.getFloatBlue(), 1.0f);
}
}

//==============================================================================
OrbitLookAndFeel::OrbitLookAndFeel() { refresh(); }

void OrbitLookAndFeel::refresh()
{
    const bool vint = ui::design == ui::Vintage;
    setColour (Slider::textBoxTextColourId, pn::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, pn::cyan.withAlpha (0.3f));
    setColour (TextEditor::backgroundColourId, vint ? pn::bg0.darker (0.08f) : pn::bg1);
    setColour (TextEditor::textColourId, pn::text);
    setColour (TextEditor::outlineColourId, pn::border);
    setColour (TextEditor::focusedOutlineColourId, pn::cyan);
    setColour (TextEditor::highlightColourId, pn::cyan.withAlpha (0.3f));
    setColour (TextEditor::highlightedTextColourId, pn::text);
    setColour (CaretComponent::caretColourId, pn::cyan);
    setColour (TextButton::textColourOffId, pn::dim);
    setColour (TextButton::textColourOnId, (ui::design == ui::Blueprint || ui::design == ui::Swiss) ? pn::bg0 : pn::text);
    setColour (Label::textColourId, pn::text);
    setColour (ComboBox::textColourId, pn::text);
    setColour (ComboBox::backgroundColourId, pn::bg1);
    setColour (ComboBox::outlineColourId, pn::border);
    setColour (PopupMenu::backgroundColourId, pn::panel);
    setColour (PopupMenu::textColourId, pn::text);
    setColour (PopupMenu::highlightedBackgroundColourId, pn::cyan.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, pn::text);
    setColour (ScrollBar::thumbColourId, pn::border.brighter (pn::light ? 0.0f : 0.7f));
    setColour (ScrollBar::backgroundColourId, Colours::transparentBlack);
}

namespace
{
float cornerFor()
{
    switch (ui::design) { case ui::Classic: return 7.0f; case ui::Vintage: return 2.5f; case ui::Studio: return 3.0f; default: return 0.0f; }
}

void bevelRect (Graphics& g, Rectangle<float> r, Colour face, bool pressed)
{
    g.setColour (face);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour ((pressed ? Colours::black : Colours::white).withAlpha (pressed ? 0.30f : 0.45f));
    g.drawLine (r.getX() + 1.0f, r.getY() + 0.8f, r.getRight() - 1.0f, r.getY() + 0.8f, 1.2f);
    g.drawLine (r.getX() + 0.8f, r.getY() + 1.0f, r.getX() + 0.8f, r.getBottom() - 1.0f, 1.2f);
    g.setColour ((pressed ? Colours::white : Colours::black).withAlpha (pressed ? 0.25f : 0.35f));
    g.drawLine (r.getX() + 1.0f, r.getBottom() - 0.8f, r.getRight() - 1.0f, r.getBottom() - 0.8f, 1.2f);
    g.drawLine (r.getRight() - 0.8f, r.getY() + 1.0f, r.getRight() - 0.8f, r.getBottom() - 1.0f, 1.2f);
    g.setColour (pn::border);
    g.drawRoundedRectangle (r, 2.5f, 1.0f);
}

void brackets (Graphics& g, Rectangle<float> r, Colour c, float len = 6.0f, float th = 1.3f)
{
    g.setColour (c);
    for (int sx = 0; sx < 2; ++sx)
        for (int sy = 0; sy < 2; ++sy)
        {
            const float x = sx ? r.getRight() : r.getX(), y = sy ? r.getBottom() : r.getY();
            const float dx = sx ? -len : len, dy = sy ? -len : len;
            g.drawLine (x, y, x + dx, y, th);
            g.drawLine (x, y, x, y + dy, th);
        }
}
}

void OrbitLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float a0, float a1, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float arcR = d * 0.5f - 3.0f;
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const auto accent = pn::accent ((int) s.getProperties().getWithDefault ("accent", 0));
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    auto pt = [&] (float a, float r) { return Point<float> (c.x + std::sin (a) * r, c.y - std::cos (a) * r); };

    switch (ui::design)
    {
    case ui::Vintage:
    {
        // numbered scale ticks, bakelite knob with a brass cap and an ivory pointer
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 10.0f;
            const bool major = (i % 5) == 0;
            g.setColour (pn::text.withAlpha (major ? 0.9f : 0.55f));
            g.drawLine (Line<float> (pt (a, arcR - (major ? 5.0f : 3.0f)), pt (a, arcR)), major ? 1.5f : 1.0f);
        }
        const float br = arcR - 8.0f;
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - br + 1.5f, c.y - br + 2.5f, br * 2.0f, br * 2.0f);
        g.setGradientFill (ColourGradient (Colour (0xff4a3526), c.x - br * 0.5f, c.y - br * 0.6f,
                                           Colour (0xff1c120b), c.x + br * 0.7f, c.y + br * 0.8f, true));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (Colour (0xffb8a46a));      // brass rim
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.6f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillEllipse (c.x - br * 0.62f, c.y - br * 0.85f, br * 0.9f, br * 0.5f);
        const float cr = br * 0.34f;
        g.setGradientFill (ColourGradient (Colour (0xffe6d49a), c.x - cr, c.y - cr, Colour (0xff8a7436), c.x + cr, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (Colour (0xfff6ead0));
        g.drawLine (Line<float> (pt (ang, br * 0.40f), pt (ang, br * 0.94f)), 3.0f);
        break;
    }
    case ui::Blueprint:
    {
        g.setColour (pn::border);
        g.drawEllipse (c.x - arcR, c.y - arcR, arcR * 2.0f, arcR * 2.0f, 1.0f);
        for (int i = 0; i <= 20; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 20.0f;
            const bool major = (i % 5) == 0;
            g.setColour (pn::text.withAlpha (major ? 0.95f : 0.5f));
            g.drawLine (Line<float> (pt (a, arcR - (major ? 6.0f : 3.0f)), pt (a, arcR)), 1.0f);
        }
        if (std::abs (ang - from) > 0.002f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, arcR - 9.0f, arcR - 9.0f, 0.0f, jmin (from, ang), jmax (from, ang), true);
            Path dashed;
            const float dl[] = { 3.0f, 2.5f };
            PathStrokeType (1.4f).createDashedStroke (dashed, val, dl, 2);
            g.setColour (accent);
            g.fillPath (dashed);
        }
        const float br = arcR - 14.0f;
        g.setColour (pn::bg0.withAlpha (0.8f));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (pn::text);
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);
        g.drawLine (Line<float> (c, pt (ang, br)), 1.4f);
        g.drawLine (c.x - 3.0f, c.y, c.x + 3.0f, c.y, 1.0f);
        g.drawLine (c.x, c.y - 3.0f, c.x, c.y + 3.0f, 1.0f);
        break;
    }
    case ui::Studio:
    {
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (pn::border);
        g.strokePath (track, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::butt));
        if (std::abs (ang - from) > 0.002f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, ang), jmax (from, ang), true);
            g.setColour (accent);
            g.strokePath (val, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::butt));
        }
        const float br = arcR - 6.0f;
        g.setColour (pn::bg1.brighter (0.06f));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (pn::border.brighter (0.2f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);
        g.setColour (pn::text);
        g.drawLine (Line<float> (pt (ang, br * 0.45f), pt (ang, br * 0.9f)), 2.0f);
        break;
    }
    case ui::Swiss:
    {
        const float br = arcR - 1.0f;
        g.setColour (pn::border);
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (pn::bg0);
        g.drawLine (Line<float> (c, pt (ang, br - 3.0f)), 3.0f);
        // value shown as a thick red/blue wedge outside the knob
        if (std::abs (ang - from) > 0.002f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, br + 0.5f, br + 0.5f, 0.0f, jmin (from, ang), jmax (from, ang), true);
            g.setColour (accent);
            g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::butt));
        }
        break;
    }
    case ui::Radar:
    {
        const int n = 21;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) (n - 1);
            const float a = a0 + (a1 - a0) * t;
            const float tp = bipolar ? 0.5f : 0.0f;
            const bool lit = bipolar ? (t >= jmin (tp, pos) - 0.001f && t <= jmax (tp, pos) + 0.001f)
                                     : (t <= pos + 0.001f);
            g.setColour (lit ? accent : pn::border);
            g.drawLine (Line<float> (pt (a, arcR - 5.0f), pt (a, arcR)), lit ? 2.4f : 1.6f);
        }
        const float br = arcR - 10.0f;
        g.setColour (pn::border);
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);
        g.setColour (accent);
        g.drawLine (Line<float> (pt (ang, br * 0.25f), pt (ang, br)), 1.6f);
        g.fillEllipse (c.x - 1.8f, c.y - 1.8f, 3.6f, 3.6f);
        break;
    }
    default:
    {
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (pn::border);
        g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        if (std::abs (ang - from) > 0.002f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, ang), jmax (from, ang), true);
            g.setColour (accent.withAlpha (0.16f));
            g.strokePath (val, PathStrokeType (10.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (val, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const float br = arcR - 6.0f;
        g.setGradientFill (ColourGradient::vertical (pn::bg1.brighter (0.35f), c.y - br, pn::panel, c.y + br));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (pn::border.brighter (0.35f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);
        g.setColour (accent.brighter (0.4f));
        g.drawLine (Line<float> (pt (ang, br * 0.30f), pt (ang, br * 0.86f)), 2.6f);
        break;
    }
    }
}

void OrbitLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos,
                                         float minPos, float maxPos, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }
    const auto acc = pn::accent ((int) s.getProperties().getWithDefault ("accent", 0));
    const float cy = (float) y + (float) h * 0.5f;
    Rectangle<float> track ((float) x + 3.0f, cy - 2.0f, (float) w - 6.0f, 4.0f);

    switch (ui::design)
    {
    case ui::Vintage:
    {
        auto groove = track.withHeight (5.0f).withCentre ({ track.getCentreX(), cy });
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (groove, 2.0f);
        g.setColour (pn::border);
        g.drawRoundedRectangle (groove, 2.0f, 1.0f);
        Rectangle<float> cap (sliderPos - 6.0f, cy - 10.0f, 12.0f, 20.0f);
        bevelRect (g, cap, Colour (0xff3a2a1c), false);
        g.setColour (Colour (0xfff0e2c0));
        g.drawLine (sliderPos, cy - 8.0f, sliderPos, cy + 8.0f, 1.6f);
        return;
    }
    case ui::Blueprint:
        g.setColour (pn::text.withAlpha (0.8f));
        g.drawLine (track.getX(), cy, track.getRight(), cy, 1.0f);
        for (int i = 0; i <= 10; ++i)
            g.drawLine (track.getX() + track.getWidth() * (float) i / 10.0f, cy - 3.0f,
                        track.getX() + track.getWidth() * (float) i / 10.0f, cy + 3.0f, 1.0f);
        g.setColour (pn::cyan);
        g.drawLine (sliderPos, cy - 8.0f, sliderPos, cy + 8.0f, 1.6f);
        {
            Path tri; tri.addTriangle (sliderPos - 4.0f, cy - 12.0f, sliderPos + 4.0f, cy - 12.0f, sliderPos, cy - 6.0f);
            g.fillPath (tri);
        }
        return;
    case ui::Studio:
        g.setColour (pn::border);
        g.fillRect (track.withHeight (2.0f).withCentre ({ track.getCentreX(), cy }));
        g.setColour (acc);
        g.fillRect (Rectangle<float> (track.getX(), cy - 1.0f, jmax (1.0f, sliderPos - track.getX()), 2.0f));
        g.setColour (pn::text);
        g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        return;
    case ui::Swiss:
        g.setColour (pn::border.withAlpha (0.18f));
        g.fillRect (track.withHeight (8.0f).withCentre ({ track.getCentreX(), cy }));
        g.setColour (pn::border);
        g.fillRect (Rectangle<float> (track.getX(), cy - 4.0f, jmax (1.0f, sliderPos - track.getX()), 8.0f));
        g.setColour (acc);
        g.fillRect (Rectangle<float> (sliderPos - 2.5f, cy - 10.0f, 5.0f, 20.0f));
        return;
    case ui::Radar:
    {
        const int n = 24;
        for (int i = 0; i < n; ++i)
        {
            const float px = track.getX() + track.getWidth() * (float) i / (float) (n - 1);
            g.setColour (px <= sliderPos ? acc : pn::border);
            g.fillRect (px - 1.2f, cy - 5.0f, 2.4f, 10.0f);
        }
        return;
    }
    default: break;
    }
    g.setColour (pn::border);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (acc);
    g.fillRoundedRectangle (track.withRight (jmax (track.getX() + 1.0f, sliderPos)), 2.0f);
    g.setColour (acc.withAlpha (0.22f));
    g.fillEllipse (sliderPos - 9.0f, cy - 9.0f, 18.0f, 18.0f);
    g.setColour (pn::text);
    g.fillEllipse (sliderPos - 5.5f, cy - 5.5f, 11.0f, 11.0f);
    g.setColour (acc);
    g.drawEllipse (sliderPos - 5.5f, cy - 5.5f, 11.0f, 11.0f, 1.5f);
}

void OrbitLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    switch (ui::design)
    {
    case ui::Vintage:
        bevelRect (g, r, on ? Colour (0xffb89a5a) : pn::bg1.darker (hover ? 0.0f : 0.05f), on || down);
        return;
    case ui::Blueprint:
        g.setColour (on ? pn::text : Colours::transparentBlack);
        g.fillRect (r);
        g.setColour (hover || on ? pn::text : pn::dim);
        g.drawRect (r, 1.0f);
        return;
    case ui::Swiss:
        g.setColour (on ? pn::border : (hover ? pn::bg1 : pn::bg0));
        g.fillRect (r);
        g.setColour (pn::border);
        g.drawRect (r, 1.6f);
        return;
    case ui::Radar:
        g.setColour (on ? pn::cyan.withAlpha (0.18f) : Colours::transparentBlack);
        g.fillRect (r);
        brackets (g, r, on ? pn::cyan : (hover ? pn::text : pn::dim));
        return;
    case ui::Studio:
        g.setColour (on ? pn::cyan.withAlpha (0.16f) : (hover ? pn::bg1.brighter (0.08f) : pn::bg1));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? pn::cyan : pn::border);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        return;
    default: break;
    }
    auto base = on ? pn::cyan.withAlpha (0.20f) : pn::bg1;
    if (hover) base = base.brighter (0.12f);
    if (down)  base = base.brighter (0.25f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? pn::cyan : (hover ? pn::dim : pn::border));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void OrbitLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool hover, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    float sw = 34.0f;
    const float sh = 18.0f;
    Rectangle<float> pill (r.getX() + 2.0f, r.getCentreY() - sh * 0.5f, sw, sh);

    switch (ui::design)
    {
    case ui::Vintage:
    {
        // pilot lamp in a brass bezel
        sw = 22.0f;
        const auto c = Point<float> (r.getX() + 12.0f, r.getCentreY());
        g.setColour (Colour (0xffb8a46a));
        g.fillEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
        g.setColour (Colour (0xff2a1d10));
        g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
        if (on)
        {
            g.setColour (pn::cyan.withAlpha (0.30f));
            g.fillEllipse (c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
            g.setGradientFill (ColourGradient (Colour (0xffffe9a8), c.x - 2.0f, c.y - 2.5f, Colour (0xffe0902a), c.x + 6.0f, c.y + 6.0f, true));
        }
        else g.setColour (Colour (0xff4a3a28));
        g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f);
        break;
    }
    case ui::Blueprint:
    {
        sw = 18.0f;
        Rectangle<float> bx (r.getX() + 2.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour (pn::text);
        g.drawRect (bx, 1.2f);
        if (on) { g.drawLine (bx.getX() + 2, bx.getY() + 2, bx.getRight() - 2, bx.getBottom() - 2, 1.6f);
                  g.drawLine (bx.getRight() - 2, bx.getY() + 2, bx.getX() + 2, bx.getBottom() - 2, 1.6f); }
        break;
    }
    case ui::Swiss:
    {
        sw = 20.0f;
        Rectangle<float> bx (r.getX() + 2.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f);
        g.setColour (on ? pn::border : pn::bg0);
        g.fillRect (bx);
        g.setColour (pn::border);
        g.drawRect (bx, 2.0f);
        if (on) { g.setColour (pn::cyan); g.fillRect (bx.reduced (5.0f)); }
        break;
    }
    case ui::Radar:
    {
        sw = 28.0f;
        g.setColour (on ? pn::cyan : pn::dim);
        g.setFont (ui::mono (13.0f, true));
        g.drawText (on ? "[X]" : "[ ]", Rectangle<float> (r.getX(), r.getY(), sw, r.getHeight()).toNearestInt(), Justification::centredLeft);
        break;
    }
    case ui::Studio:
    {
        sw = 28.0f;
        Rectangle<float> p2 (r.getX() + 2.0f, r.getCentreY() - 7.0f, 26.0f, 14.0f);
        g.setColour (on ? pn::cyan : pn::border);
        g.fillRoundedRectangle (p2, 3.0f);
        g.setColour (on ? pn::bg0 : pn::dim);
        g.fillRoundedRectangle (Rectangle<float> (on ? p2.getRight() - 12.0f : p2.getX() + 2.0f, p2.getY() + 2.0f, 10.0f, 10.0f), 2.0f);
        break;
    }
    default:
    {
        g.setColour (on ? pn::cyan.withAlpha (0.25f) : pn::bg1);
        g.fillRoundedRectangle (pill, sh * 0.5f);
        g.setColour (on ? pn::cyan : pn::border);
        g.drawRoundedRectangle (pill, sh * 0.5f, 1.2f);
        const float kx = on ? pill.getRight() - sh + 2.0f : pill.getX() + 2.0f;
        g.setColour (on ? pn::cyan : pn::dim);
        g.fillEllipse (kx, pill.getY() + 2.0f, sh - 4.0f, sh - 4.0f);
        break;
    }
    }
    g.setColour (hover ? pn::text : pn::text.withAlpha (0.85f));
    g.setFont (ui::design == ui::Radar ? ui::mono (12.0f) : ui::font (12.5f, ui::design == ui::Swiss));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (sw + 12.0f).toNearestInt(), Justification::centredLeft);
}

void OrbitLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    Rectangle<float> r (0.0f, 0.0f, (float) w, (float) h);
    const float cr = cornerFor();
    const bool focus = box.hasKeyboardFocus (true);
    if (ui::design == ui::Radar)
    {
        brackets (g, r.reduced (0.5f), focus ? pn::cyan : pn::dim);
    }
    else if (ui::design == ui::Vintage)
    {
        g.setColour (Colour (0xff2a1d10));
        g.fillRoundedRectangle (r.reduced (0.5f), cr);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawLine (1.0f, 1.0f, (float) w - 1.0f, 1.0f, 2.0f);
        g.setColour (pn::border);
        g.drawRoundedRectangle (r.reduced (0.5f), cr, 1.0f);
    }
    else
    {
        g.setColour (ui::design == ui::Blueprint ? Colours::transparentBlack : pn::bg1);
        g.fillRoundedRectangle (r.reduced (0.5f), cr);
        g.setColour (focus ? pn::cyan : (ui::design == ui::Swiss ? pn::border : pn::border));
        g.drawRoundedRectangle (r.reduced (0.5f), cr, ui::design == ui::Swiss ? 1.6f : 1.0f);
    }
    Path p;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (ui::design == ui::Vintage ? Colour (0xffe8d9a8) : pn::dim);
    g.fillPath (p);
}

void OrbitLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (Label::textColourId, ui::design == ui::Vintage ? Colour (0xfff0e2c0) : pn::text);
}

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& state, const String& id, const String& title, int accentIndex, bool bipolar)
    : titleText (title)
{
    setup (accentIndex, bipolar);
    if (auto* prm = state.getParameter (id))
    {
        attachment = std::make_unique<SliderParameterAttachment> (*prm, slider);
        slider.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
    }
}

Knob::Knob (const String& title, int accentIndex, bool bipolar,
            double lo, double hi, double def, double step, std::function<String (double)> textFn)
    : titleText (title)
{
    setup (accentIndex, bipolar);
    slider.setRange (lo, hi, step);
    slider.setValue (def, dontSendNotification);
    slider.setDoubleClickReturnValue (true, def);
    slider.textFromValueFunction = std::move (textFn);
    slider.updateText();
}

void Knob::setup (int accentIndex, bool bipolar)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 72, 18);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", accentIndex);
    addAndMakeVisible (slider);
    lookAndFeelChanged();
}

void Knob::lookAndFeelChanged()
{
    slider.setColour (Slider::textBoxTextColourId, pn::text);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxHighlightColourId, pn::cyan.withAlpha (0.3f));
}

void Knob::paint (Graphics& g)
{
    g.setColour (ui::design == ui::Swiss ? pn::text : pn::dim);
    g.setFont (ui::design == ui::Radar ? ui::mono (10.0f, true) : ui::font (10.5f, true));
    g.drawText (ui::design == ui::Vintage ? titleText.toLowerCase() : (ui::design == ui::Radar ? titleText.toUpperCase() : titleText),
                getLocalBounds().removeFromTop (14), Justification::centred);
}

void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (14)); }

//==============================================================================
MovePage::MovePage (Orbit3DProcessor& p)
    : proc (p),
      kx (p.apvts, "x", "LEFT / RIGHT", 0, true),
      ky (p.apvts, "y", "DOWN / UP", 1, true),
      kz (p.apvts, "z", "BACK / FRONT", 2, true),
      kAz ("AZIMUTH", 0, true, -180.0, 180.0, 0.0, 1.0, [] (double v) { return String (roundToInt (v)) + kDeg; }),
      kEl ("ELEVATION", 1, true, -90.0, 90.0, 0.0, 1.0, [] (double v) { return String (roundToInt (v)) + kDeg; }),
      kDist ("DISTANCE", 2, false, 0.0, 10.0, 2.5, 0.05, [] (double v) { return String (v, 1) + " m"; }),
      kRate (p.apvts, "mrate", "RATE", 3, false),
      kDepth (p.apvts, "mdepth", "DEPTH", 1, false)
{
    nameEditor.setMultiLine (false);
    nameEditor.setTextToShowWhenEmpty ("Track name (auto from Reaper)", pn::dim);
    nameEditor.setFont (ui::font (13.0f));
    nameEditor.setInputRestrictions (40);
    nameEditor.setText (proc.getLabel(), false);
    nameEditor.setIndents (10, 6);
    nameEditor.onTextChange = [this] { proc.setLabel (nameEditor.getText()); };
    addAndMakeVisible (nameEditor);

    for (auto* b : { &xyzBtn, &sphBtn })
    {
        b->setRadioGroupId (4712);
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    xyzBtn.onClick = [this] { setSpherical (false); };
    sphBtn.onClick = [this] { setSpherical (true); };

    for (auto* k : { &kx, &ky, &kz, &kAz, &kEl, &kDist, &kRate, &kDepth })
        addAndMakeVisible (*k);

    for (auto* k : { &kAz, &kEl, &kDist })
    {
        k->slider.onValueChange = [this] { if (! updatingSph) applySpherical(); };
        k->slider.onDragStart = [this] { positionGestures (proc, true); };
        k->slider.onDragEnd = [this] { positionGestures (proc, false); };
    }

    struct Preset { const char* name; float x, y, z; };
    static const Preset list[] = {
        { "Front", 0, 0, 2.5f }, { "Back", 0, 0, -2.5f }, { "Left", -2.5f, 0, 0.0f }, { "Right", 2.5f, 0, 0.0f },
        { "Above", 0, 3.2f, 0.4f }, { "Below", 0, -3.2f, 0.4f }, { "Inside", 0, 0, 0.0f }, { "Far", 0, 0.4f, 7.0f }
    };
    for (auto& pr : list)
    {
        auto* b = presets.add (new TextButton (pr.name));
        const float px = pr.x, py = pr.y, pz = pr.z;
        b->onClick = [this, px, py, pz] { setPositionWithGesture (px, py, pz); };
        addAndMakeVisible (b);
    }

    if (auto* cp = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter ("motion")))
        motionBox.addItemList (cp->choices, 1);
    if (auto* cp = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter ("mdiv")))
        divBox.addItemList (cp->choices, 1);
    addAndMakeVisible (motionBox);
    addAndMakeVisible (divBox);
    motionAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "motion", motionBox);
    divAttach    = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mdiv", divBox);
    addAndMakeVisible (syncToggle);
    addAndMakeVisible (revToggle);
    syncAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "msync", syncToggle);
    revAttach  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mrev", revToggle);

    setSpherical (false);
    startTimerHz (20);
}

void MovePage::setPositionWithGesture (float x, float y, float z)
{
    positionGestures (proc, true);
    proc.setPositionNotifying (x, y, z);
    positionGestures (proc, false);
}

void MovePage::setSpherical (bool sph)
{
    spherical = sph;
    xyzBtn.setToggleState (! sph, dontSendNotification);
    sphBtn.setToggleState (sph, dontSendNotification);
    for (auto* k : { &kx, &ky, &kz }) k->setVisible (! sph);
    for (auto* k : { &kAz, &kEl, &kDist }) k->setVisible (sph);
    timerCallback();
}

void MovePage::applySpherical()
{
    const float az = degreesToRadians ((float) kAz.slider.getValue());
    const float el = degreesToRadians ((float) kEl.slider.getValue());
    const float d  = (float) kDist.slider.getValue();
    const float x = jlimit (-8.0f, 8.0f, d * std::cos (el) * std::sin (az));
    const float y = jlimit (-5.0f, 5.0f, d * std::sin (el));
    const float z = jlimit (-8.0f, 8.0f, d * std::cos (el) * std::cos (az));
    proc.setPositionNotifying (x, y, z);
}

void MovePage::timerCallback()
{
    if (appliedDesign != ui::design)
    {
        appliedDesign = ui::design;
        nameEditor.setColour (TextEditor::textColourId, pn::text);
        nameEditor.applyColourToAllText (pn::text, true);
        nameEditor.applyFontToAllText (ui::font (13.0f), true);
        nameEditor.setTextToShowWhenEmpty ("Track name (auto from Reaper)", pn::dim);
        nameEditor.repaint();
    }
    const bool sync = syncToggle.getToggleState();
    if (divBox.isVisible() != sync)
    {
        divBox.setVisible (sync);
        repaint();
    }

    if (! spherical)
        return;
    for (auto* k : { &kAz, &kEl, &kDist })
        if (k->slider.isMouseButtonDown())
            return;

    const float x = proc.apvts.getRawParameterValue ("x")->load();
    const float y = proc.apvts.getRawParameterValue ("y")->load();
    const float z = proc.apvts.getRawParameterValue ("z")->load();
    const float d = std::sqrt (x * x + y * y + z * z);
    updatingSph = true;
    kDist.slider.setValue (d, dontSendNotification);
    if (d > 0.05f)
    {
        kEl.slider.setValue (radiansToDegrees (std::asin (jlimit (-1.0f, 1.0f, y / d))), dontSendNotification);
        if (std::abs (x) + std::abs (z) > 0.02f)
            kAz.slider.setValue (radiansToDegrees (std::atan2 (x, z)), dontSendNotification);
    }
    updatingSph = false;
}

void MovePage::lookAndFeelChanged() { appliedDesign = -1; }

void MovePage::paint (Graphics& g) { paintSections (g, sections); }

void MovePage::resized()
{
    sections.clear();
    auto a = getLocalBounds();
    nameEditor.setBounds (a.removeFromTop (28));
    a.removeFromTop (10);

    {
        auto row = a.removeFromTop (26);
        xyzBtn.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        row.removeFromLeft (4);
        sphBtn.setBounds (row);
    }
    a.removeFromTop (8);

    sections.push_back ({ "POSITION", a.removeFromTop (18) });
    {
        auto row = a.removeFromTop (112);
        const int kw = row.getWidth() / 3;
        auto r1 = row.removeFromLeft (kw), r2 = row.removeFromLeft (kw), r3 = row;
        kx.setBounds (r1); ky.setBounds (r2); kz.setBounds (r3);
        kAz.setBounds (r1); kEl.setBounds (r2); kDist.setBounds (r3);
    }
    a.removeFromTop (4);

    sections.push_back ({ "PRESETS", a.removeFromTop (18) });
    const int gap = 6;
    const int bw = (a.getWidth() - 3 * gap) / 4;
    for (int r = 0; r < 2; ++r)
    {
        auto pr = a.removeFromTop (28);
        for (int c = 0; c < 4; ++c)
        {
            presets[r * 4 + c]->setBounds (pr.removeFromLeft (bw));
            pr.removeFromLeft (gap);
        }
        a.removeFromTop (6);
    }
    a.removeFromTop (4);

    sections.push_back ({ "MOTION AROUND YOU", a.removeFromTop (18) });
    motionBox.setBounds (a.removeFromTop (28));
    a.removeFromTop (6);
    {
        auto row = a.removeFromTop (104);
        const int kw = 86;
        kRate.setBounds (row.removeFromLeft (kw));
        kDepth.setBounds (row.removeFromLeft (kw));
        row.removeFromLeft (6);
        row.removeFromTop (14);
        syncToggle.setBounds (row.removeFromTop (26));
        row.removeFromTop (4);
        revToggle.setBounds (row.removeFromTop (26));
        row.removeFromTop (4);
        divBox.setBounds (row.removeFromTop (26));
    }
}

//==============================================================================
SoundPage::SoundPage (Orbit3DProcessor& p)
    : proc (p),
      kWidth (p.apvts, "width", "WIDTH", 3, false),
      kDist (p.apvts, "distance", "DISTANCE", 0, false),
      kAir (p.apvts, "air", "AIR", 1, false),
      kRoom (p.apvts, "room", "ROOM", 1, false),
      kSize (p.apvts, "roomSize", "SIZE", 0, false),
      kDamp (p.apvts, "damp", "DAMPING", 2, false),
      kBack (p.apvts, "backCue", "REAR CUE", 2, false),
      kHeight (p.apvts, "heightCue", "HEIGHT CUE", 3, false),
      kTrim (p.apvts, "trim", "TRIM", 0, true)
{
    for (auto* k : { &kWidth, &kDist, &kAir, &kRoom, &kSize, &kDamp, &kBack, &kHeight, &kTrim })
        addAndMakeVisible (*k);
    addAndMakeVisible (activeToggle);
    activeAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "active", activeToggle);
    startTimerHz (30);
}

void SoundPage::resized()
{
    sections.clear();
    auto a = getLocalBounds();
    auto row3 = [&] (Knob& k1, Knob& k2, Knob& k3)
    {
        auto row = a.removeFromTop (102);
        const int kw = row.getWidth() / 3;
        k1.setBounds (row.removeFromLeft (kw));
        k2.setBounds (row.removeFromLeft (kw));
        k3.setBounds (row);
    };
    sections.push_back ({ "SPACE", a.removeFromTop (18) });
    row3 (kWidth, kDist, kAir);
    sections.push_back ({ "ROOM REVERB", a.removeFromTop (18) });
    row3 (kRoom, kSize, kDamp);
    sections.push_back ({ "BINAURAL CUES + OUTPUT", a.removeFromTop (18) });
    row3 (kBack, kHeight, kTrim);
    a.removeFromTop (4);
    activeToggle.setBounds (a.removeFromTop (26));
    a.removeFromTop (8);
    readoutRect = a;
}

void SoundPage::paint (Graphics& g)
{
    paintSections (g, sections);
    auto area = readoutRect;
    if (area.getHeight() < 70) return;

    auto r = area.toFloat();
    paintCard (g, r);

    auto in = r.reduced (12.0f, 8.0f);
    const float x = proc.apvts.getRawParameterValue ("x")->load();
    const float y = proc.apvts.getRawParameterValue ("y")->load();
    const float z = proc.apvts.getRawParameterValue ("z")->load();
    const float d = std::sqrt (x * x + y * y + z * z);
    const float el = d > 0.001f ? radiansToDegrees (std::asin (jlimit (-1.0f, 1.0f, y / d))) : 0.0f;

    auto cols = in.removeFromTop (32.0f);
    const float cw = cols.getWidth() / 3.0f;
    const String labels[3] { "AZIMUTH", "ELEVATION", "DISTANCE" };
    const String values[3] { String (roundToInt (radiansToDegrees (std::atan2 (x, z)))) + kDeg,
                             String (roundToInt (el)) + kDeg, String (d, 1) + " m" };
    for (int i = 0; i < 3; ++i)
    {
        auto c = cols.removeFromLeft (cw);
        g.setColour (cardDim());
        g.setFont (ui::font (9.5f, true));
        g.drawText (labels[i], c.removeFromTop (12.0f).toNearestInt(), Justification::centredLeft);
        g.setColour (cardText());
        g.setFont (ui::font (15.0f, true));
        g.drawText (values[i], c.toNearestInt(), Justification::centredLeft);
    }
    in.removeFromTop (6.0f);

    auto bar = [&] (Rectangle<float> b, float lin, const String& name)
    {
        g.setColour (cardDim());
        g.setFont (ui::font (9.5f, true));
        g.drawText (name, b.removeFromLeft (14.0f).toNearestInt(), Justification::centredLeft);
        g.setColour (cardBar());
        g.fillRoundedRectangle (b, 2.5f);
        const float t = jlimit (0.0f, 1.0f, (toDb (lin) + 60.0f) / 60.0f);
        if (t > 0.01f)
        {
            g.setGradientFill (ColourGradient (pn::cyan, b.getX(), 0.0f, pn::pink, b.getRight(), 0.0f, false));
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * t), 2.5f);
        }
    };
    bar (in.removeFromTop (6.0f), proc.ownLevelL.load(), "L");
    in.removeFromTop (4.0f);
    bar (in.removeFromTop (6.0f), proc.ownLevelR.load(), "R");
    in.removeFromTop (8.0f);

    const float corr = proc.ownCorr.load();
    g.setColour (cardDim());
    g.setFont (ui::font (9.5f, true));
    g.drawText ("STEREO CORRELATION", in.removeFromTop (12.0f).toNearestInt(), Justification::centredLeft);
    auto cb = in.removeFromTop (8.0f);
    g.setColour (cardBar());
    g.fillRoundedRectangle (cb, 3.0f);
    g.setGradientFill (ColourGradient (pn::pink, cb.getX(), 0.0f, pn::cyan, cb.getRight(), 0.0f, false));
    g.setOpacity (0.45f);
    g.fillRoundedRectangle (cb, 3.0f);
    g.setColour (pn::border.brighter (0.5f));
    g.drawVerticalLine ((int) cb.getCentreX(), cb.getY() - 2.0f, cb.getBottom() + 2.0f);
    const float mx = cb.getX() + (corr + 1.0f) * 0.5f * cb.getWidth();
    g.setColour (cardText());
    g.fillEllipse (mx - 4.0f, cb.getCentreY() - 4.0f, 8.0f, 8.0f);
}

//==============================================================================
MasterPage::MasterPage (Orbit3DProcessor& p, SceneView& s) : proc (p), scene (s)
{
    auto& L = Look::get();
    orbitToggle.setToggleState (L.autoOrbit, dontSendNotification);
    trailToggle.setToggleState (L.showTrails, dontSendNotification);
    labelToggle.setToggleState (L.showLabels, dontSendNotification);
    orbitToggle.onClick = [this] { Look::get().autoOrbit = orbitToggle.getToggleState(); Look::changed(); };
    trailToggle.onClick = [this] { Look::get().showTrails = trailToggle.getToggleState(); Look::changed(); };
    labelToggle.onClick = [this] { Look::get().showLabels = labelToggle.getToggleState(); Look::changed(); };
    for (auto* t : { &orbitToggle, &trailToggle, &labelToggle })
        addAndMakeVisible (*t);
    startTimerHz (30);
}

void MasterPage::timerCallback()
{
    auto& L = Look::get();
    orbitToggle.setToggleState (L.autoOrbit, dontSendNotification);
    trailToggle.setToggleState (L.showTrails, dontSendNotification);
    labelToggle.setToggleState (L.showLabels, dontSendNotification);
    repaint();
}

void MasterPage::resized()
{
    auto a = getLocalBounds();
    headerRect = a.removeFromTop (18);
    auto toggles = a.removeFromBottom (3 * 28 + 6);
    orbitToggle.setBounds (toggles.removeFromTop (28));
    toggles.removeFromTop (3);
    trailToggle.setBounds (toggles.removeFromTop (28));
    toggles.removeFromTop (3);
    labelToggle.setBounds (toggles.removeFromTop (28));
    a.removeFromBottom (10);
    detailRect = a.removeFromBottom (112);
    a.removeFromBottom (10);
    listRect = a;
}

void MasterPage::mouseDown (const MouseEvent& e)
{
    if (! listRect.contains (e.getPosition()))
        return;
    const int row = (e.y - listRect.getY() + listScroll) / 30;
    int idx = 0;
    const auto& views = scene.getViews();
    for (int i = 0; i < Registry::maxSlots; ++i)
        if (views[(size_t) i].active)
        {
            if (idx == row)
            {
                scene.selectedSlot = (scene.selectedSlot == i) ? -1 : i;
                return;
            }
            ++idx;
        }
}

void MasterPage::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (listRect.contains (e.getPosition()))
        listScroll = jmax (0, listScroll - (int) (w.deltaY * 120.0f));
}

void MasterPage::paint (Graphics& g)
{
    paintSections (g, { { "TRACKS IN THE ROOM", headerRect } });
    const auto& views = scene.getViews();

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (listRect);
        int row = 0;
        for (int i = 0; i < Registry::maxSlots; ++i)
        {
            const auto& v = views[(size_t) i];
            if (! v.active) continue;
            const int y = listRect.getY() + row * 30 - listScroll;
            ++row;
            if (y + 28 < listRect.getY() || y > listRect.getBottom()) continue;

            Rectangle<float> rr ((float) listRect.getX(), (float) y, (float) listRect.getWidth(), 26.0f);
            const bool sel = i == scene.selectedSlot;
            paintRow (g, rr, sel);

            g.setColour (v.trackColour.withAlpha (v.stale ? 0.4f : 1.0f));
            g.fillEllipse (rr.getX() + 9.0f, rr.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (v.stale ? pn::dim : pn::text);
            g.setFont (ui::font (12.5f));
            g.drawText (v.name, Rectangle<float> (rr.getX() + 26.0f, rr.getY(), rr.getWidth() - 92.0f, rr.getHeight()).toNearestInt(),
                        Justification::centredLeft, true);

            Rectangle<float> lb (rr.getRight() - 58.0f, rr.getCentreY() - 3.0f, 48.0f, 6.0f);
            g.setColour (pn::panel);
            g.fillRoundedRectangle (lb, 3.0f);
            if (v.lvl > 0.01f)
            {
                g.setColour (spectrumColourFor (v));
                g.fillRoundedRectangle (lb.withWidth (lb.getWidth() * v.lvl), 3.0f);
            }
        }
        if (row == 0)
        {
            g.setColour (pn::dim);
            g.setFont (ui::font (12.0f));
            g.drawFittedText ("No tracks yet.\n\nInsert Orbit 3D on your tracks (Track mode) and they will appear here, floating in the room.",
                              listRect.reduced (10), Justification::centredTop, 6);
        }
    }

    auto r = detailRect.toFloat();
    paintCard (g, r);

    if (scene.selectedSlot < 0 || ! views[(size_t) scene.selectedSlot].active)
    {
        g.setColour (cardDim());
        g.setFont (ui::font (12.0f));
        g.drawFittedText ("Click an orb or a row to inspect a track.\nDrag an orb to move that track in 3D.",
                          detailRect.reduced (14), Justification::centred, 4);
        return;
    }

    const auto& v = views[(size_t) scene.selectedSlot];
    auto in = r.reduced (12.0f, 10.0f);
    g.setColour (cardText());
    g.setFont (ui::font (13.0f, true));
    g.drawText (v.name, in.removeFromTop (18.0f).toNearestInt(), Justification::centredLeft, true);
    g.setColour (cardDim());
    g.setFont (ui::font (11.5f));
    g.drawText ("x " + String (v.pos.x, 1) + "   y " + String (v.pos.y, 1) + "   z " + String (v.pos.z, 1)
                    + "   (" + String (v.pos.length(), 1) + " m)",
                in.removeFromTop (16.0f).toNearestInt(), Justification::centredLeft, true);
    in.removeFromTop (8.0f);

    const float sum = jmax (1.0e-6f, v.low * 0.7f + v.mid * 1.8f + v.high * 5.0f);
    const float shares[3] { v.low * 0.7f / sum, v.mid * 1.8f / sum, v.high * 5.0f / sum };
    const Colour cols[3] { pn::pink, pn::amber, pn::cyan };
    const char* names[3] { "LOW", "MID", "HIGH" };
    for (int i = 0; i < 3; ++i)
    {
        auto line = in.removeFromTop (10.0f);
        g.setColour (cardDim());
        g.setFont (ui::font (9.5f, true));
        g.drawText (names[i], line.removeFromLeft (30.0f).toNearestInt(), Justification::centredLeft);
        g.setColour (cardBar());
        g.fillRoundedRectangle (line, 3.0f);
        g.setColour (cols[i]);
        g.fillRoundedRectangle (line.withWidth (jmax (2.0f, line.getWidth() * shares[i])), 3.0f);
        in.removeFromTop (5.0f);
    }
}

//==============================================================================
namespace
{
struct SectionHeader final : Component
{
    explicit SectionHeader (String t) : title (std::move (t)) { setInterceptsMouseClicks (false, false); }
    void paint (Graphics& g) override { paintSections (g, { { title, getLocalBounds().withTrimmedTop (6) } }); }
    String title;
};

struct LabeledCombo final : Component
{
    LabeledCombo (String t, StringArray items, int* target, std::function<void()> extra = {})
        : title (std::move (t)), value (target)
    {
        box.addItemList (items, 1);
        box.setSelectedId (*value + 1, dontSendNotification);
        box.onChange = [this, extra] { *value = box.getSelectedId() - 1; Look::changed(); if (extra) extra(); };
        addAndMakeVisible (box);
    }
    void refresh() { box.setSelectedId (*value + 1, dontSendNotification); }
    void paint (Graphics& g) override
    {
        g.setColour (pn::dim);
        g.setFont (ui::font (10.5f, true));
        g.drawText (title, getLocalBounds().removeFromTop (14), Justification::centredLeft);
    }
    void resized() override { box.setBounds (getLocalBounds().withTrimmedTop (16).withHeight (26)); }
    String title; ComboBox box; int* value;
};

struct LabeledSlider final : Component
{
    LabeledSlider (String t, float* target, double lo, double hi, double step, int accentIdx)
        : title (std::move (t)), value (target)
    {
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setTextBoxStyle (Slider::TextBoxRight, false, 50, 20);
        s.setRange (lo, hi, step);
        s.setValue (*value, dontSendNotification);
        s.setDoubleClickReturnValue (true, *value);
        s.getProperties().set ("accent", accentIdx);
        s.onValueChange = [this] { *value = (float) s.getValue(); Look::touch(); };
        s.onDragEnd = [] { Look::flush(); };
        addAndMakeVisible (s);
        lookAndFeelChanged();
    }
    void lookAndFeelChanged() override
    {
        s.setColour (Slider::textBoxTextColourId, pn::text);
        s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        s.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    }
    void refresh() { s.setValue (*value, dontSendNotification); }
    void paint (Graphics& g) override
    {
        g.setColour (pn::dim);
        g.setFont (ui::font (10.5f, true));
        g.drawText (title, getLocalBounds().removeFromTop (14), Justification::centredLeft);
    }
    void resized() override { s.setBounds (getLocalBounds().withTrimmedTop (14)); }
    String title; Slider s; float* value;
};

struct LabeledToggle final : Component
{
    LabeledToggle (String t, bool* target) : value (target)
    {
        b.setButtonText (t);
        b.setToggleState (*value, dontSendNotification);
        b.onClick = [this] { *value = b.getToggleState(); Look::changed(); };
        addAndMakeVisible (b);
    }
    void refresh() { b.setToggleState (*value, dontSendNotification); }
    void resized() override { b.setBounds (getLocalBounds()); }
    ToggleButton b; bool* value;
};

struct DesignGrid final : Component
{
    static constexpr int cols = 2, gap = 8, ch = 66;
    explicit DesignGrid (std::function<void()> cb) : onChange (std::move (cb)) {}

    Rectangle<int> cell (int i) const
    {
        const int cw = (getWidth() - gap * (cols - 1)) / cols;
        return { (i % cols) * (cw + gap), (i / cols) * (ch + gap), cw, ch };
    }
    int cellAt (Point<int> p) const
    {
        for (int i = 0; i < (int) ui::NumDesigns; ++i) if (cell (i).contains (p)) return i;
        return -1;
    }
    static int heightFor() { return 3 * ch + 2 * gap; }

    void paint (Graphics& g) override
    {
        const int cur = Look::get().design;
        for (int i = 0; i < (int) ui::NumDesigns; ++i)
        {
            const auto& d = ui::designs()[(size_t) i];
            const auto r = cell (i).toFloat();
            const Colour bg0 (d.scene.bg0), bg1 (d.scene.bg1), txt (d.scene.text), dim (d.scene.dim);
            const Colour a1 (d.scene.a1), a3 (d.scene.a3), brd (d.scene.border);
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addRoundedRectangle (r, i == 0 ? 8.0f : (i == 1 ? 3.0f : 0.0f));
            g.reduceClipRegion (clip);
            g.setGradientFill (ColourGradient::vertical (bg1, r.getY(), bg0, r.getBottom()));
            g.fillRect (r);

            // a tiny motif in the design's own drawing style
            const Point<float> m (r.getRight() - 30.0f, r.getCentreY());
            switch (i)
            {
            case ui::Classic:
                g.setColour (a1.withAlpha (0.25f)); g.fillEllipse (m.x - 15, m.y - 15, 30, 30);
                g.setColour (a1); g.fillEllipse (m.x - 6, m.y - 6, 12, 12); break;
            case ui::Vintage:
                g.setColour (Colour (0xffb8a46a)); g.fillEllipse (m.x - 15, m.y - 15, 30, 30);
                g.setColour (Colour (0xff2a1d10)); g.fillEllipse (m.x - 12, m.y - 12, 24, 24);
                g.setColour (a1); g.fillEllipse (m.x - 6, m.y - 6, 12, 12); break;
            case ui::Blueprint:
                g.setColour (txt); g.drawEllipse (m.x - 14, m.y - 14, 28, 28, 1.0f);
                g.drawLine (m.x - 18, m.y, m.x + 18, m.y, 1.0f); g.drawLine (m.x, m.y - 18, m.x, m.y + 18, 1.0f); break;
            case ui::Studio:
                g.setColour (a1); g.fillEllipse (m.x - 5, m.y - 5, 10, 10);
                g.setColour (brd); g.drawEllipse (m.x - 14, m.y - 14, 28, 28, 1.0f); break;
            case ui::Swiss:
                g.setColour (txt); g.fillRect (m.x - 14, m.y - 14, 28.0f, 28.0f);
                g.setColour (a1); g.fillEllipse (m.x - 7, m.y - 7, 14, 14); break;
            default:
                g.setColour (brd); g.drawEllipse (m.x - 15, m.y - 15, 30, 30, 1.0f); g.drawEllipse (m.x - 7, m.y - 7, 14, 14, 1.0f);
                g.setColour (a1); g.fillEllipse (m.x + 4, m.y - 8, 4, 4); break;
            }

            auto fnt = [&] (float h, bool bold)
            {
                const char* fam = d.sans;
                const int st = bold ? Font::bold : Font::plain;
                return fam[0] ? Font (FontOptions (String (fam), h, st)) : Font (FontOptions (h, st));
            };
            auto tr = r.reduced (9.0f, 7.0f);
            g.setColour (txt);
            g.setFont (fnt (12.5f, true));
            g.drawText (d.name, tr.removeFromTop (17.0f).withTrimmedRight (50.0f).toNearestInt(), Justification::centredLeft, true);
            g.setColour (dim);
            g.setFont (fnt (9.5f, false));
            g.drawFittedText (d.blurb, tr.withTrimmedRight (56.0f).toNearestInt(), Justification::topLeft, 3, 0.9f);
        }
        for (int i = 0; i < (int) ui::NumDesigns; ++i)
        {
            const auto r = cell (i).toFloat();
            const float cr = i == 0 ? 8.0f : (i == 1 ? 3.0f : 0.0f);
            g.setColour (i == cur ? pn::cyan : (i == hover ? pn::dim : pn::border));
            g.drawRoundedRectangle (r.reduced (1.0f), cr, i == cur ? 2.5f : 1.0f);
        }
    }
    void mouseMove (const MouseEvent& e) override { const int h = cellAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int i = cellAt (e.getPosition());
        if (i >= 0 && i != Look::get().design)
        {
            Look::get().applyDesignDefaults (i);
            ui::applyDesign (i);
            Look::themeChanged();
            onChange();
        }
    }
    int hover = -1;
    std::function<void()> onChange;
};
}

struct LookPage::Impl
{
    std::vector<std::unique_ptr<Component>> comps;
    std::vector<int> heights;
    std::vector<std::function<void()>> refreshers;
    TextButton resetBtn { "Reset look to defaults" };

    template <typename T> T* add (std::unique_ptr<T> c, int h)
    {
        auto* raw = c.get();
        comps.push_back (std::move (c));
        heights.push_back (h);
        return raw;
    }
};

LookPage::LookPage (std::function<void()> onThemeChanged) : impl (new Impl)
{
    auto& L = Look::get();
    auto& I = *impl;
    auto addC = [&] (Component* c) { addAndMakeVisible (c); };

    auto section = [&] (const String& t) { addC (I.add (std::make_unique<SectionHeader> (t), 26)); };
    auto combo = [&] (const String& t, StringArray items, int* v)
    {
        auto* c = I.add (std::make_unique<LabeledCombo> (t, items, v), 46);
        I.refreshers.push_back ([c] { c->refresh(); });
        addC (c);
    };
    auto slider = [&] (const String& t, float* v, double lo, double hi, double step, int acc)
    {
        auto* c = I.add (std::make_unique<LabeledSlider> (t, v, lo, hi, step, acc), 38);
        I.refreshers.push_back ([c] { c->refresh(); });
        addC (c);
    };
    auto toggle = [&] (const String& t, bool* v)
    {
        auto* c = I.add (std::make_unique<LabeledToggle> (t, v), 28);
        I.refreshers.push_back ([c] { c->refresh(); });
        addC (c);
    };

    section ("DESIGN");
    {
        auto* tg = I.add (std::make_unique<DesignGrid> (onThemeChanged), DesignGrid::heightFor() + 6);
        addC (tg);
    }

    section ("ORBS");
    combo ("Orb style", { "Glow", "Sphere", "Ring", "Pulse", "Lamp", "Target", "Flat", "Blip" }, &L.orbStyle);
    combo ("Orb colour", { "Spectrum (bass / mid / high)", "Track colour", "Design accents" }, &L.colourMode);
    slider ("Orb size", &L.orbSize, 0.5, 2.0, 0.01, 0);
    slider ("Glow", &L.glow, 0.0, 2.0, 0.01, 3);
    slider ("Trail length", &L.trailLen, 0.0, 2.0, 0.01, 1);
    slider ("Label size", &L.labelSize, 0.8, 1.8, 0.01, 2);

    section ("ROOM");
    combo ("Background (Classic)", { "Gradient", "Solid", "Stars", "Nebula" }, &L.background);
    combo ("Floor", { "Grid", "Dots", "Polar", "None" }, &L.floorStyle);
    slider ("Grid brightness", &L.gridBrightness, 0.0, 2.5, 0.01, 0);
    slider ("Perspective", &L.perspective, 0.6, 1.8, 0.01, 1);
    toggle ("Room walls", &L.showWalls);
    toggle ("Distance rings", &L.showRings);

    section ("SCENE");
    combo ("Listener", { "Head", "Dot", "Headphones" }, &L.headStyle);
    toggle ("Show listener", &L.showHead);
    toggle ("Motion trails", &L.showTrails);
    toggle ("Track names", &L.showLabels);
    toggle ("Stereo spread markers", &L.showSpread);
    toggle ("Hint text", &L.showHud);
    toggle ("Auto-orbit camera", &L.autoOrbit);
    slider ("Auto-orbit speed", &L.orbitSpeed, 0.2, 4.0, 0.01, 2);

    section ("MASTER SPECTRUM");
    toggle ("Show spectrum", &L.showSpectrum);
    combo ("Spectrum style", { "Filled", "Line", "Bars" }, &L.spectrumStyle);

    I.resetBtn.onClick = [this, onThemeChanged]
    {
        {
            auto& cur = Look::get();
            const int dsg = cur.design;
            cur = LookSettings();
            cur.applyDesignDefaults (dsg);
            ui::applyDesign (dsg);
        }
        Look::themeChanged();
        for (auto& r : impl->refreshers) r();
        onThemeChanged();
        repaint();
    };
    I.heights.push_back (44);
    addC (&I.resetBtn);
}

LookPage::~LookPage() = default;

int LookPage::getContentHeight() const
{
    int h = 0;
    for (int x : impl->heights) h += x + 4;
    return h + 8;
}

void LookPage::resized()
{
    int y = 0;
    const int w = getWidth();
    for (size_t i = 0; i < impl->comps.size(); ++i)
    {
        impl->comps[i]->setBounds (0, y, w, impl->heights[i]);
        y += impl->heights[i] + 4;
    }
    impl->resetBtn.setBounds (0, y + 6, w, 32);
}

//==============================================================================
ControlPanel::ControlPanel (Orbit3DProcessor& p, SceneView& s, std::function<void()> onThemeChanged)
    : proc (p), scene (s),
      modeAttach (*p.apvts.getParameter ("mode"), [this] (float v) { applyMode (v > 0.5f); }),
      move (p), sound (p), masterPage (p, s), look (std::move (onThemeChanged))
{
    for (auto* b : { &trackBtn, &masterBtn })
    {
        b->setRadioGroupId (4711);
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    trackBtn.onClick  = [this] { modeAttach.setValueAsCompleteGesture (0.0f); };
    masterBtn.onClick = [this] { modeAttach.setValueAsCompleteGesture (1.0f); };

    for (size_t i = 0; i < tabBtns.size(); ++i)
    {
        tabBtns[i].setClickingTogglesState (false);
        tabBtns[i].setRadioGroupId (4713);
        tabBtns[i].onClick = [this, i] { showPage ((int) i); };
        addAndMakeVisible (tabBtns[i]);
    }

    addChildComponent (move);
    addChildComponent (sound);
    addChildComponent (masterPage);
    lookViewport.setViewedComponent (&look, false);
    lookViewport.setScrollBarsShown (true, false);
    lookViewport.setScrollBarThickness (8);
    addChildComponent (lookViewport);

    modeAttach.sendInitialUpdate();
}

void ControlPanel::applyMode (bool m)
{
    master = m;
    page = 0;
    trackBtn.setToggleState (! m, dontSendNotification);
    masterBtn.setToggleState (m, dontSendNotification);
    updateTabs();
    resized();
    repaint();
}

void ControlPanel::refreshMode() { applyMode (proc.isMasterMode()); }

void ControlPanel::showPage (int index)
{
    page = jlimit (0, master ? 1 : 2, index);
    updateTabs();
}

void ControlPanel::updateTabs()
{
    static const char* trackNames[3] { "MOVE", "SOUND", "LOOK" };
    static const char* masterNames[2] { "ROOM", "LOOK" };
    const int n = master ? 2 : 3;
    for (int i = 0; i < 3; ++i)
    {
        tabBtns[(size_t) i].setVisible (i < n);
        if (i < n)
        {
            tabBtns[(size_t) i].setButtonText (master ? masterNames[i] : trackNames[i]);
            tabBtns[(size_t) i].setToggleState (i == page, dontSendNotification);
        }
    }
    const bool isLook = page == n - 1;
    move.setVisible (! master && page == 0);
    sound.setVisible (! master && page == 1);
    masterPage.setVisible (master && page == 0);
    lookViewport.setVisible (isLook);
    resized();
}

void ControlPanel::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (60);

    auto modeRow = area.removeFromTop (34);
    trackBtn.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2 - 2));
    modeRow.removeFromLeft (4);
    masterBtn.setBounds (modeRow);
    area.removeFromTop (10);

    {
        const int n = master ? 2 : 3;
        auto row = area.removeFromTop (28);
        const int gap = 4;
        const int w = (row.getWidth() - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
        {
            tabBtns[(size_t) i].setBounds (row.removeFromLeft (w));
            row.removeFromLeft (gap);
        }
    }
    area.removeFromTop (12);

    move.setBounds (area);
    sound.setBounds (area);
    masterPage.setBounds (area);
    lookViewport.setBounds (area.expanded (4, 0).withTrimmedBottom (0));
    look.setSize (lookViewport.getWidth() - 14, look.getContentHeight());
}

void ControlPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const String sub = master ? "Spatial mix analyser" : "Binaural 3D positioner";
    g.fillAll (pn::panel);

    switch (ui::design)
    {
    case ui::Vintage:
    {
        g.setGradientFill (ColourGradient::vertical (pn::bg1, 0.0f, pn::bg0, b.getBottom()));
        g.fillAll();
        // fine brushed grain
        Random rnd (7);
        for (int i = 0; i < 260; ++i)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.035f));
            const float yy = rnd.nextFloat() * b.getHeight();
            g.drawHorizontalLine ((int) yy, rnd.nextFloat() * b.getWidth() * 0.5f, b.getWidth());
        }
        // walnut cheek on the right edge
        const auto wood = b.removeFromRight (12.0f);
        g.setGradientFill (ColourGradient (Colour (0xff4a2c17), wood.getX(), 0.0f, Colour (0xff2a170a), wood.getRight(), 0.0f, false));
        g.fillRect (wood);
        for (int i = 0; i < 40; ++i)
        {
            g.setColour (Colour (0xff1a0e06).withAlpha (0.35f));
            const float yy = (float) i * b.getHeight() / 40.0f;
            g.drawLine (wood.getX(), yy, wood.getRight(), yy + 14.0f, 0.8f);
        }
        g.setColour (Colour (0xff8c7f5e)); g.drawVerticalLine (0, 0.0f, getHeight() * 1.0f);
        // brass nameplate
        Rectangle<float> plate (14.0f, 12.0f, b.getWidth() - 28.0f, 44.0f);
        g.setGradientFill (ColourGradient (Colour (0xffd9c67f), plate.getX(), plate.getY(), Colour (0xff9c8845), plate.getRight(), plate.getBottom(), false));
        g.fillRoundedRectangle (plate, 3.0f);
        g.setColour (Colour (0xff5b4a1c)); g.drawRoundedRectangle (plate, 3.0f, 1.2f);
        for (auto p : { plate.getTopLeft() + Point<float> (6, 6), plate.getTopRight() + Point<float> (-6, 6),
                        plate.getBottomLeft() + Point<float> (6, -6), plate.getBottomRight() + Point<float> (-6, -6) })
        {
            g.setColour (Colour (0xff5b4a1c)); g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
            g.setColour (Colour (0xffe9d99a)); g.drawLine (p.x - 1.6f, p.y + 1.0f, p.x + 1.6f, p.y - 1.0f, 0.8f);
        }
        g.setColour (Colour (0xff2a1f0a));
        g.setFont (Font (FontOptions (String ("Georgia"), 24.0f, Font::bold | Font::italic)));
        g.drawText ("Orbit 3D", plate.withTrimmedBottom (14.0f).toNearestInt(), Justification::centred);
        g.setFont (ui::font (9.5f));
        g.drawText (sub.toUpperCase() + "  " + String::fromUTF8 ("\xc2\xb7") + "  MODEL O3", plate.withTrimmedTop (30.0f).toNearestInt(), Justification::centred);
        break;
    }
    case ui::Blueprint:
    {
        g.fillAll (pn::bg0);
        g.setColour (pn::border.withAlpha (0.28f));
        for (float x = 0; x < b.getWidth(); x += 16.0f) g.drawVerticalLine ((int) x, 0.0f, b.getHeight());
        for (float y = 0; y < b.getHeight(); y += 16.0f) g.drawHorizontalLine ((int) y, 0.0f, b.getWidth());
        g.setColour (pn::text); g.drawVerticalLine (0, 0.0f, b.getHeight());
        Rectangle<float> tb (12.0f, 10.0f, b.getWidth() - 24.0f, 46.0f);
        g.setColour (pn::bg0); g.fillRect (tb);
        g.setColour (pn::text); g.drawRect (tb, 1.2f);
        g.drawVerticalLine ((int) (tb.getRight() - 78.0f), tb.getY(), tb.getBottom());
        g.setFont (ui::font (20.0f, true));
        g.drawText ("ORBIT 3D", tb.withTrimmedRight (80.0f).withTrimmedBottom (14.0f).reduced (8.0f, 0.0f).toNearestInt(), Justification::centredLeft);
        g.setFont (ui::font (9.5f)); g.setColour (pn::dim);
        g.drawText (sub.toUpperCase(), tb.withTrimmedRight (80.0f).withTrimmedTop (28.0f).reduced (8.0f, 0.0f).toNearestInt(), Justification::centredLeft);
        g.setColour (pn::text);
        g.drawText ("DWG 3D-001", tb.removeFromRight (78.0f).withTrimmedBottom (23.0f).toNearestInt(), Justification::centred);
        g.drawText ("REV  A", tb.removeFromRight (78.0f).withTrimmedTop (23.0f).toNearestInt(), Justification::centred);
        break;
    }
    case ui::Studio:
    {
        g.fillAll (pn::bg0);
        g.setColour (pn::border); g.drawVerticalLine (0, 0.0f, b.getHeight());
        g.drawHorizontalLine (58, 14.0f, b.getWidth() - 14.0f);
        g.setColour (pn::cyan);
        g.drawEllipse (16.0f, 20.0f, 20.0f, 20.0f, 1.6f);
        g.fillEllipse (30.0f, 21.0f, 5.0f, 5.0f);
        g.setColour (pn::text);
        g.setFont (ui::font (19.0f));
        g.drawText ("Orbit 3D", Rectangle<int> (46, 14, 160, 24), Justification::centredLeft);
        g.setFont (ui::font (10.5f)); g.setColour (pn::dim);
        g.drawText (sub, Rectangle<int> (47, 37, 220, 14), Justification::centredLeft);
        break;
    }
    case ui::Swiss:
    {
        g.fillAll (pn::bg0);
        g.setColour (pn::border);
        g.fillRect (0.0f, 0.0f, 3.0f, b.getHeight());
        g.fillRect (16.0f, 58.0f, b.getWidth() - 32.0f, 3.0f);
        g.setColour (pn::pink); g.fillRect (16.0f, 14.0f, 14.0f, 14.0f);
        g.setColour (pn::text);
        g.setFont (ui::font (28.0f, true));
        g.drawText ("Orbit 3D", Rectangle<int> (38, 8, 240, 32), Justification::centredLeft);
        g.setFont (ui::font (11.0f, true));
        g.drawText (sub, Rectangle<int> (16, 38, 280, 16), Justification::centredLeft);
        break;
    }
    case ui::Radar:
    {
        g.fillAll (pn::bg0);
        g.setColour (pn::border); g.drawVerticalLine (0, 0.0f, b.getHeight());
        g.setColour (pn::border.withAlpha (0.35f));
        for (float y = 0; y < b.getHeight(); y += 4.0f) g.drawHorizontalLine ((int) y, 1.0f, b.getWidth());
        const Point<float> c (36.0f, 36.0f);
        g.setColour (pn::cyan.withAlpha (0.6f));
        g.drawEllipse (c.x - 18, c.y - 18, 36, 36, 1.2f); g.drawEllipse (c.x - 9, c.y - 9, 18, 18, 1.0f);
        g.drawLine (c.x - 20, c.y, c.x + 20, c.y, 0.8f); g.drawLine (c.x, c.y - 20, c.x, c.y + 20, 0.8f);
        g.setColour (pn::cyan); g.fillEllipse (c.x + 6, c.y - 10, 4, 4);
        g.setFont (ui::mono (19.0f, true));
        g.drawText ("ORBIT-3D", Rectangle<int> (64, 14, 200, 24), Justification::centredLeft);
        g.setFont (ui::mono (10.0f)); g.setColour (pn::dim);
        g.drawText ("// " + sub.toUpperCase(), Rectangle<int> (65, 39, 240, 14), Justification::centredLeft);
        break;
    }
    default:
    {
        g.setColour (pn::border);
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
        const float cx = 36.0f, cy = 38.0f;
        ColourGradient glow (pn::cyan.withAlpha (0.5f), cx, cy, Colours::transparentBlack, cx + 26.0f, cy, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - 26.0f, cy - 26.0f, 52.0f, 52.0f);
        g.setGradientFill (ColourGradient (pn::cyan.brighter (0.6f), cx - 4.0f, cy - 5.0f, pn::violet.darker (0.4f), cx + 9.0f, cy + 9.0f, true));
        g.fillEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
        g.setColour (pn::violet.withAlpha (0.7f));
        g.drawEllipse (cx - 17.0f, cy - 6.0f, 34.0f, 12.0f, 1.2f);
        g.setFont (ui::font (24.0f, true));
        g.setColour (pn::text);
        g.drawText ("ORBIT", Rectangle<int> (66, 16, 100, 28), Justification::centredLeft);
        g.setColour (pn::cyan);
        g.drawText ("3D", Rectangle<int> (140, 16, 50, 28), Justification::centredLeft);
        g.setFont (ui::font (11.0f));
        g.setColour (pn::dim);
        g.drawText (sub, Rectangle<int> (67, 41, 200, 14), Justification::centredLeft);
        break;
    }
    }
}

//==============================================================================
Orbit3DEditor::Orbit3DEditor (Orbit3DProcessor& p)
    : AudioProcessorEditor (p), proc (p), scene (p),
      side (p, scene, [this] { themeChanged(); })
{
    Look::get();
    laf.refresh();
    setLookAndFeel (&laf);
    addAndMakeVisible (scene);
    addAndMakeVisible (side);
    setResizable (true, true);
    setResizeLimits (980, 720, 1900, 1200);
    setSize (1180, 760);
    lastVersion = Look::themeVersion();
    startTimerHz (10);
}

Orbit3DEditor::~Orbit3DEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Orbit3DEditor::themeChanged()
{
    laf.refresh();
    sendLookAndFeelChange();
    repaint();
}

void Orbit3DEditor::timerCallback()
{
    if (lastVersion != Look::themeVersion())
    {
        lastVersion = Look::themeVersion();
        ui::applyDesign (Look::get().design);
        themeChanged();
    }
    if (! Desktop::getInstance().getMainMouseSource().isDragging())
        Look::flush();
}

void Orbit3DEditor::paint (Graphics& g) { g.fillAll (pn::bg0); }

void Orbit3DEditor::resized()
{
    auto b = getLocalBounds();
    side.setBounds (b.removeFromRight (320));
    scene.setBounds (b);
}
