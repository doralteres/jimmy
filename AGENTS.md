# Project Guidelines

## Overview

Jimmy is a **JUCE-based VST3 instrument plugin** (C++17) that acts as a live teleprompter for Cubase 12+. It displays lyrics and chords synced to the DAW timeline. It produces **no audio output** — purely a data/display tool.

See [PROJECT.md](PROJECT.md) for the original design brief and [README.md](README.md) for architecture.

## Tech Stack

- **C++17**, **JUCE 8** (git submodule at `libs/JUCE/`)
- **CMake 3.22+** build system
- VST3 format only, targeting **macOS (arm64) + Windows**

## Architecture

```
PluginProcessor (audio thread)  →  SharedState (lock-free atomics)  →  PluginEditor (UI thread, 30Hz timer)
                                                                        ├── Edit Mode: LyricsEditor, SectionManager
                                                                        └── Live Mode: TeleprompterView
SongModel (mutex-guarded) ← XML serialize/deserialize for state persistence
```

**Critical constraint**: The audio thread (`processBlock`) must **never** allocate memory, lock mutexes, or do anything that could block. Use only `std::atomic` operations for audio↔UI communication via `SharedState`.

## Code Style

- All source files live in `src/` — flat structure, no nested directories
- Header-only for small components (`ChordParser.h`, `SongModel.h`, `TeleprompterView.h`, etc.)
- `.h/.cpp` split for `PluginProcessor` and `PluginEditor` (JUCE convention)
- Use JUCE 8 APIs — `juce::Font(juce::FontOptions(...))` not the deprecated `juce::Font(float)` constructor
- Use `juce::Colour(0xffRRGGBB)` hex literals for colors; centralize in `Theme.h`
- Prefix class members with no prefix (no `m_`); use `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on components

## Build and Test

```bash
# First time setup
git submodule update --init --recursive

# Build (macOS)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(sysctl -n hw.ncpu)

# Output
build/Jimmy_artefacts/Release/VST3/Jimmy.vst3

# Install for testing
cp -R build/Jimmy_artefacts/Release/VST3/Jimmy.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

After modifying source files, always rebuild and verify zero warnings from `src/` files.

## Conventions

- **Thread safety**: Audio thread writes to `SharedState` atomics. UI thread reads them. `SongModel` has its own mutex for UI-thread access — never access `SongModel` from the audio thread.
- **State persistence**: All user data (lyrics, sections, chords) is serialized as XML via `SongModel::toXml()`/`fromXml()`, stored in VST3 state block (saved with Cubase project).
- **RTL/Hebrew support**: Use `TeleprompterView::isRtlText()` for text direction detection. Lyrics can be in Hebrew — always handle Unicode properly.
- **No DSP**: `processBlock` clears output buffers. The plugin reads MIDI and transport only.
- **MIDI chord detection**: Chord track MIDI → `ChordParser::identify()` via pitch-class interval matching.
- **Documentation**: When making user-facing changes (new features, changed behavior, new UI elements), update the relevant docs (`docs/USAGE.md`, in-plugin help text in `PluginEditor.cpp::showHelpPopup()`, and `docs/INSTALL.md` if setup steps change). Keep docs and code in sync.

## Documentation

- [docs/BUILD.md](docs/BUILD.md) — Build instructions (macOS + Windows)
- [docs/INSTALL.md](docs/INSTALL.md) — Plugin installation into Cubase
- [docs/USAGE.md](docs/USAGE.md) — End-user guide
- [docs/CI_WORKFLOWS.md](docs/CI_WORKFLOWS.md) — CI/CD pipeline details

## CI

PR builds run on macOS via GitHub Actions. Release workflow auto-bumps version and creates GitHub Releases with `.vst3` artifacts. See [docs/CI_WORKFLOWS.md](docs/CI_WORKFLOWS.md).
