from i2ptorrents.i18n import STRINGS, normalize_language, set_language, t


def test_default_language_is_english() -> None:
    set_language("en")
    assert t("settings") == "Settings"
    assert t("add_torrent") == "Add torrent"


def test_russian_language() -> None:
    set_language("ru")
    assert t("settings") == "Настройки"
    assert t("add_torrent") == "Добавить торрент"
    set_language("en")


def test_normalize_language() -> None:
    assert normalize_language("ru") == "ru"
    assert normalize_language("Русский") == "ru"
    assert normalize_language("en") == "en"
    assert normalize_language("de") == "en"


def test_about_strings() -> None:
    set_language("en")
    assert t("about") == "About"
    assert "0.1.0" in t("subtitle", version="0.1.0")
    assert "BSD-3-Clause" in t("about_body", version="0.1.0", author="Vade", license="BSD-3-Clause")
    set_language("ru")
    assert t("about") == "О программе"
    set_language("en")


def test_translation_catalogs_have_the_same_keys() -> None:
    assert set(STRINGS["en"]) == set(STRINGS["ru"])
