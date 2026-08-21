pub fn stylesheet(theme: &str) -> &'static str {
    if theme == "night" {
        include_str!("../assets/theme/night.qss")
    } else {
        include_str!("../assets/theme/light.qss")
    }
}

pub fn tooltip_palette_colors(theme: &str) -> (&'static str, &'static str) {
    if theme == "night" {
        ("#22252d", "#e3e8f1")
    } else {
        ("#f2f4f8", "#1d1d1f")
    }
}
