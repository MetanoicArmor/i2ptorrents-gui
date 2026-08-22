# Bundled UI fonts

The GUI uses **[Inter](https://rsms.me/inter/)** (SIL Open Font License 1.1). Inter may be redistributed with the application.

Download before building or running:

```bash
./scripts/sync-inter-fonts.sh
```

```powershell
.\scripts\sync-inter-fonts.ps1
```

Release builds for **Linux and Windows** run the sync step and ship `fonts/` next to the executable. **macOS** uses the system San Francisco font and does not bundle font files.

## Optional system fallback

If Inter is missing, the app tries installed system UI fonts (including SF Pro Text when present). Do **not** bundle Apple SF Pro fonts in release packages.

## License

Inter © The Inter Project Authors. See `Inter-OFL.txt` after sync.
