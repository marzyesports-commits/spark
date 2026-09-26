# ✦ Spark

Spark is a sound-mutation instrument and effect. Drop in any sound, play it from MIDI as a cloud of grains or as a wavetable, then hit **SPARK** to roll new variations. Keep the ones you love and breed them together.

It comes as two plugins built from one codebase:

| Plugin | Type | What it does |
|---|---|---|
| **Spark** | Instrument (MIDI in) | Plays a dropped sample as grains or as a wavetable sliced from it |
| **Spark FX** | Audio effect | Live granular resynthesis of a track, with tempo-synced stutter |

Formats: **VST3**, **Audio Unit** and **standalone app** on macOS (universal: Apple Silicon and Intel, macOS 11+). VST3 and standalone also build on Windows and Linux.

## Presets

Spark ships with **103 instrument presets** and **75 effect presets**, sorted into categories. Each preset notes what kind of sample it suits. Click the preset name at the top to open the browser. There you can pick a category, search by name or by sample type (try "808", "vocal" or "breaks"), use the arrow keys to audition, or press **Surprise me** for a random preset.

| Spark (instrument) | Spark FX (effect) |
|---|---|
| Starters · **Bass** (808s, subs, reese, growls) · **Pads** · Keys & Plucks · Leads · **Vocal Chops** · Textures · **Drums & Perc** · FX & Risers · Wavetable | Starters · Subtle Polish · Shimmer & Space · Rhythmic Stutter · Glitch & Chaos · Pitch & Harmony · Freeze & Drone · Lo-fi & Dark · **Vocal FX** · **Drum Bus** · **Bass Tools** |

- **Level-matched.** Presets are level-matched so switching doesn't jump in volume, and Spark has a transparent safety clipper on its output.
- **Presets act on your sound.** They shape whatever sound is loaded; they don't load a sample. Drop in the kind of sound the preset's note suggests.
- **Your own presets.** **Save** stores your own presets under **User**, as `.sparkpreset` files in `Documents/Spark/Presets`. They're easy to back up or share. Right-click a user preset to reveal or delete it.
- **Locks still apply.** Locked facets stay put when you browse presets.

## Using Spark

- **The core.** The gold ring is your sound. Around it are eight **facets**. Drag an arc up or down to change a facet (hold Shift for fine control, double-click to reset).
- **SPARK** rolls a new variation. **Mutate** sets how many facets move; **Chaos** sets how far they move.
- **Locks.** Lock a facet in the list on the right and Spark, Breed and preset changes leave it alone.
- **Lineage.** Every variation is saved along the bottom. Click one to go back to it. **Keep** stars the current one. **Breed** crosses the current variation with your most recent kept one. Right-click a variation to keep or un-keep it. The lineage is saved with your project.
- **Source (instrument).** Drag a WAV, AIFF or FLAC onto Spark, or use Import. Spark switches between:
  - **Grain**: plays the sound as overlapping grains (Position, Grain, Motion).
  - **Table**: plays a 64-frame wavetable sliced from the sound, with pitch-detected single cycles (Morph, Motion).
- **Wavetables.**
  - **Import.** Wavetable WAVs from Serum, Serum 2 or Vital are detected automatically, either by their `clm` marker or by being an exact multiple of 2048 samples.
  - **Make table** slices whatever sound is loaded.
  - **Export** writes a 2048-samples-per-frame WAV with the current **Drive** and **Tone** baked in. Serum and Vital load it as a wavetable. Exports go to `Documents/Spark/Wavetables`.
- **About Serum presets (.fxp / .SerumPreset).** These files are settings for Serum's own engine, not audio, so Spark can't play them. To bring a Serum sound in, either load its wavetable WAV or bounce a note from Serum and drop the audio into Spark.
- **Shape (envelopes).** Every note has two envelopes, each with Attack, Hold, Decay, Sustain and Release:
  - **Amp** sets the volume of each note.
  - **Tone** sweeps the Tone filter on each note. Set **Amount** to make it open (positive) or close (negative), for plucks, wows and acid squelches.

  How to edit them:
  - **Points.** Drag a point to set a time or level.
  - **Curves.** Drag the small circle in the middle of a slope to bend its curve: *punchy* for snappy hits, *swell* for slow blooms.
  - **Velocity.** This sets how much playing harder makes notes louder, or deepens the tone sweep.
  - **Big editor.** Every number can be dragged (Shift for fine moves, double-click to reset). The ⤢ button opens a large editor with both envelopes side by side.
- **Spark FX.**
  - **Freeze** stops listening and keeps playing grains from what's already captured.
  - **Capture to sample** saves the last four seconds of input to `Music/Spark/Captures` so you can drag it into the Spark instrument.

## Installing on a Mac

1. Download `Spark-<version>-macOS.pkg` from the latest build (Actions → Build macOS → Artifacts) or from Releases.
2. The installer isn't signed with an Apple Developer ID yet, so macOS will block a normal double-click. **Right-click the .pkg → Open → Open**, or go to System Settings → Privacy & Security and click **Open Anyway**.
3. In Ableton Live, open Settings → Plug-Ins, turn on **Use VST3 Plug-in System Folders** (and **Use Audio Units** if you want the AU), then click **Rescan**.

## Building from source

Requirements: CMake 3.22+, a C++20 compiler (Xcode 15+ on macOS). JUCE 8.0.10 is downloaded automatically.

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Spark_All SparkFX_All
bash packaging/macos/make-installer.sh 1.0.0      # → dist/Spark-1.0.0-macOS.pkg
```

Tests: configure with `-DSPARK_BUILD_TESTS=ON`, build `SparkTests` and run it. It renders every preset, checks the randomiser, locks, lineage and state saving, round-trips wavetables, runs the effect, and saves UI snapshots.

### Continuous builds

`.github/workflows/build-macos.yml` builds on every push to `main`. Each run:

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
