# winget

Manifests for **MetanoicArmor.I2PTorrents** (portable zip from GitHub Releases).

After a release:

```bash
./packaging/refresh-checksums.sh v0.2.7
# paste InstallerSha256 into 0.2.7/MetanoicArmor.I2PTorrents.installer.yaml
```

The published zip is portable and is launched from the WinGet `Links` directory. Set `ArchiveBinariesDependOnPath: true` so Qt DLLs beside the exe are found. Until a build ships `vcruntime140.dll` next to the exe (instead of only `vc_redist.*.exe`), also declare `Microsoft.VCRedist.2015+.x64` / `.arm64` as a package dependency. Without that, validation exits `0xC0000135` (`STATUS_DLL_NOT_FOUND`).

Then open a PR against [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) under:

`manifests/m/MetanoicArmor/I2PTorrents/0.2.7/`

Users (once merged):

```powershell
winget install MetanoicArmor.I2PTorrents
```
