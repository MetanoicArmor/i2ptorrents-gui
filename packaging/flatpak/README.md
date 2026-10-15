# Flatpak

Manifest: [`org.metanoicarmor.I2PTorrents.yml`](org.metanoicarmor.I2PTorrents.yml)

Uses the Linux **x86_64 AppImage** from GitHub Releases. Update `url` / `sha256` after each tag (`../refresh-checksums.sh`).

## Local test

```bash
flatpak install flathub org.freedesktop.Platform//24.08 org.freedesktop.Sdk//24.08
flatpak-builder --user --install --force-clean build-dir packaging/flatpak/org.metanoicarmor.I2PTorrents.yml
flatpak run org.metanoicarmor.I2PTorrents
```

## Flathub

Follow [App Submission](https://github.com/flathub/flathub/wiki/App-Submission). Typical app-id repo: `flathub/org.metanoicarmor.I2PTorrents`.
