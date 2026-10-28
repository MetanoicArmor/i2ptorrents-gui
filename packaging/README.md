# Package managers

Templates for distributing **I2P Torrents** from [GitHub Releases](https://github.com/MetanoicArmor/i2ptorrents-gui/releases/latest).

| Channel | Path | Notes |
| --- | --- | --- |
| **Homebrew (macOS)** | [`homebrew/`](homebrew/) | Tap: [MetanoicArmor/homebrew-i2ptorrents](https://github.com/MetanoicArmor/homebrew-i2ptorrents) |
| **winget (Windows)** | [`winget/`](winget/) | PR into [winget-pkgs](https://github.com/microsoft/winget-pkgs) |
| **AUR (Arch)** | [`aur/i2ptorrents-bin/`](aur/i2ptorrents-bin/) | Binary package from release AppImage |
| **Flatpak** | [`flatpak/`](flatpak/) | Flathub template (`org.metanoicarmor.I2PTorrents`) |
| **`.deb` / `.rpm`** | Built by `./build-linux.sh` | Attached to each GitHub Release |

## Bump after a release

```bash
./packaging/refresh-checksums.sh v0.3.0
```

Paste the printed SHA-256 values into Homebrew / AUR / winget / Flatpak files, then:

1. Commit packaging updates in this repo.
2. Copy `packaging/homebrew/Casks/i2ptorrents.rb` into the tap repo and push.
3. For AUR: update `PKGBUILD` + `.SRCINFO` and push to `aur.archlinux.org`.
4. For winget: open a PR with the manifests under `winget/0.3.0/`.
5. For Flatpak: submit/update the Flathub repo when ready.
