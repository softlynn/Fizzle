# Changelog

## v0.1.8 - Stability + Appearance Settings Pass

### Fixed
- Hardened audio device reconfigure flow to reduce launch and buffer-size change crashes.
- Diagnostics now report the active runtime buffer size from the audio callback (not only requested size).
- Smoothed window dragging by skipping heavy UI refresh work while the app is actively dragged.
- Refined tray popup window flags/background behavior to reduce Windows border artifacts.

### Added
- Appearance settings now persist and apply live:
  - Light/Dark mode
  - Aqua/Salmon theme variants
  - Background opacity (Transparent/Opaque)
  - UI size (Small, Normal, Large)

### Changed
- Header lane spacing and divider animation placement were tuned for cleaner integration with the main body.
- Subtle border/glow balancing and logo visibility polish across themes.

## v0.1.7 - Header Integration + Tray State Clarity

### Fixed
- Stabilized VST reorder behavior by applying direct row swaps and keeping the dragged row above peers during drag.
- Prevented accidental VST reorders while touching row controls (wet/dry knob, edit button, enable checkbox).
- Tray menu now uses custom popup background drawing for the full menu surface, removing the residual bright border artifact.
- Monitor setup now retries with default device settings if preferred sample rate/buffer setup fails.
- Listen monitor queue now trims stale backlog frames to reduce drift when monitoring load spikes.

### Changed
- Reworked the top header to read as one continuous surface with the main body (no detached look).
- Updated header branding to lowercase `fizzle` with clearer logo treatment and stronger icon visibility.
- Increased shell padding and tightened spacing/margins for more uniform alignment.
- Tray menu now shows explicit stateful toggles: `Effects: On/Off` and `Mute`.
- In-app mute state is now persistent and obvious (badge + hint), instead of transient text.
- Added a restart-audio loading overlay so restart actions provide immediate visual feedback.

## v0.1.6 - Tray Control + Visual Stability Pass

### Fixed
- Tray menu actions now route through the main UI logic so `Enable/Bypass`, `Mute`, `Restart Audio`, and preset selection reliably apply in-app and in audio behavior.
- Reduced gradient color banding in both app surfaces and tray menu backgrounds using subtle dithering/noise overlays.
- Settings overlay dimming is now clipped to the rounded app shell (no dark overspill to the window border).
- Improved top-right window controls with cleaner capsule styling and stronger hover/press feedback.

### Changed
- Rebalanced warm accent usage to better match the existing blue glass theme and border treatment.

## v0.1.5 - In-Depth Header Integration

### Fixed
- Replaced window titlebar rendering with an in-app integrated header layer.
- Added custom in-header window controls (min/max/close) with drag and double-click maximize behavior.
- Repositioned animated header line into a dedicated visible header lane.
- Removed residual tray popup white border by forcing opaque full-background painting.

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
