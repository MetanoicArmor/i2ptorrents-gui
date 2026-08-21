//! Thin cxx-free FFI around custom Qt widgets compiled from `native/qt_chrome.cpp`.

use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::os::raw::c_uchar;

use qtrs::ffi::QWidget;
use qtrs::widget::AsWidget;

#[repr(C)]
struct I2pSettingsIn {
    stylesheet: *const c_char,
    title: *const c_char,
    rpc_label: *const c_char,
    rpc_value: *const c_char,
    rpc_placeholder: *const c_char,
    rpc_tip: *const c_char,
    dir_label: *const c_char,
    dir_value: *const c_char,
    browse: *const c_char,
    refresh_label: *const c_char,
    seconds_suffix: *const c_char,
    refresh_value: c_int,
    lang_label: *const c_char,
    lang_en: *const c_char,
    lang_ru: *const c_char,
    lang_current: *const c_char,
    theme_label: *const c_char,
    theme_light: *const c_char,
    theme_night: *const c_char,
    theme_current: *const c_char,
    view_label: *const c_char,
    view_simple: *const c_char,
    view_detailed: *const c_char,
    view_current: *const c_char,
    note: *const c_char,
    save: *const c_char,
    cancel: *const c_char,
}

#[repr(C)]
struct I2pAboutIn {
    stylesheet: *const c_char,
    title: *const c_char,
    heading: *const c_char,
    body: *const c_char,
    github_label: *const c_char,
    github_url: *const c_char,
    donate_label: *const c_char,
    donate_address: *const c_char,
    qr_path: *const c_char,
    ok: *const c_char,
}

#[link(name = "i2p_qt_chrome")]
extern "C" {
    fn i2p_widget_delete(widget: *mut c_void);
    fn i2p_widget_set_object_name(widget: *mut c_void, name: *const c_char);
    fn i2p_widget_set_tooltip(widget: *mut c_void, tip: *const c_char);
    fn i2p_widget_set_cursor(widget: *mut c_void, shape: c_int);
    fn i2p_label_set_text(widget: *mut c_void, text: *const c_char);
    fn i2p_widget_repolish(widget: *mut c_void);
    fn i2p_line_edit_set_placeholder(widget: *mut c_void, text: *const c_char);
    fn i2p_line_edit_text(widget: *mut c_void) -> *const c_char;
    fn i2p_line_edit_set_text(widget: *mut c_void, text: *const c_char);
    fn i2p_widget_set_stylesheet(widget: *mut c_void, css: *const c_char);
    fn i2p_dialog_new(parent: *mut c_void, title: *const c_char, w: c_int, h: c_int)
        -> *mut c_void;
    fn i2p_dialog_exec(dialog: *mut c_void) -> c_int;
    fn i2p_dialog_accept(dialog: *mut c_void);
    fn i2p_dialog_reject(dialog: *mut c_void);
    fn i2p_dialog_finish(dialog: *mut c_void);
    fn i2p_settings_exec(parent: *mut c_void, input: *const I2pSettingsIn) -> c_int;
    fn i2p_settings_rpc() -> *const c_char;
    fn i2p_settings_dir() -> *const c_char;
    fn i2p_settings_refresh() -> c_int;
    fn i2p_settings_language() -> *const c_char;
    fn i2p_settings_theme() -> *const c_char;
    fn i2p_settings_view() -> *const c_char;
    fn i2p_defer(cb: I2pVoidCb, ctx: *mut c_void);
    fn i2p_open_file(
        parent: *mut c_void,
        title: *const c_char,
        filter: *const c_char,
    ) -> *const c_char;
    fn i2p_about_exec(parent: *mut c_void, input: *const I2pAboutIn);
    fn i2p_push_button_set_checkable(widget: *mut c_void, checkable: c_int);
    fn i2p_push_button_set_checked(widget: *mut c_void, checked: c_int);
    fn i2p_push_button_set_auto_exclusive(widget: *mut c_void, exclusive: c_int);
    fn i2p_push_button_set_text(widget: *mut c_void, text: *const c_char);
    fn i2p_piece_map_new(have: *const c_uchar, len: c_int) -> *mut c_void;
    fn i2p_overlay_scroll_new() -> *mut c_void;
    fn i2p_overlay_scroll_set_widget(scroll: *mut c_void, child: *mut c_void);
    fn i2p_overlay_scroll_apply_theme(scroll: *mut c_void, theme: *const c_char);
    fn i2p_styled_combo_new() -> *mut c_void;
    fn i2p_styled_combo_clear(combo: *mut c_void);
    fn i2p_styled_combo_add_item(combo: *mut c_void, text: *const c_char, data: *const c_char);
    fn i2p_styled_combo_set_index(combo: *mut c_void, index: c_int);
    fn i2p_styled_combo_data(combo: *mut c_void) -> *const c_char;
    fn i2p_styled_combo_apply_theme(combo: *mut c_void, theme: *const c_char);
    fn i2p_styled_combo_on_changed(combo: *mut c_void, cb: I2pIntCb, ctx: *mut c_void);
    fn i2p_styled_combo_block_signals(combo: *mut c_void, block: c_int);
    fn i2p_actions_popup_new(parent: *mut c_void) -> *mut c_void;
    fn i2p_actions_popup_add_action(
        popup: *mut c_void,
        text: *const c_char,
        enabled: c_int,
        cb: I2pVoidCb,
        ctx: *mut c_void,
    );
    fn i2p_actions_popup_add_separator(popup: *mut c_void);
    fn i2p_actions_popup_apply_theme(popup: *mut c_void, theme: *const c_char);
    fn i2p_actions_popup_show_below(popup: *mut c_void, anchor: *mut c_void);
    fn i2p_spin_row_new(min: c_int, max: c_int, value: c_int, suffix: *const c_char)
        -> *mut c_void;
    fn i2p_spin_row_value(row: *mut c_void) -> c_int;
    fn i2p_spin_row_set_suffix(row: *mut c_void, suffix: *const c_char);
    fn i2p_spin_row_on_changed(row: *mut c_void, cb: I2pIntCb, ctx: *mut c_void);
    fn i2p_install_rounded_tooltips();
    fn i2p_apply_tooltip_palette(theme: *const c_char);
    fn i2p_confirm_remove(
        parent: *mut c_void,
        title: *const c_char,
        text: *const c_char,
        checkbox: *const c_char,
        yes_label: *const c_char,
        cancel_label: *const c_char,
        delete_data: *mut c_int,
    ) -> c_int;
    fn i2p_set_named_text(parent: *mut c_void, name: *const c_char, text: *const c_char);
    fn i2p_set_dialog_title(parent: *mut c_void, title: *const c_char);
    fn i2p_apply_app_font(point_size: c_int);
    fn i2p_shortcut_new(parent: *mut c_void, key: *const c_char, cb: I2pVoidCb, ctx: *mut c_void);
}

