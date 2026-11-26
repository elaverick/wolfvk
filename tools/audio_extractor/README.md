# WL6 Audio Extractor

`tools/audio_extractor` is a standalone C17 tool for converting the original Wolfenstein 3D WL6 sound effects into OGG Vorbis assets that WolfVK can load.

It supports both original effect formats:

- Digitised PCM effects from the final VSWAP DigiList page. These are 8-bit unsigned mono samples at 7,000 Hz, as used by the original SoundBlaster path.
- AdLib effects from the 87 original sound chunks in `AUDIOHED.WL6` / `AUDIOT.WL6`. The tool renders these through the pinned C11 Opal OPL3 emulator to 16-bit mono PCM at 44,100 Hz.

The extractor writes the 87 named WL6 sound effects. Where a sound has an original digitised recording, that recording is written after the synthesised version and takes precedence. This preserves the original DigiMap aliases while also supplying the effects that exist only in the AdLib bank.

## Build

A C17 compiler, CMake 3.24 or newer and Git are required. CMake fetches the pinned Opal OPL emulator and the pinned Unity test framework.

Windows:

```powershell
cmake -S tools/audio_extractor -B build-audio -A x64 -DBUILD_TESTING=ON
cmake --build build-audio --config Release --parallel
ctest --test-dir build-audio -C Release --output-on-failure
```

Linux:

```sh
cmake -S tools/audio_extractor -B build-audio -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-audio --parallel
ctest --test-dir build-audio --output-on-failure
```

## Extract

Install FFmpeg with its `libvorbis` encoder and make the `ffmpeg` executable available in `PATH`. Run the extractor from the directory containing your own retail WL6 files:

```powershell
build-audio\Release\wolf_audio_extractor.exe --vswap VSWAP.WL6 --audiohed AUDIOHED.WL6 --audiot AUDIOT.WL6 --output assets\sounds
```

The tool writes named files such as `ATKPISTOLSND.ogg`, `GETAMMOSND.ogg`, `HEALTH2SND.ogg`, `OPENDOORSND.ogg`, `PUSHWALLSND.ogg` and `DONOTHINGSND.ogg` into `assets/sounds`. The game loads the original 87 sound names from this directory. `--audiohed` and `--audiot` must be supplied together; they are required to produce AdLib-only effects. `--ffmpeg <path>` overrides the FFmpeg executable and `--output <directory>` changes the destination.

The command reads `VSWAP.WL6`, `AUDIOHED.WL6` and `AUDIOT.WL6`, and writes derived OGG files. It never modifies the original WL6 files. If you omit the audio archive arguments, it retains the original digitised-only extraction behaviour.

## Current scope

The 87 AdLib effects and the digitised effects listed in the original DigiMap are converted. Digitised samples take precedence over the matching AdLib-synthesised effect. The 27 original AdLib music tracks are not yet converted; they use the separate music sequencer format and remain outside this effects extractor. PC-speaker-only playback is also not separately emulated: WolfVK uses the AdLib sound bank as its synthesised fallback.

The standalone test harness covers VSWAP digitised-page reconstruction, malformed audio archives, original DigiMap aliases, AdLib chunk parsing and OPL rendering.
