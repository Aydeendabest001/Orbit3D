# Orbit 3D  (VST3, Windows / Reaper)

Binaural 3D positioner + 3D mix analyser in one plugin.

## Install
1. Push this folder to a GitHub repo. `.github/workflows/build.yml` builds on every push (or run it from the Actions tab).
2. Download the **Orbit-3D-VST3** artifact, unzip, copy `Orbit 3D.vst3` to `C:\Program Files\Common Files\VST3\`.
3. Reaper: Options > Preferences > Plug-ins > VST > Re-scan.

## Track mode (insert on every track you want in the room)
* **MOVE tab** - drag the orb in the 3D room (Shift = height), use X/Y/Z knobs or switch to Azimuth / Elevation / Distance knobs, or hit a preset (Front, Back, Left, Right, Above, Below, Inside, Far).
* **Motion around you** - Circle, Pendulum, Figure-8 or Elevator, with rate, depth, reverse and tempo-sync (locks to Reaper's timeline). The path is previewed in the room.
* **SOUND tab** - Width, Distance, Air, Room reverb (amount / size / damping), Rear cue, Height cue, Trim, plus live az/el/dist, L/R meters and correlation.
* All position and motion controls are automatable.

## Master mode (last plugin on the Master / main bus, switch to MASTER)
* Every Track-mode instance floats in the 3D room: size = loudness, colour = bass / mid / high balance, stereo-width markers, motion trails.
* Click or drag orbs to inspect / reposition tracks. Live master spectrum + meters.
* Camera: drag to orbit, wheel to zoom, double-click to reset, quick buttons 3/4, TOP, SIDE, BEHIND.

## LOOK tab (shared by all instances, remembered between sessions)
Six **designs** (LOOK > DESIGN). Each one changes the whole interface (knobs, buttons, panels, fonts, the 3D room, orbs, labels, spectrum), not just colours:

* **Classic Blue** - the original glowing-orb look.
* **Vintage** - ivory faceplate, brass nameplate, bakelite knobs with numbered scales, pilot-lamp switches, walnut frame, paper-tag labels, amber vector lines.
* **Blueprint** - technical drawing: blueprint grid, dimension lines (16.00 m), callout labels, hatched spectrum, title block.
* **Studio Dark** - flat, quiet, hairlines, no glow.
* **Swiss Light** - bold grotesque type, hard black edges, black label boxes.
* **Radar** - phosphor scope with a rotating sweep, bearing ticks, LED knobs, scanlines.

Also: 8 orb styles (Glow, Sphere, Ring, Pulse, Lamp, Target, Flat, Blip), orb colour mode, floors, listener style, perspective, glow, trail and label size, spectrum style and more. Settings file: `%APPDATA%\Orbit3D\look.xml`.

Use headphones. The master view sees plugins running in the same Reaper process (the default).

## v4 notes
* **Top end fixed.** Earlier versions low-passed even a centred source (~5 kHz), used a low-pass interpolator in the delay lines and always applied a pinna notch. All three are gone: at factory settings a centred source is flat to 16 kHz (+2 dB presence at 3-4 kHz). Rear / far / side positions still darken the sound on purpose.
* **Mid/side rendering.** The mono (centre) content is placed at ONE point, only the side content is spread by Width, so mono material no longer comb-filters.
* **Atmosphere.** Room reverb now has distance-based pre-delay (4-30 ms), a high-passed diffuse tail, and a wet level that grows with distance. SOUND tab > ATMOSPHERE has one-click presets (Dry, Close, Studio, Hall, Cave). Tip: put distant instruments (pads, backing vocals) farther away and raise Room; keep lead instruments close for clarity.
* Default level is unity at the default 2.5 m position; closer = louder, farther = quieter (Distance knob).
* Vintage is now the default design, with analogue VU meters on the SOUND tab. An old look.xml is ignored once.
