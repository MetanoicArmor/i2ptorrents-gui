use std::collections::HashMap;
use std::sync::OnceLock;

const EN_JSON: &str = include_str!("../assets/i18n/en.json");
const RU_JSON: &str = include_str!("../assets/i18n/ru.json");

fn catalog() -> &'static HashMap<String, HashMap<String, String>> {
    static CATALOG: OnceLock<HashMap<String, HashMap<String, String>>> = OnceLock::new();
    CATALOG.get_or_init(|| {
        let mut map = HashMap::new();
        map.insert(
            "en".to_string(),
            serde_json::from_str(EN_JSON).expect("en.json"),
        );
        map.insert(
            "ru".to_string(),
            serde_json::from_str(RU_JSON).expect("ru.json"),
        );
        map
    })
}

thread_local! {
    static LANGUAGE: std::cell::RefCell<String> = std::cell::RefCell::new("en".to_string());
}

pub fn set_language(value: &str) -> String {
    let normalized = normalize_language(Some(value));
    LANGUAGE.with(|cell| {
        *cell.borrow_mut() = normalized.clone();
    });
    normalized
}

pub fn language() -> String {
    LANGUAGE.with(|cell| cell.borrow().clone())
}

pub fn strings() -> &'static HashMap<String, HashMap<String, String>> {
    catalog()
}

pub fn normalize_language(value: Option<&str>) -> String {
    let raw = value.unwrap_or("").trim().to_lowercase();
    if matches!(raw.as_str(), "ru" | "russian" | "рус" | "русский") {
        "ru".to_string()
    } else {
        "en".to_string()
    }
}

pub fn t(key: &str) -> String {
    t_args(key, &[])
}

pub fn t_args(key: &str, args: &[(&str, &str)]) -> String {
    let lang = language();
    let catalog = catalog();
    let bundle = catalog.get(&lang).or_else(|| catalog.get("en"));
    let mut text = bundle
        .and_then(|map| map.get(key))
        .cloned()
        .or_else(|| catalog.get("en").and_then(|map| map.get(key)).cloned())
        .unwrap_or_else(|| key.to_string());
    for (name, value) in args {
        let named = format!("{{{name}}}");
        text = text.replace(&named, value);
        let prefix = format!("{{{name}:");
        while let Some(start) = text.find(&prefix) {
            if let Some(rel_end) = text[start..].find('}') {
                let end = start + rel_end + 1;
                text.replace_range(start..end, value);
            } else {
                break;
            }
        }
    }
    text
}
