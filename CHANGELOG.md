# Changelog

## v0.1.4 - Header/Tray Fix Follow-up

### Fixed
- Removed remaining content/titlebar separation by forcing zero content border.
- Moved animated top line to a clean visible layer above controls.
- Further softened shell border transparency and top seam contrast.
- Eliminated residual white tray menu border with zero popup border + full background paint.

## v0.1.3 - Header and Transparency Pass

### Changed
- Stronger header/body integration so the window reads as one continuous surface.
- Removed the hard title/content seam and softened top blend/divider treatment.
- Reduced main shell border opacity for a cleaner transparent edge feel.
- Refined tray menu border transparency and corner blending.

## v0.1.2 - Auto-Install and Startup Controls

### Added
- New `Startup` tab in Settings.
- `Start with Windows` option.
- `Start minimized to tray` option.
- `Open/close window with Program Auto-Enable` option.

### Changed
- Update flow now supports automatic silent install (not only download).
- Faster settings panel fade.
- Cleaner tray menu styling with rounded visuals and improved typography.
- More obvious window controls on hover/press.
- Header/body visual integration polish.
- Updated docs/site copy for Discord/LIVE support and automatic updates.

## v0.1.1 - UI and Updates Polish

### Added
- Dedicated `Updates` tab inside Settings.
- Quick links to GitHub repo and website in both Settings and About dialog.
- Subtle version watermark in the bottom-right of the main window.

### Changed
- Cohesive custom top header with embedded Fizzle logo.
- Salmon-tinted window control buttons (minimize, maximize, close).
- Faster settings overlay fade animation.
- Click-fade accent on `Save Preset` and `About` buttons.
- Virtual mic branding updates (`Fizzle Mic` naming + Fizzle icon treatment).

## v0.1.0 - First Public Release

### Added
- Windows installer with Start Menu + Desktop shortcut support.
- Tray-first microphone effects workflow.
- Real-time mic routing to a selectable virtual mic output.
- VST3 plugin scanning, loading, enable/disable, wet/dry mix, and reordering.
- Preset save/load/delete flow.
- Diagnostics panel with sample rate, buffer, latency, CPU, and drop counters.
- First-launch in-app quick guide.
- Optional auto-download updates in Settings.
- Refreshed README and GitHub Pages website with install/use instructions.
