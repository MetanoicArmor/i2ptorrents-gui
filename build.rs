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
    let mut build = cc::Build::new();
    build.cpp(true);
    build.std("c++17");
    build.file("native/qt_chrome.cpp");
    build.include("native");
    build.warnings(false);
    if let Some((prefix, version)) = find_qt() {
        let major = if version.starts_with("6.") { 6 } else { 5 };
        let include_path =
            qmake_query("QT_INSTALL_HEADERS").unwrap_or_else(|| format!("{prefix}/include"));
        let lib_path = qmake_query("QT_INSTALL_LIBS").unwrap_or_else(|| format!("{prefix}/lib"));
        build.include(&include_path);
        build.flag(&format!("-F{lib_path}"));
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
            println!("cargo:rustc-link-lib=dwmapi");
            println!("cargo:rustc-link-lib=Qt{major}Core");
            println!("cargo:rustc-link-lib=Qt{major}Gui");
            println!("cargo:rustc-link-lib=Qt{major}Widgets");
        }
        if cfg!(target_os = "linux") {
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
    for candidate in ["/opt/homebrew", "/usr/local"] {
        let lib = PathBuf::from(candidate).join("lib/QtCore.framework/QtCore");
        if lib.exists() {
            return Some(PathBuf::from(candidate));
        }
    }
    None
}

fn find_qt() -> Option<(String, String)> {
    for cmd in ["qmake6", "qmake", "qmake-qt5"] {
        let output = Command::new(cmd)
            .args(["-query", "QT_VERSION"])
            .output()
            .ok()?;
        if !output.status.success() {
            continue;
        }
        let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
        if !version.starts_with('5') && !version.starts_with('6') {
            continue;
        }
        let prefix = Command::new(cmd)
            .args(["-query", "QT_INSTALL_PREFIX"])
            .output()
            .ok()?;
        if prefix.status.success() {
            let path = String::from_utf8_lossy(&prefix.stdout).trim().to_string();
            if !path.is_empty() && Path::new(&path).exists() {
                return Some((path, version));
            }
        }
    }
    None
}

fn qmake_query(key: &str) -> Option<String> {
    for cmd in ["qmake6", "qmake", "qmake-qt5"] {
        let output = Command::new(cmd).args(["-query", key]).output().ok()?;
        if output.status.success() {
            let value = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if !value.is_empty() {
                return Some(value);
            }
        }
    }
    None
}
