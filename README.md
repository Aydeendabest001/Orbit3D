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
12 themes (Nebula, Aurora, Sunset, Synthwave, Matrix, Ember, Ocean, Graphite, Blueprint, Gold, Ice, Sakura), 4 orb styles
(Glow, Sphere, Ring, Pulse), orb colour mode, backgrounds (Gradient, Solid, Stars, Nebula), floors (Grid, Dots, Polar, None),
listener style, perspective, glow, trail and label size, spectrum style and more. Settings file: `%APPDATA%\Orbit3D\look.xml`.

Use headphones. The master view sees plugins running in the same Reaper process (the default).
