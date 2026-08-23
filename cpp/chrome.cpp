#include "chrome.hpp"

#include "app_constants.hpp"
#include "i18n.hpp"
#include "resources.hpp"
#include "theme.hpp"

#include "qt_chrome.h"

#include <QByteArray>
#include <QVector>
#include <memory>
#include <vector>

namespace i2p {

namespace {

struct VoidCallback {
    std::function<void()> fn;
};

struct IntCallback {
    std::function<void(int)> fn;
};

struct FileChangeCallback {
    std::function<int(int, int, int)> fn;
};

void voidTrampoline(void *ctx)
{
    auto *callback = static_cast<VoidCallback *>(ctx);
    if (callback && callback->fn) {
        callback->fn();
    }
}

void intTrampoline(void *ctx, int value)
{
    auto *callback = static_cast<IntCallback *>(ctx);
    if (callback && callback->fn) {
        callback->fn(value);
    }
}

int fileChangeTrampoline(void *ctx, int index, int wanted, int priority)
{
    auto *callback = static_cast<FileChangeCallback *>(ctx);
    return callback && callback->fn ? callback->fn(index, wanted, priority) : -1;
}

void *leakVoid(std::function<void()> callback)
{
    return new VoidCallback{std::move(callback)};
}

void *leakInt(std::function<void(int)> callback)
{
    return new IntCallback{std::move(callback)};
}

void *leakFileChange(std::function<int(int, int, int)> callback)
{
    return new FileChangeCallback{std::move(callback)};
}

QByteArray toUtf8(const QString &value)
{
    return value.toUtf8();
}

class Utf8Holder {
public:
    const char *add(const QString &value)
    {
        storage_.push_back(toUtf8(value));
        return storage_.back().constData();
    }

