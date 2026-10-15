# Homebrew Cask

[`Casks/i2ptorrents.rb`](Casks/i2ptorrents.rb) installs the macOS `.app` from GitHub Releases.

## Tap

Repository: **[MetanoicArmor/homebrew-i2ptorrents](https://github.com/MetanoicArmor/homebrew-i2ptorrents)**

```bash
brew install --cask metanoicarmor/i2ptorrents/i2ptorrents
```

Or:

```bash
brew tap MetanoicArmor/i2ptorrents
brew install --cask i2ptorrents
```

After each release, run `../refresh-checksums.sh vX.Y.Z`, update `version` / `sha256` here, copy the cask into the tap, and push.