type I2pVoidCb = unsafe extern "C" fn(*mut c_void);
type I2pIntCb = unsafe extern "C" fn(*mut c_void, c_int);

unsafe extern "C" fn void_trampoline(ctx: *mut c_void) {
    let callback = &**(ctx as *mut Box<dyn Fn()>);
    callback();
}

unsafe extern "C" fn int_trampoline(ctx: *mut c_void, value: c_int) {
    let callback = &**(ctx as *mut Box<dyn Fn(i32)>);
    callback(value);
}

fn leak_void(callback: impl Fn() + 'static) -> *mut c_void {
    Box::into_raw(Box::new(Box::new(callback) as Box<dyn Fn()>)) as *mut c_void
}

fn leak_int(callback: impl Fn(i32) + 'static) -> *mut c_void {
    Box::into_raw(Box::new(Box::new(callback) as Box<dyn Fn(i32)>)) as *mut c_void
}

fn cstr(value: &str) -> CString {
    CString::new(value.replace('\0', "")).unwrap_or_else(|_| CString::new("").unwrap())
}

pub struct NativeWidget {
    ptr: *mut QWidget,
    has_parent: bool,
}

impl NativeWidget {
    fn from_raw(ptr: *mut c_void) -> Self {
        debug_assert!(!ptr.is_null());
        Self {
            ptr: ptr as *mut QWidget,
            has_parent: false,
        }
    }

    pub fn as_ptr(&self) -> *mut c_void {
        self.ptr as *mut c_void
    }

    pub fn piece_map(have: &[bool]) -> Self {
        let bits: Vec<u8> = have.iter().map(|bit| u8::from(*bit)).collect();
        let ptr = unsafe { i2p_piece_map_new(bits.as_ptr(), bits.len() as c_int) };
        Self::from_raw(ptr)
    }

    pub fn overlay_scroll() -> Self {
        Self::from_raw(unsafe { i2p_overlay_scroll_new() })
    }

