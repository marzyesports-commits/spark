# ✦ Spark

Spark is a sound-mutation instrument. Drop in any sound, play it from MIDI as a cloud of grains, as a wavetable or as the sample itself, then hit **SPARK** to roll new variations. Keep the ones you love and breed them together. **Shapeshift** rebuilds a synth note you bounced from Serum, Serum 2 or Vital, and the **FX** page holds a full effects rack.

Formats: **VST3**, **Audio Unit** and **standalone app** on macOS (universal: Apple Silicon and Intel, macOS 11+), and **VST3** and **standalone app** on Windows 10/11 (64-bit).

## Presets

Spark ships with **118 presets** and a **library of 82 sounds**. Presets are sorted into categories. Each preset notes what kind of sample it suits. Click the preset name at the top to open the browser. There you can pick a category, search by name or by sample type (try "808", "vocal" or "breaks"), use the arrow keys to audition, or press **Surprise me** for a random preset.

Starters · **Bass** (808s, subs, reese, growls) · **Pads** · Keys & Plucks · Leads · **Vocal Chops** · Textures · **Drums & Perc** · FX & Risers · Wavetable · **Motion** (presets built around LFOs, macros, glide and layers)

- **The sound library.** Every preset brings its own sound from Spark's library: basses and 808s, pads, keys, leads, synthesised vowel voices, textures, drum hits and loops, FX, and Serum-style wavetables. Every sound was synthesised from scratch for Spark (`tools/factory_sounds/make_sounds.py`), so it's all royalty-free. Browse them with **Sounds** in the SOURCE panel.
- **Keep your own sound.** The lock under the waveform decides whether presets bring their sound or keep the one you have. Dropping in (or picking) a sound locks it, so presets then shape *your* sound; unlock to let presets bring theirs. Projects that use a library sound save just its name.

- **Level-matched.** Presets are level-matched so switching doesn't jump in volume, and Spark has a transparent safety clipper on its output.
- **Your own presets.** **Save** stores your own presets under **User**, as `.sparkpreset` files in `Documents/Spark/Presets`. They're easy to back up or share. Right-click a user preset to reveal or delete it.
- **Locks still apply.** Locked facets stay put when you browse presets.
- **Presets reset the FX rack** to its defaults (only the Space reverb on), so each preset sounds the same every time. Use **Chain** on the FX page to add effects back.

## Using Spark

- **The core.** The gold ring is your sound. Around it are eight **facets**. Drag an arc up or down to change a facet (hold Shift for fine control, double-click to reset).
- **SPARK** rolls a new variation. **Mutate** sets how many facets move; **Chaos** sets how far they move.
- **Locks.** Lock a facet in the list on the right and Spark, Breed and preset changes leave it alone.
- **Lineage.** Every variation is saved along the bottom. Click one to go back to it. **Keep** stars the current one. **Breed** crosses the current variation with your most recent kept one. Right-click a variation to keep or un-keep it. The lineage is saved with your project.
- **Source.** Drag a WAV, AIFF or FLAC onto Spark, or use Import. Spark detects the sound's pitch, so it plays in tune across the keyboard (the detected note is shown under the waveform). Switch between:
  - **Grain**: plays the sound as overlapping grains (Position, Grain, Motion).
  - **Table**: plays a 64-frame wavetable sliced from the sound, with pitch-detected single cycles (Morph, Motion).
  - **Sample**: plays the sound itself from the start, repitched per key.
- **Motion** also spreads the wavetable voice in stereo: the centre stays solid and the unison detune goes to the sides.
- **Wavetables.**
  - **Import.** Wavetable WAVs from Serum, Serum 2 or Vital are detected automatically, either by their `clm` marker or by being an exact multiple of 2048 samples.
  - **Make table** slices whatever sound is loaded.
  - **Export** writes a 2048-samples-per-frame WAV with the current **Drive** and **Tone** baked in. Serum and Vital load it as a wavetable. Exports go to `Documents/Spark/Wavetables`.
