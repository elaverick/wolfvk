# WL6 Audio Extractor

`tools/audio_extractor` is a standalone C17 tool for extracting the digitised sound effects stored in the original Wolfenstein 3D WL6 `VSWAP.WL6` archive.

It reads the final VSWAP DigiList page, follows each sample's page and byte-length entry, concatenates the original unsigned 8-bit mono PCM data, and encodes a named OGG Vorbis file for every original `DigiMap` event. Reused samples are exported under each of their original sound names. The source sample rate is 7,000 Hz, matching the original Wolf3D SoundBlaster driver.

## Build

A C17 compiler, CMake 3.24 or newer and Git are required. The tests use the pinned Unity framework fetched by CMake.

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

Install FFmpeg with its `libvorbis` encoder and make the `ffmpeg` executable available in `PATH`. Then, from the game directory containing your own WL6 files, run:

```powershell
build-audio\Release\wolf_audio_extractor.exe --vswap VSWAP.WL6 --output assets\sounds
```

The tool writes files such as `ATKPISTOLSND.ogg`, `ATKMACHINEGUNSND.ogg` and `YEAHSND.ogg` to `assets/sounds`. You can override the FFmpeg executable with `--ffmpeg <path>` and the destination with `--output <directory>`.

The extractor does not copy or modify any original game files. It only reads `VSWAP.WL6` and writes derived audio to the chosen output directory.

## Current scope

This first extractor covers the digitised PCM effects listed by the original WL6 `DigiMap`. The remaining sound IDs use the original PC-speaker or AdLib synthesis data in `AUDIOHED.WL6` / `AUDIOT.WL6`. The 27 music entries also use the original AdLib sequencer format. Those synthesised sound effects and music tracks are not converted by this tool yet; they must be rendered through an OPL-compatible synthesiser before being encoded as OGG. The engine's separate `assets/music` directory is reserved for those tracks.
