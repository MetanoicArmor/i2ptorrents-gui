use i2ptorrents_gui::theme::tooltip_palette_colors;

#[test]
fn tooltip_palette_follows_theme() {
    assert_eq!(tooltip_palette_colors("night"), ("#2c2c2e", "#f5f5f7"));
    assert_eq!(tooltip_palette_colors("light"), ("#f2f2f7", "#1d1d1f"));
    assert_eq!(tooltip_palette_colors("other"), ("#f2f2f7", "#1d1d1f"));
}
