# AUR: i2ptorrents-bin

Binary package that installs the official **AppImage** from GitHub Releases (x86_64 and aarch64).

## First publish

1. Create package **`i2ptorrents-bin`** on [aur.archlinux.org/submit](https://aur.archlinux.org/submit/).
2. Push `PKGBUILD` and `.SRCINFO` from [`i2ptorrents-bin/`](i2ptorrents-bin/).

```bash
cd packaging/aur/i2ptorrents-bin
# after filling sha256sums from ../refresh-checksums.sh
makepkg --printsrcinfo > .SRCINFO
```

## Users

```bash
yay -S i2ptorrents-bin
```
