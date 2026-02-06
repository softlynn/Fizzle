## Fizzle v0.1.7

This update focuses on tray control clarity, header integration, and UI behavior polish.

### Highlights
- Header/body visuals are now unified so the top bar no longer feels detached.
- Tray menu now shows explicit toggle state for `Effects: On/Off` and `Mute`.
- Mute status is now obvious in-app (persistent badge + hint) instead of briefly flashing.
- Restart Audio now shows a loading/status overlay for clearer feedback.
- Settings overlay now closes when clicking outside the settings panel.
- VST row dragging is cleaner:
  - dragged row stays in front,
  - row controls no longer trigger accidental drag,
  - reorder operation is more stable.
- Tray menu border/transparency rendering has been cleaned up.

### Audio Reliability
- Monitor setup now retries with default settings if preferred monitor setup fails.
- Monitor queue trims stale buffered audio under backlog conditions to reduce listen drift.