- **Shapeshift.** Bounce one held note from Serum, Serum 2, Vital or any synth (a few seconds, including the release) and press **Shapeshift**. Spark:
  - finds the note,
  - rebuilds the sound as a 64-frame wavetable that follows how its tone changes over time (a filter sweep becomes a table scan),
  - matches the amp envelope (attack, decay, sustain, release and their curves) and the stereo width.

  The result is an ordinary Spark sound, so every facet, envelope and effect can reshape it. **TABLE** plays the rebuild; **SAMPLE** plays the original bounce. It works best on pitched sounds; noise layers and pitch bends inside the note come out simplified.
- **About Serum presets (.fxp / .SerumPreset).** These files are settings for Serum's own engine, not audio, so Spark can't play them. Bounce a note and Shapeshift it, or load the preset's wavetable WAV.
- **Undo and redo.** The arrows left of the preset name undo and redo any edit on any page: knobs, Spark, Breed, presets, modulation and the loaded sound. A whole drag is one step, and the tooltip names what will be undone. Cmd/Ctrl+Z and Shift+Cmd+Z / Ctrl+Y work too when Spark has keyboard focus.
- **Shape (envelopes).** Every note has two envelopes, each with Delay, Attack, Hold, Decay, Sustain (with a slope) and Release:
  - **Amp** sets the volume of each note.
  - **Tone** sweeps the Tone filter on each note. Set **Amount** to make it open (positive) or close (negative), for plucks, wows and acid squelches.

  How to edit them:
  - **Points.** Drag a point to set a time or level.
  - **Curves.** Drag the small circle in the middle of a slope to bend its curve: *punchy* for snappy hits, *swell* for slow blooms.
  - **Delay and sustain slope.** Delay waits before the attack. The diamond on the sustain tilts it: down fades a held note away (like a piano), up swells it.
  - **Playhead.** While you play, a glowing dot runs along each envelope for every sounding note.
  - **Exact values.** Double-click any number to type a value, like `250 ms`, `1.2 s`, `70%`, `punchy 40` or `fade 30`. Alt-click resets it.
  - **Velocity.** This sets how much playing harder makes notes louder, or deepens the tone sweep.
  - **Big editor.** Every number can be dragged (Shift for fine moves, double-click to reset). The ⤢ button opens a large editor with both envelopes side by side.
- **SYNTH page.** Click **SYNTH** at the top.
  - **Filter:** low-pass, high-pass, band-pass or notch, with resonance and key tracking. Cutoff is the Tone facet.
  - **Layers:** a sine **sub** (its **Pitch** knob sets how far below the note it plays, any interval down to 3 octaves) and a **noise** layer with a colour control from dark rumble to bright hiss. Both run through the filter and envelopes with the rest of the note.
  - **Play:** **Poly**, **Mono** (each key restarts the envelopes) or **Legato** (overlapping keys slide without restarting), plus **Glide**, pitch-bend range and velocity.
  - **Modulation:** two **LFOs** (7 shapes, free or synced to tempo, shared or restarting on each note), four **macros**, and the mod wheel, aftertouch and velocity. The **mod matrix** has 8 slots, each routing a source to a facet, resonance or volume.
  - **Assigning modulation:** drag an LFO's or macro's handle onto a facet on the ring, a facet in the list, or the CUTOFF or RES knob. Hover over the **SOUND** tab while dragging to get to the facets. Or click **+ add** in the matrix. A modulated facet shows a white marker that moves with it.
  - **Spark and Breed** also vary the modulation, macros and layers you're using. The lock on the matrix keeps them off the modulation.
- **Sparks and lightning.** Moving a facet throws sparks off its arc and fires lightning into the core; SPARK, Breed and preset changes set off a burst. It animates only while something moves, so it costs nothing when the sound is still.
- **FX page.** Click **FX** at the top. Seven effects run in order: **Distortion** → **EQ** → **Chorus** → **Grains** (a pitched grain cloud) → **Stutter** (tempo-synced) → **Delay** (ping-pong, tempo-synced) → **Reverb**.
  - Each effect has an ON/OFF switch and a lock. Effects that are on light up gold; off ones go dark. The strip across the top shows the whole chain at a glance: click any effect there to switch it. **SPARK** and **Breed** also vary the effects that are on and unlocked.
  - **SPARK FX** rolls only the effects. **Chain** loads a ready-made rack. **ALL OFF** clears everything except the reverb.
  - The Space facet sets the reverb amount, so it still works from the main page.

