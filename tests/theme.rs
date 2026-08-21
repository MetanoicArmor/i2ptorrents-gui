use i2ptorrents_gui::theme::tooltip_palette_colors;

#[test]
fn tooltip_palette_follows_theme() {
    assert_eq!(tooltip_palette_colors("night"), ("#22252d", "#e3e8f1"));
    assert_eq!(tooltip_palette_colors("light"), ("#f2f4f8", "#1d1d1f"));
    assert_eq!(tooltip_palette_colors("other"), ("#f2f4f8", "#1d1d1f"));
}
