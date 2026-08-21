use i2ptorrents_gui::i18n::{normalize_language, set_language, strings, t, t_args};

#[test]
fn default_language_is_english() {
    set_language("en");
    assert_eq!(t("settings"), "Settings");
    assert_eq!(t("add_torrent"), "Add torrent");
    assert!(t("add_torrent_tip").contains("{shortcut}"));
    assert!(t("settings_tip").contains("{shortcut}"));
}

#[test]
fn russian_language() {
    set_language("ru");
    assert_eq!(t("settings"), "Настройки");
    assert_eq!(t("add_torrent"), "Добавить торрент");
    assert!(t("add_torrent_tip").contains("Добавить торрент"));
    assert!(t("settings_tip").contains("Настройки"));
    set_language("en");
}

#[test]
fn normalize_language_aliases() {
    assert_eq!(normalize_language(Some("ru")), "ru");
    assert_eq!(normalize_language(Some("Русский")), "ru");
    assert_eq!(normalize_language(Some("en")), "en");
    assert_eq!(normalize_language(Some("de")), "en");
}

#[test]
fn about_strings() {
    set_language("en");
    assert_eq!(t("about"), "About");
    assert_eq!(t("subtitle"), "for i2pd");
    let body = t_args(
        "about_body",
        &[
            ("version", "0.1.0"),
            ("author", "Vade"),
            ("license", "BSD-3-Clause"),
        ],
    );
    assert!(body.contains("BSD-3-Clause"));
    assert_eq!(t("donate_gram"), "Donations: Gram (ex-TON)");
    set_language("ru");
    assert_eq!(t("about"), "О программе");
    set_language("en");
}

#[test]
fn translation_catalogs_have_the_same_keys() {
    let catalog = strings();
    assert_eq!(
        catalog["en"]
            .keys()
            .collect::<std::collections::BTreeSet<_>>(),
        catalog["ru"].keys().collect()
    );
}
