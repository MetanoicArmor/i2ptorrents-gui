# -*- mode: python ; coding: utf-8 -*-
import os
import sys

_SPECDIR = os.path.dirname(os.path.abspath(SPEC))
_icon_file = "I2PTorrents.ico" if sys.platform == "win32" else "icon.png"

_datas = []
for _src, _dest in (("icon.png", "."), ("VERSION", "."), ("AUTHORS", "."), ("LICENSE", ".")):
    if os.path.isfile(os.path.join(_SPECDIR, _src)):
        _datas.append((_src, _dest))

a = Analysis(
    ["run_gui.py"],
    pathex=[_SPECDIR],
    binaries=[],
    datas=_datas,
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="I2PTorrents",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=_icon_file,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="I2PTorrents",
)
