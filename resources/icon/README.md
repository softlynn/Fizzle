# Icon Assets

Current generated assets:

- `app.png` - high-res source icon used for future exports.
- `app.ico` - Windows app/installer icon.

Regenerate from a source image:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/generate_logo_assets.ps1 -SourceImage "C:\path\to\logo.png"
```

Or replace with your own final brand assets:

- `app.ico` (required) used by:
  - executable icon
  - window icon
  - tray icon
  - installer icon
  - Start Menu shortcut icon

Recommended source export sizes in the `.ico`: 16, 24, 32, 48, 64, 128, 256.
