# Fizzle

![Fizzle Logo](resources/icon/app.png)

Fizzle is a tray-first Windows microphone processing app for VRChat, Discord, and Codex voice input. It captures your mic, runs a realtime VST3 chain, and routes to a virtual mic device with low latency.

## Highlights

- Windows desktop app built with C++ and JUCE.
- Tray-first workflow with quick menu actions.
- WASAPI shared mode (no exclusive device lock).
- VST3 chain with enable/disable, wet/dry, editor open, drag reorder.
- Presets stored as JSON.
- App settings and logs in `%APPDATA%\Fizzle`.
- Inno Setup installer with Start Menu integration.
- Unit tests for DSP basics and Windows CI build.

## Repository Layout

- `CMakeLists.txt` - root build configuration.
- `cmake/` - compiler/version helper modules.
- `src/` - app, UI, audio engine, VST host.
- `tests/` - unit tests.
- `installer/` - Inno Setup script and packaging files.
- `resources/icon/` - app logo/icon assets.
- `.github/workflows/windows-build.yml` - CI workflow.

## Build (Windows 11, Visual Studio 2022)

```powershell
cmake -S . -B build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-x64 --config Release --target Fizzle
cmake --build build-x64 --config Release --target FizzleTests
ctest --test-dir build-x64 -C Release --output-on-failure
```

Shortcut script:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_release.ps1
```

Binary:

- `build-x64/Fizzle_artefacts/Release/Fizzle.exe`

## Run

```powershell
.\build-x64\Fizzle_artefacts\Release\Fizzle.exe --show
```

## Build Installer

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_installer.ps1
```

Or compile directly with Inno Setup:

```powershell
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\Fizzle.iss
```

Output:

- `installer/out/Fizzle-Setup-<version>.exe`

## Icon Pipeline

Primary icon files:

- `resources/icon/app.png`
- `resources/icon/app.ico`

Regenerate icon assets from a source image:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\generate_logo_assets.ps1 -SourceImage "C:\path\to\logo.png"
```

These assets are used for:

- executable/window icon
- tray icon
- installer icon
- Start Menu and desktop shortcut icon

## Usage

1. Select your lav receiver in `Input Mic`.
2. Set `Virtual Mic Output` to `CABLE Input (VB-Audio Virtual Cable)`.
3. In VRChat/Codex select `CABLE Output` as microphone.
4. Keep buffer at `256` for best latency/stability balance.

## Diagnostics

The diagnostics panel reports:

- selected input/output devices
- sample rate
- buffer size
- CPU load
- estimated latency
- dropped buffers

## Troubleshooting

- No sound:
  - verify input/output selection
  - confirm VB-Cable installation
  - run `Test Tone`
- Crackles:
  - raise buffer size to `512`
  - close heavy background apps
- High latency:
  - use `256` or `128` if stable
  - prefer 48 kHz device config in Windows

## Versioning

Semantic versioning (`MAJOR.MINOR.PATCH`) is defined in `CMakeLists.txt`:

- `project(Fizzle VERSION x.y.z)`

## CI

GitHub Actions builds and tests on Windows:

- `.github/workflows/windows-build.yml`

## Publish Checklist

- [x] Build `Release` successfully
- [x] Run unit tests
- [x] Build installer
- [x] Verify icon assets in `resources/icon/`
- [x] Verify README and license present

## License

MIT (`LICENSE`)
