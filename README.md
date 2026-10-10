# AutoDeafen

A Windows Geode mod that sends a configured Discord **Toggle Deafen** hotkey when a classic level reaches a selected percentage, then sends the same key again when the attempt ends.

## Status

Initial preview. Compiled for Geometry Dash **2.2081** and Geode **5.11.1**. Loading through Geode and automated state-machine tests have been verified. End-to-end Discord voice behavior and full visual QA have **not yet been verified**. This is not an official update to Lynxdeer's AutoDeafen.

The implementation was authored with OpenAI Codex. This is disclosed for review, including Geode's rules regarding predominantly AI-generated submissions. Publication on GitHub does not mean acceptance into the Geode index.

## Features

- Per-level opt-in and decimal thresholds from 1 to 99.9 percent.
- Graphite and purple interface with Russian labels and a small optional HUD.
- Restore on death, restart, completion, exit, and optionally pause.
- Optional operation in practice mode.
- Selectable F6–F12 hotkey; F8 by default.
- No Discord account tokens, OAuth application, external backend, or network requests.

## Setup

1. Install the `.geode` release asset in the game's `geode/mods` folder.
2. In desktop Discord, open Settings → Keybinds → Add a Keybind.
3. Select **Toggle Deafen** and record **F8**. Leave the Discord settings page.
4. Turn off other AutoDeafen mods and mod-menu features.
5. Start a classic level, pause, and click the **headphone icon** in the upper right.
6. Set the threshold and enable the level. Test twice before playing in voice.

Hotkeys cannot read Discord's actual deafen state. Start with voice enabled and avoid manually changing the deafen state during an attempt. If either program runs elevated while the other does not, Windows may block input. Unexpected process termination cannot guarantee restoration.

## Building

Use Geode SDK **v5.11.1**, Geode CLI, CMake 3.25+, an MSVC compiler with C++23 support, and the Windows SDK. Set `GEODE_SDK` to the SDK directory and install its Windows prebuilt binaries.

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo
```

The standalone state-machine tests require only a C++20 compiler:

```powershell
cl /EHsc /std:c++20 tests/engine.cpp /Fe:engine-tests.exe
./engine-tests.exe
```

Tests cover decimal thresholds, one toggle per run, death/end, pause/resume, disabling, starts beyond the threshold, non-finite progress, failed injection, and failed restoration.

## Attribution

Geode: https://github.com/geode-sdk/geode

AutoDeafen is an existing feature and category, including Lynxdeer's mod: https://github.com/Lynxdeer/AutoDeafen . AutoDeafen is an independent implementation; it does not contain that repository's source code and is not endorsed by its author.
