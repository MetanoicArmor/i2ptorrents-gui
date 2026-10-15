# winget

Manifests for **MetanoicArmor.I2PTorrents** (portable zip from GitHub Releases).

After a release:

```bash
./packaging/refresh-checksums.sh v0.2.7
# paste InstallerSha256 into 0.2.7/MetanoicArmor.I2PTorrents.installer.yaml
```

Then open a PR against [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) under:

`manifests/m/MetanoicArmor/I2PTorrents/0.2.7/`

Users (once merged):

```powershell
winget install MetanoicArmor.I2PTorrents
```