    pub fn styled_combo() -> Self {
        Self::from_raw(unsafe { i2p_styled_combo_new() })
    }

    pub fn spin_row(min: i32, max: i32, value: i32, suffix: &str) -> Self {
        let suffix = cstr(suffix);
        Self::from_raw(unsafe { i2p_spin_row_new(min, max, value, suffix.as_ptr()) })
    }

    pub fn dialog(parent: &dyn AsWidget, title: &str, width: i32, height: i32) -> Self {
        let title = cstr(title);
        let mut widget = Self::from_raw(unsafe {
            i2p_dialog_new(
                parent.widget_ptr() as *mut c_void,
                title.as_ptr(),
                width,
                height,
            )
        });
        widget.has_parent = true;
        widget
    }

    pub fn set_object_name(&self, name: &str) {
        let name = cstr(name);
        unsafe { i2p_widget_set_object_name(self.as_ptr(), name.as_ptr()) }
    }
}

impl AsWidget for NativeWidget {
    fn widget_ptr(&self) -> *mut QWidget {
        self.ptr
    }

    fn set_has_parent(&mut self) {
        self.has_parent = true;
    }
}

impl Drop for NativeWidget {
    fn drop(&mut self) {
        if !self.has_parent && !self.ptr.is_null() {
            unsafe { i2p_widget_delete(self.as_ptr()) }
        }
        self.ptr = std::ptr::null_mut();
    }
}

pub fn set_tooltip(widget: &dyn AsWidget, tip: &str) {
    set_tooltip_ptr(widget.widget_ptr() as usize, tip);
}

pub fn set_tooltip_ptr(ptr: usize, tip: &str) {
    if ptr == 0 {
        return;
    }
    let tip = cstr(tip);
    unsafe { i2p_widget_set_tooltip(ptr as *mut c_void, tip.as_ptr()) }
}

pub fn set_label_text(ptr: usize, text: &str) {
    if ptr == 0 {
        return;
    }
    let text = cstr(text);
    unsafe { i2p_label_set_text(ptr as *mut c_void, text.as_ptr()) }
}

pub fn set_object_name_ptr(ptr: usize, name: &str) {
    if ptr == 0 {
        return;
    }
    let name = cstr(name);
    unsafe { i2p_widget_set_object_name(ptr as *mut c_void, name.as_ptr()) }
}

pub fn set_cursor_ptr(ptr: usize, shape: i32) {
    if ptr == 0 {
        return;
    }
    unsafe { i2p_widget_set_cursor(ptr as *mut c_void, shape) }
}

pub fn set_placeholder(widget: &dyn AsWidget, text: &str) {
    set_placeholder_ptr(widget.widget_ptr() as usize, text);
}

pub fn set_placeholder_ptr(ptr: usize, text: &str) {
    if ptr == 0 {
        return;
    }
    let text = cstr(text);
    unsafe { i2p_line_edit_set_placeholder(ptr as *mut c_void, text.as_ptr()) }
}

pub fn set_stylesheet(widget: &dyn AsWidget, css: &str) {
    let css = cstr(css);
    unsafe { i2p_widget_set_stylesheet(widget.widget_ptr() as *mut c_void, css.as_ptr()) }
}

pub fn line_edit_text(ptr: usize) -> String {
    if ptr == 0 {
        return String::new();
    }
    unsafe {
        let raw = i2p_line_edit_text(ptr as *mut c_void);
        if raw.is_null() {
            String::new()
        } else {
            CStr::from_ptr(raw).to_string_lossy().into_owned()
        }
    }
}

pub fn line_edit_set_text(ptr: usize, text: &str) {
    if ptr == 0 {
        return;
    }
    let text = cstr(text);
    unsafe { i2p_line_edit_set_text(ptr as *mut c_void, text.as_ptr()) }
}

pub fn dialog_exec(dialog: &dyn AsWidget) -> bool {
    unsafe { i2p_dialog_exec(dialog.widget_ptr() as *mut c_void) != 0 }
}

pub fn dialog_accept(ptr: usize) {
    if ptr == 0 {
        return;
    }
    unsafe { i2p_dialog_accept(ptr as *mut c_void) }
}

pub fn dialog_reject(ptr: usize) {
    if ptr == 0 {
        return;
    }
    unsafe { i2p_dialog_reject(ptr as *mut c_void) }
}