Spark 1.3 retires the separate Spark FX plugin; its sound lives on in the FX page. If you installed an earlier version, the old Spark FX stays installed, so older projects that use it still open.

## Installing on a Mac

1. Download `Spark-<version>-macOS.pkg` from the latest build (Actions → Build macOS → Artifacts) or from Releases.
2. The installer isn't signed with an Apple Developer ID yet, so macOS will block a normal double-click. **Right-click the .pkg → Open → Open**, or go to System Settings → Privacy & Security and click **Open Anyway**.
3. In Ableton Live, open Settings → Plug-Ins, turn on **Use VST3 Plug-in System Folders** (and **Use Audio Units** if you want the AU), then click **Rescan**.

## Installing on Windows

1. Download `Spark-<version>-Windows-Setup.exe` from Releases.
2. The installer isn't code-signed yet, so SmartScreen may say "Windows protected your PC". Click **More info**, then **Run anyway**.
3. The plugin installs to `C:\Program Files\Common Files\VST3`. Rescan plug-ins in your DAW. In Ableton Live, go to Settings → Plug-Ins, turn on **Use VST3 Plug-in System Folders**, then click **Rescan**.

## Projects keep their sound

When you save a project, Spark stores the sample you dropped in *inside the project*, as lossless FLAC. Sessions open with the right sound on any computer, even if the original file has moved or been deleted. Samples are capped at 60 seconds. Projects that use the built-in sound stay tiny.

## Building from source

Requirements: CMake 3.22+, a C++20 compiler (Xcode 15+ on macOS). JUCE 8.0.10 is downloaded automatically.

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Spark_All
bash packaging/macos/make-installer.sh 1.0.0      # → dist/Spark-1.0.0-macOS.pkg
```

Tests: configure with `-DSPARK_BUILD_TESTS=ON`, build `SparkTests` and run it. It renders every preset, checks the randomiser, locks, lineage and state saving, round-trips wavetables, checks every effect and Shapeshift, and saves UI snapshots.

### Continuous builds

`.github/workflows/build.yml` builds Mac and Windows on every push to `main`. Each run:

1. Builds universal binaries.
2. Packages the installer and a plain zip of the plugins.
3. Runs the tests, Apple's `auval` and `pluginval`.
4. Uploads the installer as a build artifact.

Pushing a tag like `v1.0.1` also publishes a GitHub Release.

### Signing and notarisation (optional)

To remove the "unidentified developer" warning you need an Apple Developer account (US$99/year). Export your *Developer ID Application* and *Developer ID Installer* certificates as .p12 files and add these repository secrets:

| Secret | Value |
|---|---|
| `DEVELOPER_ID_APP_P12` | base64 of the Application .p12 |
| `DEVELOPER_ID_INSTALLER_P12` | base64 of the Installer .p12 |
| `P12_PASSWORD` | password for both .p12 files |
| `APP_SIGN_IDENTITY` | e.g. `Developer ID Application: Your Name (TEAMID)` |
| `INSTALLER_SIGN_IDENTITY` | e.g. `Developer ID Installer: Your Name (TEAMID)` |
| `NOTARY_APPLE_ID`, `NOTARY_TEAM_ID`, `NOTARY_PASSWORD` | Apple ID, team ID and an app-specific password |

The workflow then turns on the hardened runtime, signs everything and notarises the installer.

## Licences

- Spark is built with [JUCE](https://juce.com). JUCE is dual-licensed (AGPLv3 or a commercial JUCE licence). If you plan to sell or distribute Spark as closed source, check JUCE's current licence tiers first.
- Fonts: Syne (© The Syne Project Authors), Manrope (© The Manrope Project Authors) and JetBrains Mono (© The JetBrains Mono Project Authors), all under the SIL Open Font License 1.1 (`Resources/Fonts/OFL.txt`).
