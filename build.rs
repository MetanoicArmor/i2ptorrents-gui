#[cfg(target_os = "macos")]
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    macos_qt_stubs();
    if std::env::var("CARGO_FEATURE_GUI").is_ok() {
        compile_qt_chrome();
    }
}

fn compile_qt_chrome() {
    println!("cargo:rerun-if-changed=native/qt_chrome.cpp");
    println!("cargo:rerun-if-changed=native/qt_chrome.h");
    println!("cargo:rerun-if-changed=native/macos_vibrancy.mm");
    println!("cargo:rerun-if-changed=native/macos_vibrancy.h");
    let mut build = cc::Build::new();
    build.cpp(true);
    build.std("c++17");
    build.file("native/qt_chrome.cpp");
    build.include("native");
    build.warnings(false);
    if cfg!(target_os = "macos") {
        let mut objc = cc::Build::new();
        objc.file("native/macos_vibrancy.mm");
        objc.flag("-fobjc-arc");
        objc.include("native");
        objc.warnings(false);
        objc.compile("i2p_macos_vibrancy");
        println!("cargo:rustc-link-lib=framework=AppKit");
        println!("cargo:rustc-link-lib=framework=QuartzCore");
    }
    if let Some((prefix, version)) = find_qt() {
        let major = if version.starts_with("6.") { 6 } else { 5 };
        let include_path =
            qmake_query("QT_INSTALL_HEADERS").unwrap_or_else(|| format!("{prefix}/include"));
        let lib_path = qmake_query("QT_INSTALL_LIBS").unwrap_or_else(|| format!("{prefix}/lib"));
        build.include(&include_path);
        if cfg!(target_os = "macos") {
            build.flag(&format!("-F{lib_path}"));
        }
        for module in ["QtCore", "QtGui", "QtWidgets"] {
            let framework_headers =
                PathBuf::from(&lib_path).join(format!("{module}.framework/Headers"));
            if framework_headers.exists() {
                build.include(&framework_headers);
            }
            let direct = PathBuf::from(&include_path).join(module);
            if direct.exists() {
                build.include(&direct);
            }
            for sub in ["qt6", "Qt6", "qt5", "Qt5"] {
                let nested = PathBuf::from(&include_path).join(sub).join(module);
                if nested.exists() {
                    build.include(&nested);
                }
            }
        }
        if cfg!(target_os = "windows") {
            build.flag("/Zc:__cplusplus");
            build.flag("/DNOMINMAX");
            build.flag("/permissive-");
            println!("cargo:rustc-link-search=native={lib_path}");
            println!("cargo:rustc-link-lib=dwmapi");
            println!("cargo:rustc-link-lib=user32");
            println!("cargo:rustc-link-lib=Qt{major}Core");
            println!("cargo:rustc-link-lib=Qt{major}Gui");
            println!("cargo:rustc-link-lib=Qt{major}Widgets");
        }
        if cfg!(target_os = "linux") {
            println!("cargo:rustc-link-search=native={lib_path}");
            println!("cargo:rustc-link-lib=Qt{major}Core");
            println!("cargo:rustc-link-lib=Qt{major}Gui");
            println!("cargo:rustc-link-lib=Qt{major}Widgets");
        }
    }
    build.compile("i2p_qt_chrome");
}

fn macos_qt_stubs() {
    #[cfg(target_os = "macos")]
    {
        if std::env::var("CARGO_CFG_TARGET_OS").ok().as_deref() != Some("macos") {
            return;
        }
        let Some(prefix) = macos_qt_prefix() else {
            return;
        };
        let lib = prefix.join("lib");
        let out = PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("qt-macos-stubs");
        let _ = fs::create_dir_all(&out);
        for (name, framework) in [
            ("libQt6Core.dylib", "QtCore"),
            ("libQt6Gui.dylib", "QtGui"),
            ("libQt6Widgets.dylib", "QtWidgets"),
            ("libQt6UiTools.dylib", "QtUiTools"),
        ] {
            let dest = out.join(name);
            let src = lib.join(format!("{framework}.framework/{framework}"));
            let _ = fs::remove_file(&dest);
            let _ = std::os::unix::fs::symlink(&src, &dest);
        }
        println!("cargo:rustc-link-search=native={}", out.display());
        println!("cargo:rustc-link-arg=-F{}", lib.display());
    }
}

