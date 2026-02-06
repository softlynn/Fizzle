# Fizzle v0.1.7

## Highlights
- Added more buffer-size options for latency/CPU tuning: `64`, `96`, `128`, `192`, `256`, `384`, `512`, `1024`.
- Improved buffer-value persistence in UI/presets so custom sizes stay selected correctly.
- Listen mode now better identifies Bluetooth earbuds/headsets when auto-picking a monitor output.
- Listen monitor setup now prefers the active processing sample rate and buffer size when supported.
- Reduced monitor lag buildup by trimming stale queued listen audio when callback backlog occurs.

## Notes
- Bluetooth earbuds on Windows still have codec/transport latency that cannot be fully removed in-app.
- This release reduces avoidable app-side monitoring delay, but it does not eliminate Bluetooth baseline latency.

## Installer
- `Fizzle-Setup-0.1.7.exe`