    const char *addLiteral(const char *value)
    {
        return value;
    }

private:
    std::vector<QByteArray> storage_;
};

} // namespace

NativeWidget::NativeWidget(void *ptr, bool hasParent)
    : ptr_(ptr)
    , hasParent_(hasParent)
{
}

NativeWidget::NativeWidget(NativeWidget &&other) noexcept
    : ptr_(other.ptr_)
    , hasParent_(other.hasParent_)
{
    other.ptr_ = nullptr;
}

NativeWidget &NativeWidget::operator=(NativeWidget &&other) noexcept
{
    if (this != &other) {
        if (!hasParent_ && ptr_ != nullptr) {
            i2p_widget_delete(ptr_);
        }
        ptr_ = other.ptr_;
        hasParent_ = other.hasParent_;
        other.ptr_ = nullptr;
    }
    return *this;
}

NativeWidget::~NativeWidget()
{
    if (!hasParent_ && ptr_ != nullptr) {
        i2p_widget_delete(ptr_);
    }
    ptr_ = nullptr;
}

QWidget *NativeWidget::widget() const
{
    return static_cast<QWidget *>(ptr_);
}

NativeWidget NativeWidget::pieceMap(const QVector<bool> &have)
{
    QByteArray bits;
    bits.reserve(have.size());
    for (bool bit : have) {
        bits.append(bit ? char(1) : char(0));
    }
    return NativeWidget(i2p_piece_map_new(reinterpret_cast<const uint8_t *>(bits.constData()),
                                          bits.size()));
}

NativeWidget NativeWidget::overlayScroll()
{
    return NativeWidget(i2p_overlay_scroll_new());
}

NativeWidget NativeWidget::torrentCard(const QString &theme)
{
    return NativeWidget(i2p_torrent_card_new(toUtf8(theme).constData()));
}

void NativeWidget::setObjectName(const QString &name) const
{
    i2p_widget_set_object_name(ptr_, toUtf8(name).constData());
}

void NativeWidget::setTooltip(const QString &tip) const
{
    i2p_widget_set_tooltip(ptr_, toUtf8(tip).constData());
}

void installRoundedTooltips()
{
    i2p_install_rounded_tooltips();
}

void applyTooltipPalette(const QString &theme)
{
    i2p_apply_tooltip_palette(toUtf8(theme).constData());
}

void applyWindowMaterial(QWidget *widget, const QString &theme)
{
    i2p_apply_window_material(widget, theme == QStringLiteral("light") ? 0 : 1);
}

void applyAppFont()
{
    const QString fontsDir = resourceFontsDir();
    i2p_apply_app_font(13,
                       toUtf8(fontsDir).constData(),
                       toUtf8(language()).constData());
}

void setLabelText(quintptr ptr, const QString &text)
{
    if (ptr == 0) {
        return;
    }
    i2p_label_set_text(reinterpret_cast<void *>(ptr), toUtf8(text).constData());
}

void setObjectNamePtr(quintptr ptr, const QString &name)
{
    if (ptr == 0) {
        return;
    }
    i2p_widget_set_object_name(reinterpret_cast<void *>(ptr), toUtf8(name).constData());
}

void setCursorPtr(quintptr ptr, int shape)
{
    if (ptr == 0) {
        return;
    }
    i2p_widget_set_cursor(reinterpret_cast<void *>(ptr), shape);
}

void setPlaceholderPtr(quintptr ptr, const QString &text)
{
    if (ptr == 0) {
        return;
    }
    i2p_line_edit_set_placeholder(reinterpret_cast<void *>(ptr), toUtf8(text).constData());
}

void setButtonText(quintptr ptr, const QString &text)
{
    if (ptr == 0) {
        return;
    }
    i2p_push_button_set_text(reinterpret_cast<void *>(ptr), toUtf8(text).constData());
}

void setCheckable(QWidget *widget, bool checkable)
{
    i2p_push_button_set_checkable(widget, checkable ? 1 : 0);
    i2p_push_button_set_auto_exclusive(widget, checkable ? 1 : 0);
}

void setChecked(quintptr ptr, bool checked)
{
    if (ptr == 0) {
        return;
    }
    i2p_push_button_set_checked(reinterpret_cast<void *>(ptr), checked ? 1 : 0);
    i2p_widget_repolish(reinterpret_cast<void *>(ptr));
}

QString lineEditText(quintptr ptr)
{
    if (ptr == 0) {
        return {};
    }
    const char *raw = i2p_line_edit_text(reinterpret_cast<void *>(ptr));
    return raw ? QString::fromUtf8(raw) : QString{};
}

void overlaySetWidget(quintptr scrollPtr, QWidget *child)
{
    i2p_overlay_scroll_set_widget(reinterpret_cast<void *>(scrollPtr), child);
}

void overlayApplyTheme(quintptr scrollPtr, const QString &theme)
{
    i2p_overlay_scroll_apply_theme(reinterpret_cast<void *>(scrollPtr), toUtf8(theme).constData());
}

void onClick(QWidget *widget, std::function<void()> callback)
{
    i2p_widget_on_click(widget, voidTrampoline, leakVoid(std::move(callback)));
}

void defer(std::function<void()> callback)
{
    i2p_defer(voidTrampoline, leakVoid(std::move(callback)));
}

void addShortcut(QWidget *parent, const QString &key, std::function<void()> callback)
{
    i2p_shortcut_new(parent, toUtf8(key).constData(), voidTrampoline, leakVoid(std::move(callback)));
}

std::optional<QString> openFile(QWidget *parent, const QString &title, const QString &filter)
{
    const char *raw =
        i2p_open_file(parent, toUtf8(title).constData(), toUtf8(filter).constData());
    if (raw == nullptr || raw[0] == '\0') {
        return std::nullopt;
    }
    return QString::fromUtf8(raw);
}

void aboutExec(QWidget *parent, const QString &stylesheet)
{
    const QString body = trArgs(QStringLiteral("about_body"),
                                {{QStringLiteral("version"), appVersion()},
                                 {QStringLiteral("author"), APP_AUTHOR},
                                 {QStringLiteral("license"), APP_LICENSE}});
    const QString qr = resourcePath(QStringLiteral("ton_donation_qr.png"));
    Utf8Holder utf8;
    i2p_about_in input{
        utf8.add(stylesheet),
        utf8.add(trKey(QStringLiteral("about_title"))),
        utf8.add(APP_NAME),
        utf8.add(body),
        utf8.add(trKey(QStringLiteral("github"))),
        APP_GITHUB,
        utf8.add(trKey(QStringLiteral("donate_gram"))),
        APP_TON_ADDRESS,
        utf8.add(qr),
        utf8.add(trKey(QStringLiteral("ok"))),
    };
    i2p_about_exec(parent, &input);
}

std::optional<SettingsResult> settingsExec(QWidget *parent,
                                           const QString &stylesheet,
                                           const AppSettings &settings)
{
    Utf8Holder utf8;
    i2p_settings_in input{
        utf8.add(stylesheet),
        utf8.add(trKey(QStringLiteral("settings_title"))),
        utf8.add(trKey(QStringLiteral("rpc_address"))),
        utf8.add(settings.rpcUrl),
        utf8.addLiteral("http://127.0.0.1:9191/mytorrents"),
        utf8.add(trKey(QStringLiteral("rpc_setup_tip"))),
        utf8.add(trKey(QStringLiteral("torrents_directory"))),
        utf8.add(settings.torrentsDir),
        utf8.add(trKey(QStringLiteral("browse"))),
        utf8.add(trKey(QStringLiteral("refresh_interval"))),
        utf8.add(trKey(QStringLiteral("seconds_suffix"))),
        static_cast<int>(std::clamp(settings.refreshSeconds, quint32(2), quint32(60))),
        utf8.add(trKey(QStringLiteral("language"))),
        utf8.add(trKey(QStringLiteral("language_name_en"))),
        utf8.add(trKey(QStringLiteral("language_name_ru"))),
        utf8.add(settings.language),
        utf8.add(trKey(QStringLiteral("theme"))),
        utf8.add(trKey(QStringLiteral("theme_light"))),
        utf8.add(trKey(QStringLiteral("theme_night"))),
        utf8.add(settings.theme),
        utf8.add(trKey(QStringLiteral("torrent_view"))),
        utf8.add(trKey(QStringLiteral("view_simple"))),
        utf8.add(trKey(QStringLiteral("view_detailed"))),
        utf8.add(settings.torrentView),
        utf8.add(trKey(QStringLiteral("settings_note"))),
        utf8.add(trKey(QStringLiteral("save"))),
        utf8.add(trKey(QStringLiteral("cancel"))),
    };
    if (i2p_settings_exec(parent, &input) == 0) {
        return std::nullopt;
    }
    SettingsResult result;
    result.rpcUrl = QString::fromUtf8(i2p_settings_rpc());
    result.torrentsDir = QString::fromUtf8(i2p_settings_dir());
    result.refreshSeconds =
        static_cast<quint32>(std::clamp(i2p_settings_refresh(), 2, 60));
    result.language = QString::fromUtf8(i2p_settings_language());
    result.theme = QString::fromUtf8(i2p_settings_theme());
    result.torrentView = QString::fromUtf8(i2p_settings_view());
    return result;
}

void filesExec(QWidget *parent,
               const QString &stylesheet,
               const QString &title,
               const QVector<TorrentFile> &files,
               const std::function<int(int, int, int)> &onChange)
{
    std::vector<QByteArray> names;
    std::vector<QByteArray> fullNames;
    std::vector<QByteArray> sizes;
    std::vector<QByteArray> progresses;
    std::vector<i2p_file_row> rows;
    names.reserve(files.size());
    fullNames.reserve(files.size());
    sizes.reserve(files.size());
    progresses.reserve(files.size());
    rows.reserve(files.size());
    for (const TorrentFile &file : files) {
        names.push_back(toUtf8(file.displayName()));
        fullNames.push_back(toUtf8(file.name));
        sizes.push_back(toUtf8(formatBytes(file.length)));
        progresses.push_back(toUtf8(file.progressLabel()));
        rows.push_back(i2p_file_row{
            static_cast<int>(file.index),
            names.back().constData(),
            fullNames.back().constData(),
            sizes.back().constData(),
            progresses.back().constData(),
            file.wanted ? 1 : 0,
            static_cast<int>(file.priority),
        });
    }
    Utf8Holder utf8;
    i2p_files_in input{
        utf8.add(stylesheet),
        utf8.add(title),
        utf8.add(trKey(QStringLiteral("files_note"))),
        utf8.add(trKey(QStringLiteral("files_set_unsupported"))),
        utf8.add(trKey(QStringLiteral("files_empty"))),
        utf8.add(trKey(QStringLiteral("close"))),
        utf8.add(trKey(QStringLiteral("files_name"))),
        utf8.add(trKey(QStringLiteral("files_size"))),
        utf8.add(trKey(QStringLiteral("files_progress"))),
        utf8.add(trKey(QStringLiteral("files_priority"))),
        utf8.add(trKey(QStringLiteral("priority_skip"))),
        utf8.add(trKey(QStringLiteral("priority_low"))),
        utf8.add(trKey(QStringLiteral("priority_normal"))),
        utf8.add(trKey(QStringLiteral("priority_high"))),
        rows.data(),
        static_cast<int>(rows.size()),
    };
    i2p_files_exec(parent, &input, fileChangeTrampoline, leakFileChange(onChange));
}

std::optional<bool> confirmRemove(QWidget *parent,
                                    const QString &title,
                                    const QString &text,
                                    const QString &checkbox,
                                    const QString &yesLabel,
                                    const QString &cancelLabel)
{
    Utf8Holder utf8;
    int deleteData = 0;
    const int accepted = i2p_confirm_remove(parent,
                                            utf8.add(title),
                                            utf8.add(text),
                                            utf8.add(checkbox),
                                            utf8.add(yesLabel),
                                            utf8.add(cancelLabel),
                                            &deleteData);
    if (accepted == 0) {
        return std::nullopt;
    }
    return deleteData != 0;
}

void popupAddAction(void *popup, const QString &text, bool enabled, std::function<void()> callback)
{
    i2p_actions_popup_add_action(popup,
                                 toUtf8(text).constData(),
                                 enabled ? 1 : 0,
                                 voidTrampoline,
                                 leakVoid(std::move(callback)));
}

void popupSeparator(void *popup)
{
    i2p_actions_popup_add_separator(popup);
}

void showPopupBelow(QWidget *parent,
                    quintptr anchorPtr,
                    const QString &theme,
                    const std::function<void(void *popup)> &build)
{
    void *popup = i2p_actions_popup_new(parent);
    i2p_actions_popup_apply_theme(popup, toUtf8(theme).constData());
    build(popup);
    i2p_actions_popup_show_below(popup, reinterpret_cast<void *>(anchorPtr));
}

} // namespace i2p