#[cfg(target_os = "macos")]
fn macos_qt_prefix() -> Option<PathBuf> {
    if let Ok(prefix) = std::env::var("I2P_QT_PREFIX") {
        let root = PathBuf::from(&prefix);
        let lib = root.join("lib/QtCore.framework/QtCore");
        if lib.exists() {
            return Some(root);
        }
    }
    for candidate in ["/opt/homebrew", "/usr/local"] {
        let lib = PathBuf::from(candidate).join("lib/QtCore.framework/QtCore");
        if lib.exists() {
            return Some(PathBuf::from(candidate));
        }
    }
    None
}

fn find_qt() -> Option<(String, String)> {
    for cmd in qmake_commands() {
        if let Some(result) = query_qt_prefix(&cmd) {
            return Some(result);
        }
    }
    None
}

fn qmake_commands() -> Vec<String> {
    #[cfg(target_os = "windows")]
    {
        let mut cmds = vec![
            "qmake6".to_string(),
            "qmake".to_string(),
            "qmake-qt5".to_string(),
        ];
        if let Some(path) = find_qmake_on_windows() {
            cmds.insert(0, path);
        }
        cmds
    }
    #[cfg(not(target_os = "windows"))]
    {
        vec![
            "qmake6".to_string(),
            "qmake".to_string(),
            "qmake-qt5".to_string(),
        ]
    }
}

fn query_qt_prefix(qmake: &str) -> Option<(String, String)> {
    let output = Command::new(qmake)
        .args(["-query", "QT_VERSION"])
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }
    let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
    if !version.starts_with('5') && !version.starts_with('6') {
        return None;
    }
    let prefix = Command::new(qmake)
        .args(["-query", "QT_INSTALL_PREFIX"])
        .output()
        .ok()?;
    if !prefix.status.success() {
        return None;
    }
    let path = String::from_utf8_lossy(&prefix.stdout).trim().to_string();
    if path.is_empty() || !Path::new(&path).exists() {
        return None;
    }
    Some((path, version))
}

#[cfg(target_os = "windows")]
fn find_qmake_on_windows() -> Option<String> {
    let mut candidates = Vec::new();
    for key in ["QTDIR", "Qt6_DIR", "QT_ROOT"] {
        if let Ok(root) = std::env::var(key) {
            let qmake = PathBuf::from(&root).join("bin").join("qmake.exe");
            if qmake.exists() {
                candidates.push(qmake);
            }
        }
    }
    for root in ["C:\\Qt", "D:\\Qt"] {
        let root = PathBuf::from(root);
        if !root.is_dir() {
            continue;
        }
        let Ok(versions) = std::fs::read_dir(&root) else {
            continue;
        };
        for version in versions.flatten() {
            let Ok(kits) = std::fs::read_dir(version.path()) else {
                continue;
            };
            for kit in kits.flatten() {
                let qmake = kit.path().join("bin").join("qmake.exe");
                if qmake.exists() {
                    candidates.push(qmake);
                }
            }
        }
    }
    candidates.sort_by(|a, b| qt_kit_rank(a).cmp(&qt_kit_rank(b)));
    candidates.first().map(|path| path.to_string_lossy().into_owned())
}

#[cfg(target_os = "windows")]
fn qt_kit_rank(path: &Path) -> u8 {
    let text = path.to_string_lossy().to_ascii_lowercase();
    if text.contains("msvc2022_64") {
        0
    } else if text.contains("msvc2019_64") {
        1
    } else if text.contains("msvc") {
        2
    } else if text.contains("mingw") {
        3
    } else {
        4
    }
}

fn qmake_query(key: &str) -> Option<String> {
    for cmd in qmake_commands() {
        let output = Command::new(&cmd).args(["-query", key]).output().ok()?;
        if output.status.success() {
            let value = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if !value.is_empty() {
                return Some(value);
            }
        }
    }
    None
}
