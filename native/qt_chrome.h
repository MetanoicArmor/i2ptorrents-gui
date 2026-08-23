#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*i2p_void_cb)(void *ctx);
typedef void (*i2p_int_cb)(void *ctx, int value);
typedef int (*i2p_file_change_cb)(void *ctx, int index, int wanted, int priority);

void i2p_widget_delete(void *widget);
void i2p_widget_set_object_name(void *widget, const char *name);
void i2p_widget_set_tooltip(void *widget, const char *tip);
void i2p_widget_set_cursor(void *widget, int shape);
void i2p_widget_on_click(void *widget, i2p_void_cb cb, void *ctx);
void i2p_label_set_text(void *widget, const char *text);
void i2p_widget_repolish(void *widget);
void i2p_line_edit_set_placeholder(void *widget, const char *text);
const char *i2p_line_edit_text(void *widget);
void i2p_line_edit_set_text(void *widget, const char *text);
void i2p_widget_set_stylesheet(void *widget, const char *css);

void *i2p_dialog_new(void *parent, const char *title, int w, int h);
int i2p_dialog_exec(void *dialog);
void i2p_dialog_accept(void *dialog);
void i2p_dialog_reject(void *dialog);
void i2p_dialog_finish(void *dialog);

void i2p_push_button_set_checkable(void *widget, int checkable);
void i2p_push_button_set_checked(void *widget, int checked);
void i2p_push_button_set_auto_exclusive(void *widget, int exclusive);
void i2p_push_button_set_text(void *widget, const char *text);

void *i2p_piece_map_new(const uint8_t *have, int len);
void i2p_piece_map_set_have(void *widget, const uint8_t *have, int len);

void *i2p_overlay_scroll_new(void);
void i2p_overlay_scroll_set_widget(void *scroll, void *child);
void i2p_overlay_scroll_apply_theme(void *scroll, const char *theme);

void *i2p_torrent_card_new(const char *theme);

void *i2p_styled_combo_new(void);
void i2p_styled_combo_clear(void *combo);
void i2p_styled_combo_add_item(void *combo, const char *text, const char *data);
void i2p_styled_combo_set_index(void *combo, int index);
int i2p_styled_combo_index(void *combo);
const char *i2p_styled_combo_data(void *combo);
void i2p_styled_combo_apply_theme(void *combo, const char *theme);
void i2p_styled_combo_on_changed(void *combo, i2p_int_cb cb, void *ctx);
void i2p_styled_combo_block_signals(void *combo, int block);

void *i2p_actions_popup_new(void *parent);
void i2p_actions_popup_add_action(void *popup, const char *text, int enabled,
                                  i2p_void_cb cb, void *ctx);
void i2p_actions_popup_add_separator(void *popup);
void i2p_actions_popup_apply_theme(void *popup, const char *theme);
void i2p_actions_popup_show_below(void *popup, void *anchor);

void *i2p_spin_row_new(int min, int max, int value, const char *suffix);
int i2p_spin_row_value(void *row);
void i2p_spin_row_set_value(void *row, int value);
void i2p_spin_row_set_suffix(void *row, const char *suffix);
void i2p_spin_row_on_changed(void *row, i2p_int_cb cb, void *ctx);

void i2p_install_rounded_tooltips(void);
void i2p_apply_tooltip_palette(const char *theme);
void i2p_apply_window_material(void *widget, int night);

typedef struct i2p_settings_in {
    const char *stylesheet;
    const char *title;
    const char *rpc_label;
    const char *rpc_value;
    const char *rpc_placeholder;
    const char *rpc_tip;
    const char *dir_label;
    const char *dir_value;
    const char *browse;
    const char *refresh_label;
    const char *seconds_suffix;
    int refresh_value;
    const char *lang_label;
    const char *lang_en;
    const char *lang_ru;
    const char *lang_current;
    const char *theme_label;
    const char *theme_light;
    const char *theme_night;
    const char *theme_current;
    const char *view_label;
    const char *view_simple;
    const char *view_detailed;
    const char *view_current;
    const char *note;
    const char *save;
    const char *cancel;
} i2p_settings_in;

int i2p_settings_exec(void *parent, const i2p_settings_in *in);
const char *i2p_settings_rpc(void);
const char *i2p_settings_dir(void);
int i2p_settings_refresh(void);
const char *i2p_settings_language(void);
const char *i2p_settings_theme(void);
const char *i2p_settings_view(void);
void i2p_defer(i2p_void_cb cb, void *ctx);
const char *i2p_open_file(void *parent, const char *title, const char *filter);

typedef struct i2p_about_in {
    const char *stylesheet;
    const char *title;
    const char *heading;
    const char *body;
    const char *github_label;
    const char *github_url;
    const char *donate_label;
    const char *donate_address;
    const char *qr_path;
    const char *ok;
} i2p_about_in;

void i2p_about_exec(void *parent, const i2p_about_in *in);

typedef struct i2p_file_row {
    int index;
    const char *name;
    const char *full_name;
    const char *size;
    const char *progress;
    int wanted;
    int priority;
} i2p_file_row;

typedef struct i2p_files_in {
    const char *stylesheet;
    const char *title;
    const char *note;
    const char *unsupported_note;
    const char *empty;
    const char *close;
    const char *col_name;
    const char *col_size;
    const char *col_progress;
    const char *col_priority;
    const char *priority_skip;
    const char *priority_low;
    const char *priority_normal;
    const char *priority_high;
    const i2p_file_row *files;
    int file_count;
} i2p_files_in;

void i2p_files_exec(void *parent, const i2p_files_in *in, i2p_file_change_cb cb, void *ctx);

typedef struct i2p_peer_row {
    const char *address;
    const char *address_tip;
    const char *client;
    const char *rate_down;
    const char *rate_up;
    const char *flags;
} i2p_peer_row;

typedef struct i2p_peers_in {
    const char *stylesheet;
    const char *title;
    const char *note;
    const char *empty;
    const char *close;
    const char *col_address;
    const char *col_client;
    const char *col_down;
    const char *col_up;
    const char *col_flags;
    const i2p_peer_row *peers;
    int peer_count;
} i2p_peers_in;

void i2p_peers_exec(void *parent, const i2p_peers_in *in);

int i2p_confirm_remove(void *parent, const char *title, const char *text,
                       const char *checkbox, const char *yes_label,
                       const char *cancel_label, int *delete_data);
void i2p_set_named_text(void *parent, const char *name, const char *text);
void i2p_set_dialog_title(void *parent, const char *title);
void i2p_apply_app_font(int point_size, const char *fonts_dir, const char *language);
void i2p_shortcut_new(void *parent, const char *key, i2p_void_cb cb, void *ctx);

#ifdef __cplusplus
}
#endif
