# Fizzle v0.1.5

In-depth header architecture fix and tray border cleanup.

## Highlights
- Reworked header integration by removing external titlebar rendering.
- Added integrated top header controls directly in-app:
  - Minimize
  - Maximize/Restore
  - Close to tray
- Added drag-to-move and double-click maximize from the integrated header.
- Moved animated header line to a dedicated visible lane so it no longer hides behind controls.
- Eliminated white tray popup border artifact with opaque full-surface popup painting.

## Notes
- This release focuses on structural UI integration and tray visual correctness.