pub fn dialog_finish(dialog: &dyn AsWidget) {
    unsafe { i2p_dialog_finish(dialog.widget_ptr() as *mut c_void) }
}

pub fn defer(callback: impl Fn() + 'static) {
    unsafe { i2p_defer(void_trampoline, leak_void(callback)) }
}

pub fn open_file(parent: usize, title: &str, filter: &str) -> Option<String> {
    let title = cstr(title);
    let filter = cstr(filter);
    let path = ffi_string(unsafe {
        i2p_open_file(parent as *mut c_void, title.as_ptr(), filter.as_ptr())
    });
    if path.is_empty() {
        None
    } else {
        Some(path)
    }
}

pub fn about_exec(parent: usize, stylesheet: &str) {
    use crate::i18n::{t, t_args};
    use crate::{resource_path, version, APP_AUTHOR, APP_GITHUB, APP_LICENSE, APP_NAME, APP_TON_ADDRESS};
    let body = t_args(
        "about_body",
        &[
            ("version", version()),
            ("author", APP_AUTHOR),
            ("license", APP_LICENSE),
        ],
    );
    let qr = resource_path("ton_donation_qr.png")
        .map(|path| path.to_string_lossy().into_owned())
        .unwrap_or_default();
    let stylesheet = cstr(stylesheet);
    let title = cstr(&t("about_title"));
    let heading = cstr(APP_NAME);
    let body = cstr(&body);
    let github_label = cstr(&t("github"));
    let github_url = cstr(APP_GITHUB);
    let donate_label = cstr(&t("donate_gram"));
    let donate_address = cstr(APP_TON_ADDRESS);
    let qr_path = cstr(&qr);
    let ok = cstr(&t("ok"));
    let input = I2pAboutIn {
        stylesheet: stylesheet.as_ptr(),
        title: title.as_ptr(),
        heading: heading.as_ptr(),
        body: body.as_ptr(),
        github_label: github_label.as_ptr(),
        github_url: github_url.as_ptr(),
        donate_label: donate_label.as_ptr(),
        donate_address: donate_address.as_ptr(),
        qr_path: qr_path.as_ptr(),
        ok: ok.as_ptr(),
    };
    unsafe { i2p_about_exec(parent as *mut c_void, &input) }
}

pub struct SettingsResult {
    pub rpc_url: String,
    pub torrents_dir: String,
    pub refresh_seconds: u32,
    pub language: String,
    pub theme: String,
    pub torrent_view: String,
}

pub fn settings_exec(
    parent: usize,
    stylesheet: &str,
    settings: &crate::config::AppSettings,
) -> Option<SettingsResult> {
    use crate::i18n::t;
    let stylesheet = cstr(stylesheet);
    let title = cstr(&t("settings_title"));
    let rpc_label = cstr(&t("rpc_address"));
    let rpc_value = cstr(&settings.rpc_url);
    let rpc_placeholder = cstr("http://127.0.0.1:9191/mytorrents");
    let rpc_tip = cstr(&t("rpc_setup_tip"));
    let dir_label = cstr(&t("torrents_directory"));
    let dir_value = cstr(&settings.torrents_dir);
    let browse = cstr(&t("browse"));
    let refresh_label = cstr(&t("refresh_interval"));
    let seconds_suffix = cstr(&t("seconds_suffix"));
    let lang_label = cstr(&t("language"));
    let lang_en = cstr(&t("language_name_en"));
    let lang_ru = cstr(&t("language_name_ru"));
    let lang_current = cstr(&settings.language);
    let theme_label = cstr(&t("theme"));
    let theme_light = cstr(&t("theme_light"));
    let theme_night = cstr(&t("theme_night"));
    let theme_current = cstr(&settings.theme);
    let view_label = cstr(&t("torrent_view"));
    let view_simple = cstr(&t("view_simple"));
    let view_detailed = cstr(&t("view_detailed"));
    let view_current = cstr(&settings.torrent_view);
    let note = cstr(&t("settings_note"));
    let save = cstr(&t("save"));
    let cancel = cstr(&t("cancel"));
    let input = I2pSettingsIn {
        stylesheet: stylesheet.as_ptr(),
        title: title.as_ptr(),
        rpc_label: rpc_label.as_ptr(),
        rpc_value: rpc_value.as_ptr(),
        rpc_placeholder: rpc_placeholder.as_ptr(),
        rpc_tip: rpc_tip.as_ptr(),
        dir_label: dir_label.as_ptr(),
        dir_value: dir_value.as_ptr(),
        browse: browse.as_ptr(),
        refresh_label: refresh_label.as_ptr(),
        seconds_suffix: seconds_suffix.as_ptr(),
        refresh_value: settings.refresh_seconds.clamp(2, 60) as c_int,
        lang_label: lang_label.as_ptr(),
        lang_en: lang_en.as_ptr(),
        lang_ru: lang_ru.as_ptr(),
        lang_current: lang_current.as_ptr(),
        theme_label: theme_label.as_ptr(),
        theme_light: theme_light.as_ptr(),
        theme_night: theme_night.as_ptr(),
        theme_current: theme_current.as_ptr(),
        view_label: view_label.as_ptr(),
        view_simple: view_simple.as_ptr(),
        view_detailed: view_detailed.as_ptr(),
        view_current: view_current.as_ptr(),
        note: note.as_ptr(),
        save: save.as_ptr(),
        cancel: cancel.as_ptr(),
    };
    let accepted = unsafe { i2p_settings_exec(parent as *mut c_void, &input) };
    if accepted == 0 {
        return None;
    }
    Some(SettingsResult {
        rpc_url: ffi_string(unsafe { i2p_settings_rpc() }),
        torrents_dir: ffi_string(unsafe { i2p_settings_dir() }),
        refresh_seconds: unsafe { i2p_settings_refresh() }.clamp(2, 60) as u32,
        language: ffi_string(unsafe { i2p_settings_language() }),
        theme: ffi_string(unsafe { i2p_settings_theme() }),
        torrent_view: ffi_string(unsafe { i2p_settings_view() }),
    })
}

fn ffi_string(raw: *const c_char) -> String {
    if raw.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(raw).to_string_lossy().into_owned() }
    }
}

pub fn set_checkable(widget: &dyn AsWidget, checkable: bool) {
    unsafe {
        i2p_push_button_set_checkable(widget.widget_ptr() as *mut c_void, i32::from(checkable));
        i2p_push_button_set_auto_exclusive(
            widget.widget_ptr() as *mut c_void,
            i32::from(checkable),
        );
    }
}

pub fn set_checked(ptr: usize, checked: bool) {
    if ptr == 0 {
        return;
    }
    unsafe {
        i2p_push_button_set_checked(ptr as *mut c_void, i32::from(checked));
        i2p_widget_repolish(ptr as *mut c_void);
    }
}

pub fn set_button_text(ptr: usize, text: &str) {
    if ptr == 0 {
        return;
    }
    let text = cstr(text);
    unsafe { i2p_push_button_set_text(ptr as *mut c_void, text.as_ptr()) }
}

pub fn overlay_set_widget(scroll_ptr: usize, child: &dyn AsWidget) {
    unsafe {
        i2p_overlay_scroll_set_widget(scroll_ptr as *mut c_void, child.widget_ptr() as *mut c_void)
    }
}

pub fn overlay_apply_theme(scroll_ptr: usize, theme: &str) {
    let theme = cstr(theme);
    unsafe { i2p_overlay_scroll_apply_theme(scroll_ptr as *mut c_void, theme.as_ptr()) }
}

pub fn combo_clear(ptr: usize) {
    unsafe { i2p_styled_combo_clear(ptr as *mut c_void) }
}

pub fn combo_add_item(ptr: usize, text: &str, data: &str) {
    let text = cstr(text);
    let data = cstr(data);
    unsafe { i2p_styled_combo_add_item(ptr as *mut c_void, text.as_ptr(), data.as_ptr()) }
}

pub fn combo_set_index(ptr: usize, index: i32) {
    unsafe { i2p_styled_combo_set_index(ptr as *mut c_void, index) }
}

pub fn combo_data(ptr: usize) -> String {
    unsafe {
        let raw = i2p_styled_combo_data(ptr as *mut c_void);
        if raw.is_null() {
            String::new()
        } else {
            CStr::from_ptr(raw).to_string_lossy().into_owned()
        }
    }
}

pub fn combo_apply_theme(ptr: usize, theme: &str) {
    let theme = cstr(theme);
    unsafe { i2p_styled_combo_apply_theme(ptr as *mut c_void, theme.as_ptr()) }
}

pub fn combo_on_changed(ptr: usize, callback: impl Fn(i32) + 'static) {
    unsafe { i2p_styled_combo_on_changed(ptr as *mut c_void, int_trampoline, leak_int(callback)) }
}

pub fn combo_block_signals(ptr: usize, block: bool) {
    unsafe { i2p_styled_combo_block_signals(ptr as *mut c_void, i32::from(block)) }
}

pub fn combo_set_items(ptr: usize, items: &[(&str, &str)], selected_data: &str) {
    combo_block_signals(ptr, true);
    combo_clear(ptr);
    let mut selected = 0;
    for (index, (text, data)) in items.iter().enumerate() {
        combo_add_item(ptr, text, data);
        if *data == selected_data {
            selected = index as i32;
        }
    }
    if !items.is_empty() {
        combo_set_index(ptr, selected);
    }
    combo_block_signals(ptr, false);
}

pub fn spin_value(ptr: usize) -> i32 {
    unsafe { i2p_spin_row_value(ptr as *mut c_void) }
}

pub fn spin_set_suffix(ptr: usize, suffix: &str) {
    let suffix = cstr(suffix);
    unsafe { i2p_spin_row_set_suffix(ptr as *mut c_void, suffix.as_ptr()) }
}

pub fn spin_on_changed(ptr: usize, callback: impl Fn(i32) + 'static) {
    unsafe { i2p_spin_row_on_changed(ptr as *mut c_void, int_trampoline, leak_int(callback)) }
}

pub fn popup_add_action(
    popup: *mut c_void,
    text: &str,
    enabled: bool,
    callback: impl Fn() + 'static,
) {
    let text = cstr(text);
    unsafe {
        i2p_actions_popup_add_action(
            popup,
            text.as_ptr(),
            i32::from(enabled),
            void_trampoline,
            leak_void(callback),
        );
    }
}

pub fn show_popup_below(
    parent: &dyn AsWidget,
    anchor: usize,
    theme: &str,
    build: impl FnOnce(*mut c_void),
) {
    let popup = unsafe { i2p_actions_popup_new(parent.widget_ptr() as *mut c_void) };
    let theme_c = cstr(theme);
    unsafe { i2p_actions_popup_apply_theme(popup, theme_c.as_ptr()) }
    build(popup);
    unsafe { i2p_actions_popup_show_below(popup, anchor as *mut c_void) }
}

pub fn popup_separator(popup: *mut c_void) {
    unsafe { i2p_actions_popup_add_separator(popup) }
}

pub fn install_rounded_tooltips() {
    unsafe { i2p_install_rounded_tooltips() }
}

pub fn apply_tooltip_palette(theme: &str) {
    let theme = cstr(theme);
    unsafe { i2p_apply_tooltip_palette(theme.as_ptr()) }
}

pub fn apply_app_font() {
    let point_size = if cfg!(windows) { 10 } else { 13 };
    unsafe { i2p_apply_app_font(point_size) }
}

pub fn add_shortcut(parent: &dyn AsWidget, key: &str, callback: impl Fn() + 'static) {
    let key = cstr(key);
    unsafe {
        i2p_shortcut_new(
            parent.widget_ptr() as *mut c_void,
            key.as_ptr(),
            void_trampoline,
            leak_void(callback),
        );
    }
}

pub fn confirm_remove(
    parent: &dyn AsWidget,
    title: &str,
    text: &str,
    checkbox: &str,
    yes: &str,
    cancel: &str,
) -> Option<bool> {
    let title = cstr(title);
    let text = cstr(text);
    let checkbox = cstr(checkbox);
    let yes = cstr(yes);
    let cancel = cstr(cancel);
    let mut delete_data: c_int = 0;
    let accepted = unsafe {
        i2p_confirm_remove(
            parent.widget_ptr() as *mut c_void,
            title.as_ptr(),
            text.as_ptr(),
            checkbox.as_ptr(),
            yes.as_ptr(),
            cancel.as_ptr(),
            &mut delete_data,
        )
    };
    if accepted != 0 {
        Some(delete_data != 0)
    } else {
        None
    }
}

pub fn set_named_text(parent: usize, name: &str, text: &str) {
    let name = cstr(name);
    let text = cstr(text);
    unsafe { i2p_set_named_text(parent as *mut c_void, name.as_ptr(), text.as_ptr()) }
}

pub fn set_dialog_title(parent: usize, title: &str) {
    let title = cstr(title);
    unsafe { i2p_set_dialog_title(parent as *mut c_void, title.as_ptr()) }
}
