#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "qt_chrome.h"
#ifdef __APPLE__
#include "macos_vibrancy.h"
#endif

#include "torrent_create.hpp"

#include <QAbstractItemView>
#include <QAbstractAnimation>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QEnterEvent>
#include <QFileDialog>
#include <QEasingCurve>
#include <QEvent>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <climits>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QPropertyAnimation>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRectF>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QShowEvent>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QStyleFactory>
#include <QStyledItemDelegate>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QProgressBar>
#include <QThread>
#include <QEventLoop>
#include <QTextLine>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QUrl>
#include <QVariant>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

QWidget *as_widget(void *ptr) { return static_cast<QWidget *>(ptr); }

QString qstr(const char *text) { return QString::fromUtf8(text ? text : ""); }

void install_app_shutdown_helper(QWidget *main_window);
void dismiss_transient_windows(QWidget *keep);
void shutdown_app_chrome();

void update_popup_rounded_mask(QWidget *widget, qreal radius);
void apply_widget_rounded_mask(QWidget *widget, qreal radius);

#ifdef Q_OS_WIN
bool is_main_app_window(QWidget *widget);
void install_windows_glass(QWidget *host, bool night, int alpha, const QColor &lightTint = QColor());
void install_windows_sidebar_glass(QWidget *host, bool night);
void install_windows_surface_glass(QWidget *host, bool night);
void apply_windows_dialog_chrome(QDialog *dialog, bool night);
bool apply_windows_main_acrylic(HWND hwnd);
#endif

Qt::WindowFlags hosted_dialog_flags() {
    Qt::WindowFlags flags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                            Qt::WindowCloseButtonHint;
#ifdef Q_OS_MAC
    flags |= Qt::Window;
#endif
    return flags;
}

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif

void apply_opaque_dialog_chrome(QDialog *dialog, bool night);

#ifdef Q_OS_LINUX
constexpr int kLinuxDialogGlassAlpha = 200;

QColor linux_dialog_glass(bool night) {
    return night ? QColor(0x1c, 0x1c, 0x1e, kLinuxDialogGlassAlpha)
                 : QColor(0xff, 0xff, 0xff, kLinuxDialogGlassAlpha);
}

class LinuxGlassBackdrop final : public QWidget {
public:
    LinuxGlassBackdrop(QWidget *parent, bool night, int alpha)
        : QWidget(parent), fill_(night ? QColor(0x1c, 0x1c, 0x1e, alpha) : QColor(0xff, 0xff, 0xff, alpha)) {
        setObjectName(QStringLiteral("LinuxGlassBackdrop"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        if (parent != nullptr) {
            setGeometry(parent->rect());
            lower();
            parent->installEventFilter(this);
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(rect(), fill_);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
            lower();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    QColor fill_;
};

void install_linux_glass(QWidget *host, bool night, int alpha) {
    if (host == nullptr) {
        return;
    }
    host->setAttribute(Qt::WA_StyledBackground, true);
    if (auto *existing = host->findChild<QWidget *>(QStringLiteral("LinuxGlassBackdrop"))) {
        existing->deleteLater();
    }
    auto *backdrop = new LinuxGlassBackdrop(host, night, alpha);
    backdrop->show();
    backdrop->lower();
    QTimer::singleShot(0, backdrop, [backdrop, host]() {
        if (backdrop != nullptr && host != nullptr) {
            backdrop->setGeometry(host->rect());
            backdrop->lower();
        }
    });
}

void apply_linux_window_chrome(QWidget *widget, bool night) {
    if (widget == nullptr) {
        return;
    }
    const bool dialog = qobject_cast<QDialog *>(widget) != nullptr;
    install_linux_glass(widget, night, dialog ? kLinuxDialogGlassAlpha : 185);
    for (QWidget *child : widget->findChildren<QWidget *>()) {
        if (child->objectName() == QLatin1String("Sidebar")) {
            child->setAttribute(Qt::WA_TranslucentBackground, false);
            child->setAutoFillBackground(false);
            install_linux_glass(child, night, 220);
        }
    }
}
#endif

void apply_window_material(QWidget *widget, bool night) {
    if (widget == nullptr) {
        return;
    }
#ifdef Q_OS_WIN
    const bool main_window = is_main_app_window(widget);
    if (main_window) {
        widget->setAttribute(Qt::WA_TranslucentBackground, true);
        widget->setAutoFillBackground(false);
    } else {
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
        widget->setAutoFillBackground(true);
        QPalette root_palette = widget->palette();
        root_palette.setColor(QPalette::Window, night ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff));
        widget->setPalette(root_palette);
    }
#else
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    widget->setAutoFillBackground(false);
#endif
    (void)widget->winId();
#ifdef __APPLE__
    const bool dialog = qobject_cast<QDialog *>(widget) != nullptr;
    widget->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, dialog);
    i2p_macos_nsview_apply_vibrancy(reinterpret_cast<void *>(widget->winId()), night ? 1 : 0,
                                    dialog ? 1 : 0);
#endif
    bool has_panes = false;
    for (QWidget *child : widget->findChildren<QWidget *>()) {
        const QString name = child->objectName();
        if (name == QLatin1String("Sidebar")) {
            has_panes = true;
            child->setAttribute(Qt::WA_StyledBackground, true);
#ifdef Q_OS_LINUX
            child->setAttribute(Qt::WA_TranslucentBackground, false);
            child->setAutoFillBackground(false);
#elif defined(Q_OS_WIN)
            if (main_window) {
                child->setAttribute(Qt::WA_TranslucentBackground, true);
                child->setAutoFillBackground(false);
            } else {
                child->setAttribute(Qt::WA_TranslucentBackground, false);
                child->setAutoFillBackground(true);
                QPalette palette = child->palette();
                palette.setColor(QPalette::Window, night ? QColor(0x2c, 0x2c, 0x2e) : QColor(0xf2, 0xf2, 0xf7));
                child->setPalette(palette);
            }
#else
            child->setAttribute(Qt::WA_TranslucentBackground, true);
            child->setAutoFillBackground(false);
#endif
        } else if (name == QLatin1String("Surface")) {
            has_panes = true;
            child->setAttribute(Qt::WA_StyledBackground, true);
#ifdef Q_OS_WIN
            if (main_window) {
                child->setAttribute(Qt::WA_TranslucentBackground, true);
                child->setAutoFillBackground(false);
            } else {
                child->setAttribute(Qt::WA_TranslucentBackground, false);
                child->setAutoFillBackground(true);
                QPalette palette = child->palette();
                palette.setColor(QPalette::Window, night ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff));
                child->setPalette(palette);
            }
#else
            child->setAttribute(Qt::WA_TranslucentBackground, false);
            child->setAutoFillBackground(true);
            QPalette palette = child->palette();
            palette.setColor(QPalette::Window, night ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff));
            child->setPalette(palette);
#endif
        }
    }
    if (has_panes) {
        if (QLayout *layout = widget->layout()) {
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
        }
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (hwnd != nullptr) {
        const BOOL dark = night ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        if (main_window) {
            (void)apply_windows_main_acrylic(hwnd);
        }
    }
    if (main_window) {
        QPointer<QWidget> root = widget;
        QTimer::singleShot(0, widget, [root, night]() {
            if (root == nullptr) {
                return;
            }
            for (QWidget *child : root->findChildren<QWidget *>()) {
                const QString name = child->objectName();
                if (name == QLatin1String("Sidebar")) {
                    install_windows_sidebar_glass(child, night);
                } else if (name == QLatin1String("Surface")) {
                    install_windows_surface_glass(child, night);
                }
            }
        });
    }
#elif defined(Q_OS_LINUX)
    apply_linux_window_chrome(widget, night);
#else
    Q_UNUSED(night);
#endif
    if (qobject_cast<QDialog *>(widget) == nullptr && widget->isWindow()) {
        install_app_shutdown_helper(widget);
    }
}

void prepare_hosted_dialog(QDialog *dialog) {
    if (dialog == nullptr) {
        return;
    }
    dialog->setModal(true);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setSizeGripEnabled(false);
#ifdef Q_OS_WIN
    dialog->setAttribute(Qt::WA_TranslucentBackground, true);
    dialog->setAutoFillBackground(false);
#else
    dialog->setAttribute(Qt::WA_TranslucentBackground, true);
    dialog->setAutoFillBackground(false);
#endif
}

bool stylesheet_is_night(const QString &css) {
    return css.contains(QLatin1String("QFrame#Surface, QWidget#Surface {"))
        && css.contains(QLatin1String("background: #1c1c1e"));
}

void apply_opaque_dialog_chrome(QDialog *dialog, bool night) {
    if (dialog == nullptr) {
        return;
    }
#ifdef Q_OS_WIN
    apply_windows_dialog_chrome(dialog, night);
    return;
#endif
    dialog->setAttribute(Qt::WA_TranslucentBackground, false);
    dialog->setAutoFillBackground(true);
    QPalette pal = dialog->palette();
    pal.setColor(QPalette::Window, night ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff));
    dialog->setPalette(pal);
#ifdef Q_OS_LINUX
    if (auto *existing = dialog->findChild<QWidget *>(QStringLiteral("LinuxGlassBackdrop"))) {
        existing->deleteLater();
    }
#endif
#ifdef Q_OS_MAC
    (void)dialog->winId();
    i2p_macos_nsview_apply_opaque_dialog(reinterpret_cast<void *>(dialog->winId()), night ? 1 : 0);
#endif
}

void apply_hosted_dialog_surface(QDialog *dialog) {
    if (dialog == nullptr) {
        return;
    }
    dialog->ensurePolished();
    const bool night = stylesheet_is_night(dialog->styleSheet());
#ifdef Q_OS_WIN
    apply_windows_dialog_chrome(dialog, night);
    return;
#endif
#ifdef Q_OS_MAC
    if (dialog->property("i2pOpaqueChrome").toBool()) {
        apply_opaque_dialog_chrome(dialog, night);
        return;
    }
#endif
    apply_window_material(dialog, night);
}

void apply_real_dialog_window(QDialog *dialog) {
    if (dialog == nullptr) {
        return;
    }
    Qt::WindowFlags flags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                            Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint;
#ifdef Q_OS_MAC
    flags |= Qt::Window;
#endif
    dialog->setWindowFlags(flags);
    dialog->setModal(true);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setSizeGripEnabled(false);
    dialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
}

struct DialogMetrics {
    static constexpr int kAboutW = 460;
    static constexpr int kSettingsW = 520;
    static constexpr int kCreateW = 540;
    static constexpr int kSettingsH = 520;
    static constexpr int kFilesW = 640;
    static constexpr int kPeersW = 640;
    static constexpr int kPeersMaxW = 960;
    static constexpr int kFilesH = 460;
    static constexpr int kButtonRowTop = 8;
    static constexpr int kButtonGap = 8;
    static constexpr int kControlMinH = 36;
    static constexpr int kQrSide = 160;
    static constexpr int kFilesHeaderMinH = 32;
    static constexpr int kFilesTableEdge = 0;
    static constexpr qreal kFilesTableRadius = 12;
    static constexpr int kFilesProgressPad = 20;
    static constexpr int kFilesPriorityCellPad = 8;
    static constexpr int kFilesPriorityRightPad = 16;
    static constexpr int kFilesScrollReserve = 14;
    static constexpr int kFilesScrollEdge = 3;

    static QMargins dialog_margins(int top = 16, int bottom = 16) {
        return QMargins(18, top, 18, bottom);
    }

    static int inner_w(int dialog_w, const QMargins &margins) {
        return dialog_w - margins.left() - margins.right();
    }
};

void lock_dialog_control(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setFixedHeight(DialogMetrics::kControlMinH);
    QSizePolicy policy = widget->sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Fixed);
    widget->setSizePolicy(policy);
#ifdef Q_OS_MAC
    // macOS style SE_LayoutItem insets QLineEdit vs QPushButton differently.
    widget->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
#endif
}

void polish_line_edit(QLineEdit *edit) {
    if (edit == nullptr) {
        return;
    }
    edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    edit->setTextMargins(4, 0, 4, 0);
    edit->setFrame(false);
    lock_dialog_control(edit);
#ifdef Q_OS_MAC
    edit->setAttribute(Qt::WA_MacNormalSize, true);
#endif
}

int wrapped_label_height(const QLabel *label, int width) {
    if (label == nullptr || width <= 0) {
        return 0;
    }
    if (label->textFormat() == Qt::RichText ||
        (label->textFormat() == Qt::AutoText && label->text().contains(QLatin1Char('<')))) {
        QTextDocument doc;
        doc.setDefaultFont(label->font());
        doc.setHtml(label->text());
        doc.setTextWidth(width);
        return static_cast<int>(std::ceil(doc.size().height())) + label->fontMetrics().descent() + 2;
    }
    const QFontMetrics metrics(label->font());
    return metrics.boundingRect(QRect(0, 0, width, INT_MAX), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, label->text())
               .height() +
           metrics.descent() + 2;
}

void lock_wrapped_label(QLabel *label, int width) {
    if (label == nullptr || width <= 0) {
        return;
    }
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    label->setFixedWidth(width);
    int height = label->heightForWidth(width);
    if (height <= 0) {
        height = wrapped_label_height(label, width);
    } else {
        height += label->fontMetrics().descent() + 2;
    }
    if (height > 0) {
        label->setMinimumHeight(height);
        label->setFixedHeight(height);
    }
}

void add_dialog_buttons(QVBoxLayout *layout, std::initializer_list<QPushButton *> buttons) {
    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, DialogMetrics::kButtonRowTop, 0, 0);
    row->setSpacing(DialogMetrics::kButtonGap);
    row->addStretch(1);
    for (QPushButton *button : buttons) {
        row->addWidget(button);
    }
    layout->addLayout(row);
}

int dialog_buttons_row_height(const QPushButton *button) {
    if (button == nullptr) {
        return DialogMetrics::kButtonRowTop;
    }
    return DialogMetrics::kButtonRowTop + button->sizeHint().height();
}

bool is_table_descendant(const QWidget *widget) {
    for (const QWidget *parent = widget != nullptr ? widget->parentWidget() : nullptr; parent != nullptr;
         parent = parent->parentWidget()) {
        if (qobject_cast<const QTableWidget *>(parent) != nullptr) {
            return true;
        }
    }
    return false;
}

#ifdef Q_OS_MAC
void clip_macos_control(QWidget *widget, qreal radius, unsigned int fill_rgb) {
    if (widget == nullptr) {
        return;
    }
    widget->setAttribute(Qt::WA_NativeWindow, true);
    i2p_macos_nsview_clip_control(reinterpret_cast<void *>(widget->winId()), radius, fill_rgb);
}
#endif

void polish_dialog_tree(QDialog *dialog) {
    if (dialog == nullptr) {
        return;
    }
    dialog->ensurePolished();
    for (QWidget *child : dialog->findChildren<QWidget *>()) {
        child->ensurePolished();
    }
}

void measure_and_lock_dialog(QDialog *dialog, int wrapped_inner_w) {
    if (dialog == nullptr) {
        return;
    }
    polish_dialog_tree(dialog);
    for (QLabel *label : dialog->findChildren<QLabel *>()) {
        if (is_table_descendant(label) || !label->wordWrap()) {
            continue;
        }
        label->setFixedWidth(wrapped_inner_w);
    }
    for (QLabel *label : dialog->findChildren<QLabel *>()) {
        if (is_table_descendant(label)) {
            continue;
        }
        if (label->wordWrap()) {
            lock_wrapped_label(label, wrapped_inner_w);
            continue;
        }
        if (const QPixmap pixmap = label->pixmap(); !pixmap.isNull()) {
            label->setFixedSize(label->sizeHint());
            continue;
        }
        label->setFixedHeight(label->sizeHint().height());
    }
    for (QPushButton *button : dialog->findChildren<QPushButton *>()) {
        if (is_table_descendant(button)) {
            continue;
        }
        const int width = std::max(button->sizeHint().width(), button->minimumSizeHint().width());
        button->setFixedSize(width, DialogMetrics::kControlMinH);
    }
    for (QComboBox *combo : dialog->findChildren<QComboBox *>()) {
        if (is_table_descendant(combo)) {
            continue;
        }
        combo->adjustSize();
        lock_dialog_control(combo);
    }
    for (QAbstractSpinBox *spin : dialog->findChildren<QAbstractSpinBox *>()) {
        if (spin->parentWidget() != nullptr &&
            spin->parentWidget()->objectName() == QLatin1String("SpinRow")) {
            spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            continue;
        }
        spin->setFixedHeight(DialogMetrics::kControlMinH);
    }
    for (QLineEdit *edit : dialog->findChildren<QLineEdit *>()) {
        polish_line_edit(edit);
        lock_dialog_control(edit);
    }
}

void layout_files_table(QTableWidget *table, QVBoxLayout *layout, int dialog_w, int dialog_h, QLabel *note,
                        int button_row_h, const QString &progress_header);

QSize modal_dialog_size(QDialog *dialog, const QSize &fixed_size) {
    if (dialog == nullptr) {
        return fixed_size;
    }
    QLayout *layout = dialog->layout();
    if (layout == nullptr) {
        return fixed_size;
    }
    layout->activate();
    const QSize content = layout->sizeHint().expandedTo(layout->minimumSize());
    const int width = fixed_size.isValid() && fixed_size.width() > 0
                          ? fixed_size.width()
                          : std::max(dialog->minimumWidth(), content.width());
    const int height =
        fixed_size.isValid() && fixed_size.height() > 0 ? fixed_size.height() : content.height();
    return QSize(width, height);
}

void restore_ui_after_modal(QWidget *host) {
    if (host != nullptr) {
        host->setEnabled(true);
    }
    if (QApplication *app = qApp) {
        for (QWidget *widget : app->topLevelWidgets()) {
            if (widget == nullptr || qobject_cast<QDialog *>(widget) != nullptr) {
                continue;
            }
            widget->setEnabled(true);
        }
        if (host != nullptr) {
            host->raise();
            host->activateWindow();
        }
    }
}

void exec_app_modal_dialog(QDialog *dialog, QWidget *host, int wrapped_inner_w, const QSize &fixed_size,
                            const std::function<void()> &finalize = {}) {
    if (dialog == nullptr) {
        return;
    }

    const bool size_already_locked =
        fixed_size.isValid() && fixed_size.width() > 0 && fixed_size.height() > 0;
    if (!size_already_locked) {
        measure_and_lock_dialog(dialog, wrapped_inner_w);
    }
    if (finalize) {
        finalize();
    }
    apply_hosted_dialog_surface(dialog);
    if (!size_already_locked) {
        QSize size = modal_dialog_size(dialog, fixed_size);
        if (size.width() <= 0 || size.height() <= 0) {
            size = dialog->sizeHint().expandedTo(QSize(240, 120));
        }
        dialog->setFixedSize(size);
    }
    if (host != nullptr) {
        (void)dialog->winId();
        (void)host->winId();
        if (QWindow *dialog_win = dialog->windowHandle()) {
            if (QWindow *host_win = host->windowHandle()) {
                dialog_win->setTransientParent(host_win);
            }
        }
    }
    dialog->exec();
    restore_ui_after_modal(host);
}

#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

void disable_dwm_rounded_frame(QWidget *widget) {
#ifdef Q_OS_WIN
    if (widget == nullptr) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (hwnd == nullptr) {
        return;
    }
    const int pref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
#else
    Q_UNUSED(widget);
#endif
}

#ifdef Q_OS_WIN
enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
};

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attribute;
    PVOID Data;
    SIZE_T SizeOfData;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI *)(HWND, WINDOWCOMPOSITIONATTRIBDATA *);

SetWindowCompositionAttributeFn window_composition_attribute_fn() {
    static const SetWindowCompositionAttributeFn fn = reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    return fn;
}

void disable_windows_popup_acrylic(HWND hwnd) {
    const SetWindowCompositionAttributeFn apply = window_composition_attribute_fn();
    if (apply == nullptr || hwnd == nullptr) {
        return;
    }
    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_DISABLED;
    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);
    apply(hwnd, &data);
}

bool apply_windows_main_acrylic(HWND hwnd) {
    const SetWindowCompositionAttributeFn apply = window_composition_attribute_fn();
    if (apply == nullptr || hwnd == nullptr) {
        return false;
    }
    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    policy.AccentFlags = 0;
    policy.GradientColor = 0;
    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);
    if (apply(hwnd, &data) == FALSE) {
        policy.AccentState = ACCENT_ENABLE_BLURBEHIND;
        return apply(hwnd, &data) != FALSE;
    }
    return true;
}

class WindowsGlassBackdrop final : public QWidget {
public:
    WindowsGlassBackdrop(QWidget *parent, bool night, int alpha, const QColor &lightTint = QColor())
        : QWidget(parent) {
        setObjectName(QStringLiteral("WindowsGlassBackdrop"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setAppearance(night, alpha, lightTint);
        if (parent != nullptr) {
            setGeometry(parent->rect());
            lower();
            parent->installEventFilter(this);
        }
    }

    ~WindowsGlassBackdrop() override {
        if (QWidget *parent = parentWidget()) {
            parent->removeEventFilter(this);
        }
    }

    void setAppearance(bool night, int alpha, const QColor &tint = QColor()) {
        const QColor base = tint.isValid() ? tint : (night ? QColor(0x2c, 0x2c, 0x2e) : QColor(0xf2, 0xf2, 0xf7));
        fill_ = QColor(base.red(), base.green(), base.blue(), alpha);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), fill_);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        QWidget *parent = parentWidget();
        if (parent == nullptr || watched != parent || event->type() != QEvent::Resize) {
            return QWidget::eventFilter(watched, event);
        }
        setGeometry(parent->rect());
        lower();
        return QWidget::eventFilter(watched, event);
    }

private:
    QColor fill_;
};

void install_windows_glass(QWidget *host, bool night, int alpha, const QColor &tint) {
    if (host == nullptr) {
        return;
    }
    host->setAttribute(Qt::WA_StyledBackground, true);
    if (auto *legacy = host->findChild<QWidget *>(QStringLiteral("WindowsSidebarGlass"))) {
        host->removeEventFilter(legacy);
        delete legacy;
    }
    auto *backdrop_widget = host->findChild<QWidget *>(QStringLiteral("WindowsGlassBackdrop"));
    if (backdrop_widget == nullptr) {
        auto *backdrop = new WindowsGlassBackdrop(host, night, alpha, tint);
        backdrop->show();
        backdrop_widget = backdrop;
    } else {
        static_cast<WindowsGlassBackdrop *>(backdrop_widget)->setAppearance(night, alpha, tint);
    }
    backdrop_widget->setGeometry(host->rect());
    backdrop_widget->lower();
}

void install_windows_sidebar_glass(QWidget *host, bool night) {
    constexpr int kSidebarGlassAlpha = 205;
    install_windows_glass(host, night, kSidebarGlassAlpha);
}

void install_windows_surface_glass(QWidget *host, bool night) {
    constexpr int kSurfaceGlassAlpha = 245;
    if (night) {
        install_windows_glass(host, true, kSurfaceGlassAlpha, QColor(0x1c, 0x1c, 0x1e));
    } else {
        install_windows_glass(host, false, kSurfaceGlassAlpha, QColor(0xff, 0xff, 0xff));
    }
}

void apply_windows_dialog_chrome(QDialog *dialog, bool night) {
    if (dialog == nullptr) {
        return;
    }
    dialog->setAttribute(Qt::WA_TranslucentBackground, true);
    dialog->setAutoFillBackground(false);
    (void)dialog->winId();
    const HWND hwnd = reinterpret_cast<HWND>(dialog->winId());
    if (hwnd != nullptr) {
        const BOOL dark = night ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        (void)apply_windows_main_acrylic(hwnd);
    }
    constexpr int kDialogGlassAlpha = 200;
    if (night) {
        install_windows_glass(dialog, true, kDialogGlassAlpha);
    } else {
        install_windows_glass(dialog, false, kDialogGlassAlpha, QColor(0xe4, 0xe4, 0xe9));
    }
}

bool is_main_app_window(QWidget *widget) {
    return widget != nullptr && widget->isWindow() && qobject_cast<QDialog *>(widget) == nullptr &&
           widget->objectName() == QLatin1String("MainWindow");
}

void clear_windows_popup_hwnd_region(HWND hwnd) {
    if (hwnd != nullptr) {
        SetWindowRgn(hwnd, nullptr, TRUE);
    }
}

void prepare_windows_popup_chrome(QWidget *widget, qreal radius) {
    if (widget == nullptr) {
        return;
    }
    (void)widget->winId();
    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (hwnd == nullptr) {
        return;
    }
    disable_dwm_rounded_frame(widget);
    disable_windows_popup_acrylic(hwnd);
    clear_windows_popup_hwnd_region(hwnd);
    update_popup_rounded_mask(widget, radius);
}
#endif

Qt::WindowFlags popup_window_flags() {
    Qt::WindowFlags flags = Qt::Popup | Qt::FramelessWindowHint;
#ifdef Q_OS_WIN
    flags |= Qt::NoDropShadowWindowHint;
#endif
    return flags;
}

bool platform_supports_window_opacity() {
#if defined(Q_OS_LINUX) || (defined(Q_OS_UNIX) && !defined(Q_OS_MAC) && !defined(Q_OS_WIN))
    return false;
#else
    return true;
#endif
}

QWidget *top_level_window_for(QWidget *widget) {
    if (widget == nullptr) {
        return nullptr;
    }
    if (QWidget *window = widget->window()) {
        return window;
    }
    return widget->isWindow() ? widget : nullptr;
}

QWidget *main_app_window() {
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets) {
        if (widget != nullptr && widget->isWindow() && qobject_cast<QDialog *>(widget) == nullptr &&
            widget->objectName() == QLatin1String("MainWindow")) {
            return widget;
        }
    }
    return nullptr;
}

void set_popup_transient_parent(QWidget *popup, QWidget *anchor) {
    if (popup == nullptr) {
        return;
    }
    (void)popup->winId();
    QWindow *popup_win = popup->windowHandle();
    if (popup_win == nullptr) {
        return;
    }
    QWidget *host = top_level_window_for(anchor);
    if (host == nullptr) {
        host = main_app_window();
    }
    if (host == nullptr) {
        return;
    }
    (void)host->winId();
    if (QWindow *host_win = host->windowHandle()) {
        popup_win->setTransientParent(host_win);
    }
}

void paint_popup_rounded_bg(QWidget *widget, const QColor &bg, const QColor &border, qreal radius) {
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(widget->rect(), QColor(0, 0, 0, 0));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QRectF rect = QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(bg);
    painter.drawRoundedRect(rect, radius, radius);
}

void apply_widget_rounded_mask(QWidget *widget, qreal radius) {
#ifdef Q_OS_MAC
    Q_UNUSED(radius);
    if (widget != nullptr) {
        widget->clearMask();
    }
    return;
#else
    if (widget == nullptr) {
        return;
    }
    const int width = widget->width();
    const int height = widget->height();
    if (width < 2 || height < 2) {
        return;
    }
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::color0);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::color1);
        painter.drawRoundedRect(QRectF(0, 0, width, height), radius, radius);
    }
    widget->setMask(QRegion(pixmap.mask()));
#endif
}

void update_popup_rounded_mask(QWidget *widget, qreal radius) {
#ifdef Q_OS_MAC
    Q_UNUSED(radius);
    widget->clearMask();
    return;
#else
    const int width = widget->width();
    const int height = widget->height();
    if (width < 2 || height < 2) {
        return;
    }
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::color0);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::color1);
        painter.drawRoundedRect(QRectF(0, 0, width, height), radius, radius);
    }
    const QRegion region = QRegion(pixmap.mask());
    widget->setMask(region);
    if (QWindow *win = widget->windowHandle()) {
        win->setMask(region);
    }
#endif
}

QScreen *popup_screen_for_anchor(QWidget *anchor) {
    if (anchor->width() > 0 && anchor->height() > 0) {
        const QPoint center = anchor->mapToGlobal(QPoint(anchor->width() / 2, anchor->height() / 2));
        if (QScreen *screen = QGuiApplication::screenAt(center)) {
            return screen;
        }
    }
    for (const QPoint &point : {
             anchor->mapToGlobal(QPoint(0, 0)),
             anchor->mapToGlobal(QPoint(std::max(0, anchor->width() - 1), std::max(0, anchor->height() - 1))),
         }) {
        if (QScreen *screen = QGuiApplication::screenAt(point)) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

QPoint clamp_popup_top_left(QPoint top_left, int popup_w, int popup_h, const QRect &geom) {
    const int x = std::max(geom.left(), std::min(top_left.x(), geom.right() - popup_w + 1));
    const int y = std::max(geom.top(), std::min(top_left.y(), geom.bottom() - popup_h + 1));
    return QPoint(x, y);
}

QPoint global_position_popup_below_anchor(QWidget *anchor, int popup_w, int popup_h, int vertical_gap,
                                          bool align_right) {
    const int x_local = align_right ? std::max(0, anchor->width() - popup_w) : 0;
    const QPoint pos_below = anchor->mapToGlobal(QPoint(x_local, anchor->height() + vertical_gap));
    const QPoint pos_above = anchor->mapToGlobal(QPoint(x_local, -popup_h - vertical_gap));
    QScreen *screen = popup_screen_for_anchor(anchor);
    if (screen == nullptr) {
        return pos_below;
    }
    const QRect geom = screen->availableGeometry();
    const QPoint pos = (pos_below.y() > geom.bottom() - popup_h + 1 && pos_above.y() >= geom.top()) ? pos_above
                                                                                                    : pos_below;
    return clamp_popup_top_left(pos, popup_w, popup_h, geom);
}

class TorrentCardWidget final : public QWidget {
public:
    explicit TorrentCardWidget(bool night, QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("TorrentCard"));
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, false);
        setAutoFillBackground(false);
        applyTheme(night);
    }

    void applyTheme(bool night) {
        if (night) {
            bg_ = QColor(QStringLiteral("#2c2c2e"));
        } else {
#ifdef Q_OS_WIN
            bg_ = QColor(QStringLiteral("#e4e4e9"));
#else
            bg_ = QColor(QStringLiteral("#f2f2f7"));
#endif
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(bg_);
        painter.drawRoundedRect(QRectF(rect()), 10.0, 10.0);
    }

private:
    QColor bg_;
};

class PieceMapWidget final : public QWidget {
public:
    explicit PieceMapWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("PieceMap"));
        setFixedHeight(9);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setHave(const uint8_t *have, int len) {
        have_.assign(have, have + std::max(0, len));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        const QRect rect = this->rect().adjusted(0, 0, -1, -1);
        const bool dark = palette().color(QPalette::WindowText).lightness() > 140;
        const QColor background(dark ? "#3b3e48" : "#d6d7dd");
        const QColor have_color(dark ? "#30d158" : "#248a3d");
        const QColor partial(dark ? "#8de5a8" : "#6bcf80");
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(rect, 3, 3);
        if (have_.empty() || rect.width() <= 0) {
            return;
        }
        const int total = static_cast<int>(have_.size());
        const int width = std::max(1, rect.width());
        for (int column = 0; column < width; ++column) {
            const int start = column * total / width;
            const int end = std::max(start + 1, (column + 1) * total / width);
            bool all = true;
            bool any = false;
            for (int i = start; i < end && i < total; ++i) {
                if (have_[static_cast<size_t>(i)]) {
                    any = true;
                } else {
                    all = false;
                }
            }
            if (all) {
                painter.fillRect(rect.x() + column, rect.y(), 1, rect.height(), have_color);
            } else if (any) {
                painter.fillRect(rect.x() + column, rect.y(), 1, rect.height(), partial);
            }
        }
    }

private:
    std::vector<uint8_t> have_;
};

class RoundedVerticalScrollbar final : public QWidget {
public:
    explicit RoundedVerticalScrollbar(QScrollBar *linked, QWidget *parent = nullptr)
        : QWidget(parent), sb_(linked) {
        setFixedWidth(6);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        if (sb_ != nullptr) {
            QObject::connect(sb_, &QScrollBar::valueChanged, this, [this](int) { update(); });
            QObject::connect(sb_, &QScrollBar::rangeChanged, this, [this](int, int) { update(); });
        }
    }

    QScrollBar *linkedBar() const { return sb_.data(); }

    void applyTheme(const QString &theme) {
        if (theme == QLatin1String("night")) {
            thumb_ = QColor(255, 255, 255, 51);
        } else {
            thumb_ = QColor(60, 60, 67, 89);
        }
        track_ = QColor(0, 0, 0, 0);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        const qreal radius = width() / 2.0;
        if (track_.alpha() > 0) {
            painter.setBrush(track_);
            painter.drawRoundedRect(QRectF(rect()), radius, radius);
        }
        painter.setBrush(thumb_);
        painter.drawRoundedRect(computeThumb(), radius, radius);
    }

    void mousePressEvent(QMouseEvent *event) override {
        const QRectF thumb = computeThumb();
        if (thumb.height() <= 0) {
            return;
        }
        dragging_ = true;
        const QPointF pos = event->position();
        drag_offset_y_ = thumb.contains(pos) ? static_cast<int>(pos.y() - thumb.y())
                                             : static_cast<int>(thumb.height() / 2.0);
        const qreal target = std::clamp(pos.y() - drag_offset_y_, 0.0, height() - thumb.height());
        setValueFromThumbY(target, thumb.height());
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!dragging_) {
            return;
        }
        const QRectF thumb = computeThumb();
        if (thumb.height() <= 0) {
            return;
        }
        const qreal target =
            std::clamp(event->position().y() - drag_offset_y_, 0.0, height() - thumb.height());
        setValueFromThumbY(target, thumb.height());
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        dragging_ = false;
        event->accept();
    }

    void wheelEvent(QWheelEvent *event) override { QApplication::sendEvent(sb_, event); }

private:
    QRectF computeThumb() const {
        const int track_h = std::max(0, height());
        if (track_h <= 0 || sb_ == nullptr) {
            return QRectF(0, 0, width(), 0);
        }
        const int sb_min = sb_->minimum();
        const int sb_max = sb_->maximum();
        const int sb_range = std::max(0, sb_max - sb_min);
        const int page = std::max(0, sb_->pageStep());
        const int total = sb_range + page;
        const qreal visible_ratio = total ? static_cast<qreal>(page) / total : 1.0;
        const qreal thumb_h =
            std::max(16.0, std::min(static_cast<qreal>(track_h),
                                    static_cast<qreal>(track_h) * std::clamp(visible_ratio, 0.05, 1.0)));
        const qreal progress =
            sb_range <= 0 ? 0.0
                          : std::clamp(static_cast<qreal>(sb_->value() - sb_min) / sb_range, 0.0, 1.0);
        return QRectF(0.0, (track_h - thumb_h) * progress, width(), thumb_h);
    }

    void setValueFromThumbY(qreal thumb_y, qreal thumb_h) {
        if (sb_ == nullptr) {
            return;
        }
        const int sb_min = sb_->minimum();
        const int sb_max = sb_->maximum();
        const int sb_range = std::max(0, sb_max - sb_min);
        if (sb_range <= 0) {
            return;
        }
        const qreal travel = std::max(0.0, height() - thumb_h);
        if (travel <= 0.0) {
            sb_->setValue(sb_min);
            return;
        }
        const qreal progress = std::clamp(thumb_y / travel, 0.0, 1.0);
        sb_->setValue(static_cast<int>(std::round(sb_min + progress * sb_range)));
    }

    QPointer<QScrollBar> sb_;
    QColor thumb_ = QColor(60, 60, 67, 89);
    QColor track_ = QColor(0, 0, 0, 0);
    bool dragging_ = false;
    int drag_offset_y_ = 0;
};

class OverlayScrollArea final : public QScrollArea {
public:
    explicit OverlayScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ensureScrollbar();
    }

    void applyTheme(const QString &theme) {
        QTimer::singleShot(0, this, [this, theme] {
            ensureScrollbar();
            if (bar_ != nullptr) {
                bar_->applyTheme(theme);
            }
            syncBar();
        });
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QScrollArea::resizeEvent(event);
        ensureScrollbar();
        syncBar();
    }

    bool event(QEvent *event) override {
        if (event->type() == QEvent::StyleChange) {
            QTimer::singleShot(0, this, [this] {
                ensureScrollbar();
                syncBar();
            });
        }
        return QScrollArea::event(event);
    }

private:
    void ensureScrollbar() {
        QScrollBar *linked = verticalScrollBar();
        if (bar_ != nullptr && bar_->linkedBar() == linked) {
            return;
        }
        if (bar_ != nullptr) {
            bar_->hide();
            delete bar_;
            bar_ = nullptr;
        }
        if (scroll_hooks_ != nullptr) {
            scroll_hooks_->disconnect(this);
            scroll_hooks_ = nullptr;
        }
        if (linked == nullptr) {
            return;
        }
        bar_ = new RoundedVerticalScrollbar(linked, this);
        bar_->hide();
        scroll_hooks_ = linked;
        QObject::connect(linked, &QScrollBar::rangeChanged, this, [this](int, int) { syncBar(); });
        QObject::connect(linked, &QScrollBar::valueChanged, this, [this](int) { syncBar(); });
    }

    void syncBar() {
        if (bar_ == nullptr) {
            return;
        }
        ensureScrollbar();
        QScrollBar *bar = verticalScrollBar();
        if (bar == nullptr) {
            bar_->hide();
            return;
        }
        const bool needed = bar->maximum() > 0;
        bar_->setVisible(needed);
        if (!needed) {
            return;
        }
        const int width = bar_->width();
        bar_->setGeometry(this->width() - width - kEdgeMargin, 8, width, std::max(16, height() - 16));
        bar_->raise();
    }

    static constexpr int kEdgeMargin = 3;
    RoundedVerticalScrollbar *bar_ = nullptr;
    QPointer<QScrollBar> scroll_hooks_;
};

class FramelessRoundedPopup : public QFrame {
public:
    explicit FramelessRoundedPopup(const QString &object_name, qreal radius, QWidget *parent = nullptr)
        : QFrame(parent), radius_(radius) {
#ifdef Q_OS_MAC
        paint_bg_ = false;
#else
        paint_bg_ = true;
#endif
        // Stay a hidden child until presentAsPopup(). A live Qt::Popup window
        // created in the constructor steals QDialog::exec()'s grab, so Settings
        // never appears.
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setObjectName(object_name);
        hide();
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        surface_ = new QFrame(this);
        surface_->setObjectName(object_name + QStringLiteral("Surface"));
        surface_->setAttribute(Qt::WA_TranslucentBackground, true);
        surface_->setAutoFillBackground(false);
        root->addWidget(surface_);
    }

    QFrame *surface() const { return surface_; }
    qreal radius() const { return radius_; }

    void presentAsPopup(QWidget *anchor = nullptr) {
        if (!(windowFlags() & Qt::Popup)) {
            setWindowFlags(popup_window_flags());
            setAttribute(Qt::WA_TranslucentBackground, true);
            setAttribute(Qt::WA_NoSystemBackground, true);
        }
        show();
        raise();
        set_popup_transient_parent(this, anchor != nullptr ? anchor : parentWidget());
#ifdef Q_OS_MAC
        update_popup_rounded_mask(this, radius_);
        auto apply_glass = [this] {
            if (!isVisible() || winId() == 0) {
                return;
            }
            i2p_macos_nsview_apply_menu_vibrancy(reinterpret_cast<void *>(winId()), popup_night_ ? 1 : 0, radius_);
            update_popup_rounded_mask(this, radius_);
        };
        apply_glass();
        QTimer::singleShot(0, this, apply_glass);
#elif defined(Q_OS_WIN)
        prepare_windows_popup_chrome(this, radius_);
        QTimer::singleShot(0, this, [this] { prepare_windows_popup_chrome(this, radius_); });
#else
        update_popup_rounded_mask(this, radius_);
#endif
    }

    void setPopupColors(bool night) {
        popup_night_ = night;
#ifdef Q_OS_LINUX
        const int bg_alpha = 230;
        const int border_alpha = 175;
#elif defined(Q_OS_WIN)
        const int bg_alpha = 255;
        const int border_alpha = 255;
#else
        const int bg_alpha = 185;
        const int border_alpha = 150;
#endif
        if (night) {
            popup_bg_ = QColor(0x2c, 0x2c, 0x2e, bg_alpha);
            popup_border_ = QColor(0x48, 0x48, 0x4a, border_alpha);
        } else {
            popup_bg_ = QColor(0xf2, 0xf2, 0xf7, bg_alpha);
            popup_border_ = QColor(0xd0, 0xd0, 0xd5, border_alpha);
        }
#ifdef Q_OS_WIN
        if (isVisible()) {
            prepare_windows_popup_chrome(this, radius_);
        }
#endif
    }

    QString shellStylesheet(bool night, const QString &extra) const {
        Q_UNUSED(night);
        const QString name = objectName();
        const QString surface = name + QStringLiteral("Surface");
        return QStringLiteral("#%1 { background: transparent; }\n#%2 { background: transparent; border: none; "
                              "border-radius: %3px; }\n%4")
            .arg(name, surface, QString::number(static_cast<int>(radius_)), extra);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
#ifdef Q_OS_MAC
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), QColor(0, 0, 0, 0));
#elif defined(Q_OS_WIN)
        Q_UNUSED(event);
        paint_popup_rounded_bg(this, popup_bg_, popup_border_, radius_);
#else
        QFrame::paintEvent(event);
        if (paint_bg_) {
            paint_popup_rounded_bg(this, popup_bg_, popup_border_, radius_);
        }
#endif
    }

    void resizeEvent(QResizeEvent *event) override {
        QFrame::resizeEvent(event);
#ifdef Q_OS_MAC
        update_popup_rounded_mask(this, radius_);
        if (isVisible() && winId() != 0) {
            i2p_macos_nsview_apply_menu_vibrancy(reinterpret_cast<void *>(winId()), popup_night_ ? 1 : 0, radius_);
        }
#elif defined(Q_OS_WIN)
        if (isVisible()) {
            prepare_windows_popup_chrome(this, radius_);
        }
#else
        update_popup_rounded_mask(this, radius_);
#endif
    }

    void showEvent(QShowEvent *event) override {
        QFrame::showEvent(event);
        if (!dwm_patched_) {
#ifdef Q_OS_WIN
            disable_dwm_rounded_frame(this);
#endif
            dwm_patched_ = true;
        }
#ifdef Q_OS_WIN
        prepare_windows_popup_chrome(this, radius_);
        QTimer::singleShot(0, this, [this] { prepare_windows_popup_chrome(this, radius_); });
#else
        update_popup_rounded_mask(this, radius_);
        QTimer::singleShot(0, this, [this] { update_popup_rounded_mask(this, radius_); });
#endif
    }

private:
    QFrame *surface_ = nullptr;
    qreal radius_ = 12.0;
    bool paint_bg_ = true;
    bool dwm_patched_ = false;
    bool popup_night_ = false;
    QColor popup_bg_ = QColor(0xf2, 0xf2, 0xf7, 185);
    QColor popup_border_ = QColor(0xd0, 0xd0, 0xd5, 180);
};

class ComboItemDelegate final : public QStyledItemDelegate {
public:
    explicit ComboItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void setNight(bool night) { night_ = night; }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const int height = QFontMetrics(opt.font).height() + 10;
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), std::clamp(height, 26, 36));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.icon = QIcon();
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered = opt.state & QStyle::State_MouseOver;
        QStyleOptionViewItem base(opt);
        base.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setClipRect(base.rect);
        style->drawControl(QStyle::CE_ItemViewItem, &base, painter, widget);
        const QString text = index.data(Qt::DisplayRole).toString();
        if (text.isEmpty()) {
            painter->restore();
            return;
        }
        const QColor sel_bg(night_ ? "#3a3a3c" : "#e8e8ed");
        const QColor hov_bg(night_ ? "#3a3a3c" : "#e8e8ed");
        const QColor sel_fg(night_ ? "#f5f5f7" : "#1d1d1f");
        const QColor txt_fg(night_ ? "#f5f5f7" : "#1d1d1f");
        if (selected || hovered) {
            QRect pill = opt.rect.adjusted(2, 1, -2, -1);
#ifdef Q_OS_MAC
            pill.translate(0, -1);
#endif
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? sel_bg : hov_bg);
            painter->drawRoundedRect(QRectF(pill), 6.0, 6.0);
        }
        painter->setPen(selected ? sel_fg : txt_fg);
        painter->setFont(opt.font);
#ifdef Q_OS_MAC
        const int shift = -2;
#else
        const int shift = 0;
#endif
        painter->drawText(opt.rect.adjusted(12, shift, -10, shift), Qt::AlignLeft | Qt::AlignVCenter, text);
        painter->restore();
    }

private:
    bool night_ = false;
};

class ComboPopup final : public FramelessRoundedPopup {
public:
    explicit ComboPopup(QWidget *parent = nullptr) : FramelessRoundedPopup(QStringLiteral("StyledComboPopup"), 12.0, parent) {
        auto *inner = new QVBoxLayout(surface());
        inner->setContentsMargins(8, 8, 8, 8);
        inner->setSpacing(0);
        list_ = new QListWidget(surface());
        list_->setObjectName(QStringLiteral("StyledComboPopupList"));
        list_->setFrameShape(QFrame::NoFrame);
        list_->setSpacing(2);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->setUniformItemSizes(true);
        delegate_ = new ComboItemDelegate(list_);
        list_->setItemDelegate(delegate_);
        inner->addWidget(list_);
        QObject::connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            if (item && on_chosen) {
                on_chosen(item->text());
            }
            hide();
        });
    }

    std::function<void(const QString &)> on_chosen;

    void setItems(const QStringList &values, const QString &selected) {
        list_->clear();
        int selected_row = 0;
        for (int i = 0; i < values.size(); ++i) {
            list_->addItem(values.at(i));
            if (values.at(i) == selected) {
                selected_row = i;
            }
        }
        if (!values.isEmpty()) {
            list_->setCurrentRow(selected_row);
        }
    }

    void showBelow(QWidget *anchor) {
        sizeToAnchor(anchor);
        move(global_position_popup_below_anchor(anchor, width(), height(), 4, false));
        presentAsPopup(anchor);
        QTimer::singleShot(0, this, [this, anchor] {
            if (isVisible()) {
                sizeToAnchor(anchor);
                move(global_position_popup_below_anchor(anchor, width(), height(), 4, false));
            }
        });
        QTimer::singleShot(0, list_, [this] { list_->setFocus(); });
    }

    void applyTheme(const QString &theme) {
        const bool night = theme == QLatin1String("night");
        setPopupColors(night);
        delegate_->setNight(night);
        const QString color = night ? QStringLiteral("#f5f5f7") : QStringLiteral("#1d1d1f");
        const QString extra = QStringLiteral(
                                  "QListWidget#StyledComboPopupList { background: transparent; border: none; "
                                  "outline: none; color: %1; font-size: 13px; padding: 0px; }"
                                  "QListWidget#StyledComboPopupList::item { border: none; padding: 0px; margin: 0px; }")
                                  .arg(color);
        setStyleSheet(shellStylesheet(night, extra));
        list_->viewport()->update();
        update();
    }

private:
    int contentHeight() {
        const int count = list_->count();
        if (count <= 0) {
            return 8;
        }
        const int visible = std::min(count, 8);
        list_->setFixedHeight(4096);
        list_->doItemsLayout();
        QListWidgetItem *first = list_->item(0);
        QListWidgetItem *last = list_->item(visible - 1);
        if (first && last) {
            const int top = list_->visualItemRect(first).top();
            const int bottom = list_->visualItemRect(last).bottom();
            if (bottom > top) {
                return bottom - top + 2;
            }
        }
        int sum = 0;
        for (int i = 0; i < visible; ++i) {
            const int row = list_->sizeHintForRow(i);
            sum += row > 0 ? row : 28;
        }
        return sum + list_->spacing() * std::max(0, visible - 1) + 2;
    }

    int fitListHeight() {
        int height = contentHeight();
        list_->setFixedHeight(height);
        const int overflow = list_->verticalScrollBar()->maximum();
        if (overflow > 0) {
            height += overflow + 4;
            list_->setFixedHeight(height);
        }
        return height;
    }

    void sizeToAnchor(QWidget *anchor) {
        const int list_h = fitListHeight();
        const QMargins margins = surface()->layout()->contentsMargins();
        const int width = std::max(anchor->width(), 160);
        const int height = margins.top() + list_h + margins.bottom();
        setFixedSize(width, height);
    }

    QListWidget *list_ = nullptr;
    ComboItemDelegate *delegate_ = nullptr;
};

class ComboBoxNoPopup final : public QComboBox {
public:
    explicit ComboBoxNoPopup(QWidget *parent = nullptr) : QComboBox(parent) {}
    std::function<bool()> popup_visible;
    std::function<void()> open_popup;
    std::function<void()> hide_popup;

    void showPopup() override {
        if (suppress_open_) {
            return;
        }
        if (open_popup) {
            open_popup();
        }
    }

    void hidePopup() override {
        if (hide_popup) {
            hide_popup();
        }
    }

    void suppressNextOpen() {
        suppress_open_ = true;
        QTimer::singleShot(0, this, [this] { suppress_open_ = false; });
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && popup_visible && popup_visible()) {
            if (hide_popup) {
                hide_popup();
            }
            suppressNextOpen();
            event->accept();
            return;
        }
        QComboBox::mousePressEvent(event);
    }

private:
    bool suppress_open_ = false;
};

class ComboDropArrow final : public QWidget {
public:
    explicit ComboDropArrow(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAutoFillBackground(false);
    }

    void setArrowColor(const QColor &color) {
        if (color_ != color) {
            color_ = color;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color_);
        const int cx = width() / 2;
        const int cy = height() / 2;
        const int half_w = 4;
        const int half_h = 3;
        const QPolygonF tri({QPointF(cx - half_w, cy - half_h), QPointF(cx + half_w, cy - half_h),
                             QPointF(static_cast<qreal>(cx), cy + half_h + 1)});
        painter.drawPolygon(tri);
    }

private:
    QColor color_ = QColor(0x8c, 0x8d, 0x94);
};

class ComboDismissFilter final : public QObject {
public:
    ComboDismissFilter(QComboBox *combo, QWidget *popup, std::function<void()> on_anchor_click)
        : combo_(combo), popup_(popup), on_anchor_click_(std::move(on_anchor_click)) {
        popup_->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == popup_) {
            if (event->type() == QEvent::Show) {
                qApp->installEventFilter(this);
            } else if (event->type() == QEvent::Hide) {
                qApp->removeEventFilter(this);
            }
            return false;
        }
        if (!popup_->isVisible()) {
            return false;
        }
        if (event->type() != QEvent::MouseButtonPress) {
            return false;
        }
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        const QPoint global = mouse->globalPosition().toPoint();
        QWidget *host = combo_->parentWidget();
        if (combo_->rect().contains(combo_->mapFromGlobal(global)) ||
            (host != nullptr && host->rect().contains(host->mapFromGlobal(global)))) {
            popup_->hide();
            if (on_anchor_click_) {
                on_anchor_click_();
            }
            return true;
        }
        return false;
    }

private:
    QComboBox *combo_ = nullptr;
    QWidget *popup_ = nullptr;
    std::function<void()> on_anchor_click_;
};

class StyledComboWidget final : public QWidget {
public:
    explicit StyledComboWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        combo_ = new ComboBoxNoPopup(this);
        combo_->setEditable(false);
        combo_->setInsertPolicy(QComboBox::NoInsert);
        combo_->setMaxVisibleItems(8);
        layout->addWidget(combo_);
        arrow_ = new ComboDropArrow(this);
        popup_ = new ComboPopup(this);
        dismiss_filter_ =
            new ComboDismissFilter(combo_, popup_, [this] { combo_->suppressNextOpen(); });
        popup_->on_chosen = [this](const QString &text) {
            const int index = combo_->findText(text);
            if (index >= 0) {
                combo_->setCurrentIndex(index);
            }
        };
        combo_->popup_visible = [this] { return popup_->isVisible(); };
        combo_->hide_popup = [this] { popup_->hide(); };
        combo_->open_popup = [this] {
            QStringList values;
            for (int i = 0; i < combo_->count(); ++i) {
                values.append(combo_->itemText(i));
            }
            popup_->setItems(values, combo_->currentText());
            popup_->showBelow(combo_);
        };
        QObject::connect(combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (on_changed) {
                on_changed(index);
            }
        });
        applyTheme(QStringLiteral("light"));
    }

    ComboBoxNoPopup *combo() const { return combo_; }
    std::function<void(int)> on_changed;

    void applyTheme(const QString &theme) {
        theme_ = theme == QLatin1String("night") ? QStringLiteral("night") : QStringLiteral("light");
        const QColor color = theme_ == QLatin1String("night") ? QColor(0x9f, 0xa1, 0xb5) : QColor(0x8c, 0x8d, 0x94);
        arrow_->setArrowColor(color);
        popup_->applyTheme(theme_);
    }

    void setItems(const std::vector<std::pair<QString, QString>> &items, const QString &selected_data) {
        const bool blocked = combo_->blockSignals(true);
        combo_->clear();
        int selected = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            combo_->addItem(items[i].first, items[i].second);
            if (items[i].second == selected_data) {
                selected = static_cast<int>(i);
            }
        }
        if (!items.empty()) {
            combo_->setCurrentIndex(selected);
        }
        combo_->blockSignals(blocked);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        arrow_->setGeometry(width() - 28, 0, 28, height());
        if (popup_->isVisible()) {
            popup_->showBelow(combo_);
        }
    }

private:
    ComboBoxNoPopup *combo_ = nullptr;
    ComboDropArrow *arrow_ = nullptr;
    ComboPopup *popup_ = nullptr;
    ComboDismissFilter *dismiss_filter_ = nullptr;
    QString theme_ = QStringLiteral("light");
};

class ActionsPopupButton final : public QFrame {
public:
    explicit ActionsPopupButton(const QString &text, QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("ActionsPopupItem"));
        setFrameShape(QFrame::NoFrame);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 8, 12, 8);
        title_ = new QLabel(text, this);
        title_->setObjectName(QStringLiteral("ActionsPopupItemTitle"));
        title_->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(title_, 1);
    }

    std::function<void()> on_clicked;

    void applyColors(bool night) {
        const QString color = night ? QStringLiteral("#f5f5f7") : QStringLiteral("#1d1d1f");
        const QString disabled = night ? QStringLiteral("#8e8e93") : QStringLiteral("#8e8e93");
        title_->setStyleSheet(QStringLiteral("QLabel#ActionsPopupItemTitle { color: %1; }"
                                             "QLabel#ActionsPopupItemTitle:disabled { color: %2; }")
                                  .arg(color, disabled));
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && isEnabled() && rect().contains(event->pos())) {
            if (on_clicked) {
                on_clicked();
            }
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    QLabel *title_ = nullptr;
};

class ActionsPopupWidget final : public FramelessRoundedPopup {
public:
    explicit ActionsPopupWidget(QWidget *parent = nullptr)
        : FramelessRoundedPopup(QStringLiteral("ActionsPopup"), 14.0, parent) {
        setMinimumWidth(236);
        surface_layout_ = new QVBoxLayout(surface());
        surface_layout_->setContentsMargins(8, 8, 8, 8);
        surface_layout_->setSpacing(4);
        setFocusPolicy(Qt::StrongFocus);
    }

    void addAction(const QString &text, bool enabled, i2p_void_cb cb, void *ctx) {
        auto *button = new ActionsPopupButton(text, surface());
        button->setEnabled(enabled);
        button->setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
        button->applyColors(night_);
        button->on_clicked = [this, cb, ctx] {
            hide();
            if (cb) {
                cb(ctx);
            }
        };
        surface_layout_->addWidget(button);
        buttons_.push_back(button);
    }

    void addSeparator() {
        auto *sep = new QFrame(surface());
        sep->setObjectName(QStringLiteral("ActionsPopupSeparator"));
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Plain);
        surface_layout_->addWidget(sep);
    }

    void showBelow(QWidget *anchor) {
        adjustSize();
        move(global_position_popup_below_anchor(anchor, width(), height(), 6, true));
        presentAsPopup(anchor);
        QTimer::singleShot(0, this, [this] { setFocus(Qt::PopupFocusReason); });
    }

    void applyTheme(const QString &theme) {
        night_ = theme == QLatin1String("night");
        setPopupColors(night_);
        QString items;
        if (night_) {
            items = QStringLiteral(
                "QFrame#ActionsPopupItem { background: transparent; border: none; border-radius: 10px; }"
                "QFrame#ActionsPopupItem:hover { background: rgba(255, 255, 255, 0.12); }"
                "QFrame#ActionsPopupItem:disabled { background: transparent; }"
                "QFrame#ActionsPopupSeparator { background: rgba(255, 255, 255, 0.12); max-height: 1px; min-height: 1px; "
                "border: none; margin: 4px 8px; }");
        } else {
            items = QStringLiteral(
                "QFrame#ActionsPopupItem { background: transparent; border: none; border-radius: 10px; }"
                "QFrame#ActionsPopupItem:hover { background: rgba(0, 0, 0, 0.06); }"
                "QFrame#ActionsPopupItem:disabled { background: transparent; }"
                "QFrame#ActionsPopupSeparator { background: rgba(0, 0, 0, 0.10); max-height: 1px; min-height: 1px; "
                "border: none; margin: 4px 8px; }");
        }
        setStyleSheet(shellStylesheet(night_, items));
        for (ActionsPopupButton *button : buttons_) {
            button->applyColors(night_);
        }
        update();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
            return;
        }
        FramelessRoundedPopup::keyPressEvent(event);
    }

private:
    QVBoxLayout *surface_layout_ = nullptr;
    std::vector<ActionsPopupButton *> buttons_;
    bool night_ = false;
};

class FilesNameLabel final : public QLabel {
public:
    FilesNameLabel(const QString &text, const QString &tip, QWidget *parent = nullptr) : QLabel(parent) {
        setObjectName(QStringLiteral("FilesName"));
        setText(text);
        setToolTip(tip.isEmpty() ? text : tip);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        setContentsMargins(pad_x_, pad_y_, pad_x_, pad_y_);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_StyledBackground, true);
    }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int w) const override {
        const int text_w = std::max(1, w - pad_x_ * 2 - extraBearing() - 4);
        return std::max(40, textHeight(text_w) + pad_y_ * 2);
    }

    QSize sizeHint() const override {
        const int w = std::max(80, width());
        return QSize(w, heightForWidth(w));
    }

    QSize minimumSizeHint() const override { return QSize(40, 40); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setClipRect(rect());
        painter.setPen(palette().color(QPalette::WindowText));
        const QRect inner = contentsRect().adjusted(extraBearing(), 0, 0, 0);
        QTextLayout layout(text(), font());
        fillLayout(&layout, inner.width());
        layout.draw(&painter, QPointF(inner.left(), inner.top()));
    }

    void resizeEvent(QResizeEvent *event) override {
        QLabel::resizeEvent(event);
        update();
    }

private:
    void fillLayout(QTextLayout *layout, int width) const {
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        option.setAlignment(Qt::AlignLeft | Qt::AlignTop);
        layout->setTextOption(option);
        const QFontMetricsF fm(font());
        const qreal spacing = std::max(fm.lineSpacing(), qreal(1));
        layout->beginLayout();
        qreal y = 0;
        const int line_width = std::max(1, width);
        while (true) {
            QTextLine line = layout->createLine();
            if (!line.isValid()) {
                break;
            }
            line.setLineWidth(line_width);
            line.setPosition(QPointF(0, y));
            y += std::max(spacing, line.height());
        }
        layout->endLayout();
    }

    int extraBearing() const { return std::max(0, -fontMetrics().minLeftBearing()); }

    int textHeight(int width) const {
        QTextLayout layout(text(), font());
        fillLayout(&layout, width);
        const QFontMetricsF fm(font());
        if (layout.lineCount() == 0) {
            return static_cast<int>(std::ceil(fm.lineSpacing() + fm.descent()));
        }
        const QTextLine last = layout.lineAt(layout.lineCount() - 1);
        return static_cast<int>(std::ceil(last.y() + last.height() + fm.descent() + 2));
    }

    static constexpr int pad_x_ = 8;
    static constexpr int pad_y_ = 8;
};

class PeersItemDelegate final : public QStyledItemDelegate {
public:
    explicit PeersItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->rect.adjust(8, 0, -8, 0);
        option->state &= ~QStyle::State_HasFocus;
        option->backgroundBrush = Qt::NoBrush;
        option->textElideMode = Qt::ElideNone;
    }
};

class FilesItemDelegate final : public QStyledItemDelegate {
public:
    explicit FilesItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->rect.adjust(8, 0, -8, 0);
        option->state &= ~QStyle::State_HasFocus;
        option->backgroundBrush = Qt::NoBrush;
    }
};

class FilesTableViewport final : public QWidget {
public:
    explicit FilesTableViewport(QTableWidget *table) : QWidget(), table_(table) {}

protected:
    void paintEvent(QPaintEvent *event) override {
        if (table_ != nullptr) {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPainterPath clip;
            clip.addRoundedRect(QRectF(rect()), DialogMetrics::kFilesTableRadius,
                                DialogMetrics::kFilesTableRadius);
            painter.setClipPath(clip);
            painter.setRenderHint(QPainter::Antialiasing, false);
            const QColor base = table_->palette().color(QPalette::Base);
            const QColor alt = table_->palette().color(QPalette::AlternateBase);
            const QRect clip_rect = event->rect();
            const int w = width();
            for (int row = 0; row < table_->rowCount(); ++row) {
                QRect row_rect = table_->visualRect(table_->model()->index(row, 0));
                if (row_rect.isEmpty()) {
                    continue;
                }
                row_rect.setLeft(0);
                row_rect.setWidth(w);
                if (!row_rect.intersects(clip_rect)) {
                    continue;
                }
                painter.fillRect(row_rect, (row % 2) == 1 ? alt : base);
            }
        }
        QWidget::paintEvent(event);
    }

private:
    QTableWidget *table_ = nullptr;
};

class FilesPriorityButton final : public QToolButton {
public:
    std::function<void(int)> on_changed;

    FilesPriorityButton(const QStringList &labels, int index, QWidget *parent = nullptr)
        : QToolButton(parent), labels_(labels) {
        setObjectName(QStringLiteral("FilesPriority"));
        setToolButtonStyle(Qt::ToolButtonTextOnly);
        setAutoRaise(false);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_MacShowFocusRect, false);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        popup_ = new ComboPopup(this);
        popup_->on_chosen = [this](const QString &text) {
            const int index = labels_.indexOf(text);
            if (index < 0 || index == current_) {
                return;
            }
            applyIndex(index);
            if (on_changed) {
                on_changed(index);
            }
        };
        applyIndex(index);
    }

    int currentIndex() const { return current_; }

    void revertTo(int index) { applyIndex(index); }

    const QStringList &labels() const { return labels_; }

    void setCompactWidth(int width) {
        compact_w_ = width;
        setFixedWidth(width);
        setMinimumWidth(width);
        setMaximumWidth(width);
        updateGeometry();
    }

    QSize sizeHint() const override {
        const int w = compact_w_ > 0 ? compact_w_ : QToolButton::sizeHint().width();
        return QSize(w, QToolButton::sizeHint().height());
    }

    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && isEnabled()) {
            showPriorityPopup();
            event->accept();
            return;
        }
        QToolButton::mousePressEvent(event);
    }

private:
    void showPriorityPopup() {
        QWidget *host = this;
        while (host != nullptr && host->styleSheet().isEmpty()) {
            host = host->parentWidget();
        }
        const bool night = stylesheet_is_night(host != nullptr ? host->styleSheet() : QString());
        popup_->applyTheme(night ? QStringLiteral("night") : QStringLiteral("light"));
        popup_->setItems(labels_, text());
        popup_->showBelow(this);
    }

    void applyIndex(int index) {
        current_ = index;
        if (index >= 0 && index < labels_.size()) {
            setText(labels_.at(index));
        }
    }

    QStringList labels_;
    ComboPopup *popup_ = nullptr;
    int current_ = 0;
    int compact_w_ = 0;
};

static int files_priority_button_width(const QFont &font, const QStringList &labels) {
    const QFontMetrics metrics(font);
    int text_w = 0;
    for (const QString &label : labels) {
        text_w = std::max(text_w, metrics.horizontalAdvance(label));
    }
    return text_w + 12;
}

static void apply_files_priority_cell_margins(QTableWidget *table, int right_margin) {
    if (table == nullptr) {
        return;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        QWidget *cell = table->cellWidget(row, 3);
        if (cell == nullptr) {
            continue;
        }
        if (QLayout *layout = cell->layout()) {
            layout->setContentsMargins(0, 4, right_margin, 4);
        }
    }
}

static void apply_files_priority_compact_width(QTableWidget *table, int width) {
    if (table == nullptr || width <= 0) {
        return;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        QWidget *cell = table->cellWidget(row, 3);
        if (cell == nullptr) {
            continue;
        }
        if (auto *button = static_cast<FilesPriorityButton *>(
                cell->findChild<QToolButton *>(QStringLiteral("FilesPriority")))) {
            button->setCompactWidth(width);
        }
    }
}

static void refresh_files_priority_button_widths(QTableWidget *table) {
    if (table == nullptr || table->rowCount() <= 0) {
        return;
    }
    QWidget *cell = table->cellWidget(0, 3);
    if (cell == nullptr) {
        return;
    }
    auto *button =
        static_cast<FilesPriorityButton *>(cell->findChild<QToolButton *>(QStringLiteral("FilesPriority")));
    if (button == nullptr) {
        return;
    }
    apply_files_priority_compact_width(table, files_priority_button_width(table->font(), button->labels()));
}

#ifdef Q_OS_MAC
void clip_native_rounded(QWidget *widget, unsigned int border_rgb, int which, unsigned int fill_rgb = 0) {
    if (widget == nullptr) {
        return;
    }
    widget->setAttribute(Qt::WA_NativeWindow, true);
    widget->setAttribute(Qt::WA_TranslucentBackground, false);
    i2p_macos_nsview_clip_rounded(reinterpret_cast<void *>(widget->winId()), DialogMetrics::kFilesTableRadius,
                                  border_rgb, which, fill_rgb);
}
#endif

// Opaque corner paint over the square table. Must stay a child of an opaque pane —
// parenting a translucent overlay to the vibrancy dialog composites as pure black.
class FilesTableOverlay final : public QWidget {
public:
    explicit FilesTableOverlay(QWidget *pane, bool night, const QColor &fill = QColor()) : QWidget(pane) {
        setObjectName(QStringLiteral("FilesTableOverlay"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
        fill_ = fill.isValid() ? fill
                               : (night ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff));
    }

    void syncToHost(const QRect &host_rect, bool behind) {
        clearMask();
        setGeometry(host_rect);
        if (behind) {
            lower();
        } else {
            raise();
        }
        show();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box = QRectF(rect());
        const QRectF inner = box.adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = DialogMetrics::kFilesTableRadius;
        QPainterPath wedges;
        wedges.setFillRule(Qt::OddEvenFill);
        wedges.addRect(box);
        wedges.addRoundedRect(inner, radius, radius);
        painter.fillPath(wedges, fill_);
    }

private:
    QColor fill_;
};

#ifdef Q_OS_WIN
static int files_table_header_trailing_gap(QTableWidget *table) {
    QHeaderView *header = table != nullptr ? table->horizontalHeader() : nullptr;
    if (header == nullptr || header->count() <= 0) {
        return 0;
    }
    const int last = header->count() - 1;
    const int section_end = header->sectionPosition(last) + header->sectionSize(last);
    return std::max(0, header->width() - section_end);
}

static void files_table_disable_native_children(QTableWidget *table) {
    if (table == nullptr) {
        return;
    }
    table->setAttribute(Qt::WA_NativeWindow, false);
    table->viewport()->setAttribute(Qt::WA_NativeWindow, false);
    if (QHeaderView *header = table->horizontalHeader()) {
        header->setAttribute(Qt::WA_NativeWindow, false);
    }
}

// Fusion on Windows ignores QSS radius on the last header section and/or leaves a
// square scrollbar gutter — paint the top-right cap in header colors on top.
class FilesTableHeaderCornerCap final : public QWidget {
public:
    FilesTableHeaderCornerCap(QTableWidget *table, const QColor &header_bg, const QColor &header_border)
        : QWidget(table), table_(table), header_bg_(header_bg), header_border_(header_border) {
        setObjectName(QStringLiteral("FilesTableHeaderCornerCap"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
    }

    void syncGeometry() {
        if (table_ == nullptr) {
            hide();
            return;
        }
        QHeaderView *header = table_->horizontalHeader();
        if (header == nullptr || !header->isVisible()) {
            hide();
            return;
        }
        const int cap_w =
            static_cast<int>(std::ceil(DialogMetrics::kFilesTableRadius)) + files_table_header_trailing_gap(table_);
        const int cap_h = header->height();
        if (cap_w <= 0 || cap_h <= 0) {
            hide();
            return;
        }
        setFixedSize(cap_w, cap_h);
        move(table_->width() - cap_w, header->y());
        raise();
        show();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        const qreal radius = DialogMetrics::kFilesTableRadius;
        const QRectF box = QRectF(rect());
        QPainterPath shape;
        shape.moveTo(box.left(), box.top());
        shape.lineTo(box.right() - radius, box.top());
        shape.arcTo(box.right() - 2 * radius, box.top(), 2 * radius, 2 * radius, 90, -90);
        shape.lineTo(box.right(), box.bottom());
        shape.lineTo(box.left(), box.bottom());
        shape.closeSubpath();
        painter.fillPath(shape, header_bg_);

        painter.setPen(QPen(header_border_, 1.0));
        painter.drawLine(QPointF(box.left(), box.bottom() - 0.5), QPointF(box.right(), box.bottom() - 0.5));
    }

private:
    QTableWidget *table_ = nullptr;
    QColor header_bg_;
    QColor header_border_;
};
#endif

class FilesTablePane final : public QWidget {
public:
    explicit FilesTablePane(QWidget *parent, bool night) : QWidget(parent)
#ifdef Q_OS_WIN
        , night_(night)
#endif
    {
        setObjectName(QStringLiteral("FilesTablePane"));
#ifdef Q_OS_LINUX
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        overlay_ = nullptr;
#elif defined(Q_OS_WIN)
        setAttribute(Qt::WA_StyledBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(false);
        overlay_ = nullptr;
        header_bg_ = night ? QColor(0x25, 0x25, 0x27) : QColor(0xec, 0xec, 0xf1);
        header_border_ = night ? QColor(0x48, 0x48, 0x4a) : QColor(0xd0, 0xd0, 0xd5);
#else
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        const QColor fill = night ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, fill);
        setPalette(pal);
        overlay_ = new FilesTableOverlay(this, night);
#endif
    }

    void setScrollOverlay(QTableWidget *table, bool enabled) {
#ifdef Q_OS_WIN
        table_ = table;
        scroll_enabled_ = enabled;
        if (table_ != nullptr && header_corner_cap_ == nullptr) {
            header_corner_cap_ = new FilesTableHeaderCornerCap(table_, header_bg_, header_border_);
        }
        ensureScrollbar();
        syncScrollbar();
#else
        Q_UNUSED(table);
        Q_UNUSED(enabled);
#endif
    }

    void applyClip() {
        QTableWidget *table = findChild<QTableWidget *>();
#ifdef Q_OS_WIN
        if (table_ != nullptr) {
            table = table_;
        }
        apply_widget_rounded_mask(this, DialogMetrics::kFilesTableRadius);
#else
        clearMask();
#endif
        if (table != nullptr) {
#ifdef Q_OS_WIN
            if (header_corner_cap_ == nullptr) {
                header_corner_cap_ = new FilesTableHeaderCornerCap(table, header_bg_, header_border_);
            }
            files_table_disable_native_children(table);
            table->clearMask();
            if (QHeaderView *header = table->horizontalHeader()) {
                header->clearMask();
                const int last = header->count() - 1;
                if (last >= 0) {
                    const int gap = files_table_header_trailing_gap(table);
                    if (gap > 0) {
                        header->resizeSection(last, header->sectionSize(last) + gap);
                    }
                }
            }
            table->viewport()->clearMask();
            if (header_corner_cap_ != nullptr) {
                header_corner_cap_->syncGeometry();
            }
#else
            table->clearMask();
            if (QHeaderView *header = table->horizontalHeader()) {
                header->clearMask();
            }
            table->viewport()->clearMask();
            table->setAttribute(Qt::WA_NativeWindow, false);
#endif
        }
        if (overlay_ != nullptr) {
#ifndef Q_OS_WIN
            if (table != nullptr) {
                table->lower();
            }
#endif
            overlay_->syncToHost(rect(), false);
        }
#ifdef Q_OS_WIN
        syncScrollbar();
#endif
    }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        applyClip();
        QTimer::singleShot(0, this, [this] { applyClip(); });
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        applyClip();
    }

#ifdef Q_OS_WIN
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(night_ ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xff, 0xff, 0xff));
        painter.drawRoundedRect(QRectF(rect()), DialogMetrics::kFilesTableRadius,
                                DialogMetrics::kFilesTableRadius);
    }
#endif

private:
#ifdef Q_OS_WIN
    void ensureScrollbar() {
        if (!scroll_enabled_ || table_ == nullptr) {
            if (bar_ != nullptr) {
                bar_->hide();
            }
            return;
        }
        QScrollBar *linked = table_->verticalScrollBar();
        if (bar_ != nullptr && bar_->linkedBar() == linked) {
            bar_->applyTheme(night_ ? QStringLiteral("night") : QStringLiteral("light"));
            return;
        }
        if (bar_ != nullptr) {
            bar_->hide();
            delete bar_;
            bar_ = nullptr;
        }
        if (linked == nullptr) {
            return;
        }
        bar_ = new RoundedVerticalScrollbar(linked, this);
        bar_->applyTheme(night_ ? QStringLiteral("night") : QStringLiteral("light"));
        bar_->hide();
    }

    void syncScrollbar() {
        if (bar_ == nullptr || table_ == nullptr || !scroll_enabled_) {
            if (bar_ != nullptr) {
                bar_->hide();
            }
            return;
        }
        QScrollBar *linked = table_->verticalScrollBar();
        const bool needed = linked != nullptr && linked->maximum() > 0;
        bar_->setVisible(needed);
        if (!needed) {
            return;
        }
        const int bar_w = bar_->width();
        const int top = table_->y() + table_->horizontalHeader()->height() + 8;
        const int height = std::max(16, table_->height() - table_->horizontalHeader()->height() - 16);
        bar_->setGeometry(width() - bar_w - DialogMetrics::kFilesScrollEdge, top, bar_w, height);
        bar_->raise();
        if (header_corner_cap_ != nullptr) {
            header_corner_cap_->syncGeometry();
            header_corner_cap_->raise();
        }
    }

    QTableWidget *table_ = nullptr;
    RoundedVerticalScrollbar *bar_ = nullptr;
    FilesTableHeaderCornerCap *header_corner_cap_ = nullptr;
    bool scroll_enabled_ = false;
    bool night_ = false;
    QColor header_bg_;
    QColor header_border_;
#endif
    FilesTableOverlay *overlay_ = nullptr;
};

void style_files_table(QTableWidget *table, const QWidget *host) {
    if (table == nullptr) {
        return;
    }
    table->setFrameShape(QFrame::NoFrame);
    table->setFrameShadow(QFrame::Plain);
    table->setLineWidth(0);
    table->setAttribute(Qt::WA_StyledBackground, true);
    table->setAutoFillBackground(false);
    table->setAlternatingRowColors(false);
    table->setViewport(new FilesTableViewport(table));
    table->viewport()->setAttribute(Qt::WA_StyledBackground, true);
    table->viewport()->setAutoFillBackground(false);
    table->setItemDelegate(new FilesItemDelegate(table));

    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        fusion->setParent(table);
        table->setStyle(fusion);
        table->horizontalHeader()->setStyle(fusion);
        table->verticalHeader()->setStyle(fusion);
        table->viewport()->setStyle(fusion);
    }

    QHeaderView *header = table->horizontalHeader();
    header->setHighlightSections(false);
    header->setSectionsClickable(false);
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
#ifdef Q_OS_WIN
    header->setStretchLastSection(true);
    header->setSectionResizeMode(3, QHeaderView::Stretch);
#else
    header->setStretchLastSection(false);
#endif
    table->setCornerButtonEnabled(false);

    const bool night = stylesheet_is_night(host != nullptr ? host->styleSheet() : QString());
    const QColor base = night ? QColor(0x2c, 0x2c, 0x2e) : QColor(0xf2, 0xf2, 0xf7);
    const QColor alt = night ? QColor(0x3d, 0x3d, 0x40) : QColor(0xe4, 0xe4, 0xe9);
    const QColor text = night ? QColor(0xf5, 0xf5, 0xf7) : QColor(0x1d, 0x1d, 0x1f);
    const QColor header_bg = night ? QColor(0x25, 0x25, 0x27) : QColor(0xec, 0xec, 0xf1);
    const QColor header_fg = night ? QColor(0xc7, 0xc7, 0xcc) : QColor(0x6e, 0x6e, 0x73);
    QPalette pal = table->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        pal.setColor(group, QPalette::Base, base);
        pal.setColor(group, QPalette::AlternateBase, alt);
        pal.setColor(group, QPalette::Text, text);
#ifdef Q_OS_WIN
        pal.setColor(group, QPalette::Window, Qt::transparent);
#else
        pal.setColor(group, QPalette::Window, base);
#endif
        pal.setColor(group, QPalette::WindowText, text);
        pal.setColor(group, QPalette::Button, header_bg);
        pal.setColor(group, QPalette::ButtonText, header_fg);
    }
    table->setPalette(pal);
    table->viewport()->setPalette(pal);
    header->setPalette(pal);
    header->setAutoFillBackground(false);
#ifdef Q_OS_WIN
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#endif
}

void style_peers_table(QTableWidget *table, const QWidget *host) {
    style_files_table(table, host);
    if (table == nullptr) {
        return;
    }
    table->setItemDelegate(new PeersItemDelegate(table));
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void layout_files_table(QTableWidget *table, QVBoxLayout *layout, int dialog_w, int dialog_h, QLabel *note,
                        int button_row_h, const QString &progress_header) {
    if (table == nullptr || layout == nullptr || note == nullptr) {
        return;
    }
    const QMargins margins = layout->contentsMargins();
    const int inner_w = DialogMetrics::inner_w(dialog_w, margins);
    const int spacing = layout->spacing();

    note->setFixedWidth(inner_w);
    int note_h = note->heightForWidth(inner_w);
    if (note_h <= 0) {
        note_h = wrapped_label_height(note, inner_w);
    }
    if (note_h > 0) {
        note->setFixedHeight(note_h);
    }

    const int chrome_h = margins.top() + margins.bottom() + note_h + button_row_h + spacing * 2;
    const int max_dialog_h = dialog_h > 0 ? dialog_h : DialogMetrics::kFilesH;
    const int max_table_h = std::max(120, max_dialog_h - chrome_h);

    QHeaderView *header = table->horizontalHeader();
    header->setHighlightSections(false);
    header->setFixedHeight(std::max(DialogMetrics::kFilesHeaderMinH, header->sizeHint().height()));
    const int header_h = header->height();

    const int table_inner =
        std::max(80, inner_w - DialogMetrics::kFilesTableEdge * 2);

    table->resizeColumnToContents(1);
    const int col_size = table->columnWidth(1);

    const QFontMetrics metrics(table->font());
    const int col_progress = std::max(metrics.horizontalAdvance(QStringLiteral("100%")),
                                      metrics.horizontalAdvance(progress_header)) +
                             DialogMetrics::kFilesProgressPad;

    int col_priority = DialogMetrics::kFilesPriorityCellPad;
    if (table->rowCount() > 0) {
        if (QWidget *cell = table->cellWidget(0, 3)) {
            if (auto *button = static_cast<FilesPriorityButton *>(
                    cell->findChild<QToolButton *>(QStringLiteral("FilesPriority")))) {
                col_priority = files_priority_button_width(table->font(), button->labels()) +
                                 DialogMetrics::kFilesPriorityCellPad + DialogMetrics::kFilesPriorityRightPad;
            }
        }
    }

    table->setColumnWidth(1, col_size);
    table->setColumnWidth(2, col_progress);
#ifndef Q_OS_WIN
    table->setColumnWidth(3, col_priority);
#endif

#ifndef Q_OS_WIN
    const int scrollbar_w = table->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, table);
    auto set_name_column = [&](int reserve_scrollbar) {
        const int name_w =
            std::max(80, table_inner - col_size - col_progress - col_priority - reserve_scrollbar);
        table->setColumnWidth(0, name_w);
        for (int row = 0; row < table->rowCount(); ++row) {
            QWidget *cell = table->cellWidget(row, 0);
            if (cell != nullptr && cell->objectName() == QLatin1String("FilesName")) {
                table->setRowHeight(row, static_cast<FilesNameLabel *>(cell)->heightForWidth(name_w));
            }
        }
    };

    set_name_column(0);
    int rows_h = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        rows_h += table->rowHeight(row);
    }

    const int content_table_h = header_h + rows_h + DialogMetrics::kFilesTableEdge * 2;
    const bool need_scroll = content_table_h > max_table_h;
    const int table_h = need_scroll ? max_table_h : content_table_h;
    table->setFixedSize(inner_w, table_h);
    table->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    table->setMaximumHeight(table_h);
    if (QWidget *parent = table->parentWidget()) {
        if (parent->objectName() == QLatin1String("FilesTablePane")) {
            parent->setFixedSize(inner_w, table_h);
            parent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            static_cast<FilesTablePane *>(parent)->applyClip();
        }
    }
    table->setVerticalScrollBarPolicy(need_scroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (need_scroll) {
        set_name_column(scrollbar_w);
    }
    refresh_files_priority_button_widths(table);
    apply_files_priority_cell_margins(table, DialogMetrics::kFilesPriorityRightPad);
#else
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);

    auto update_name_rows = [&](int name_w) {
        table->setColumnWidth(0, name_w);
        for (int row = 0; row < table->rowCount(); ++row) {
            QWidget *cell = table->cellWidget(row, 0);
            if (cell != nullptr && cell->objectName() == QLatin1String("FilesName")) {
                table->setRowHeight(row, static_cast<FilesNameLabel *>(cell)->heightForWidth(name_w));
            }
        }
    };

    auto name_column_width = [&](int scroll_reserve) {
        return std::max(80, table_inner - col_size - col_progress - col_priority - scroll_reserve);
    };

    update_name_rows(name_column_width(0));
    int rows_h = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        rows_h += table->rowHeight(row);
    }

    const int content_table_h = header_h + rows_h + DialogMetrics::kFilesTableEdge * 2;
    const bool need_scroll = content_table_h > max_table_h;
    const int table_h = need_scroll ? max_table_h : content_table_h;
    const int scroll_reserve = need_scroll ? DialogMetrics::kFilesScrollReserve : 0;
    update_name_rows(name_column_width(scroll_reserve));

    table->setFixedSize(inner_w, table_h);
    table->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    table->setMaximumHeight(table_h);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    if (QWidget *parent = table->parentWidget()) {
        if (parent->objectName() == QLatin1String("FilesTablePane")) {
            parent->setFixedSize(inner_w, table_h);
            parent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            auto *pane = static_cast<FilesTablePane *>(parent);
            pane->setScrollOverlay(table, need_scroll);
            pane->applyClip();
        }
    }
    refresh_files_priority_button_widths(table);
    apply_files_priority_cell_margins(table, DialogMetrics::kFilesPriorityRightPad + scroll_reserve);
#endif
}

class CardClickFilter final : public QObject {
public:
    CardClickFilter(QWidget *target, i2p_void_cb cb, void *ctx) : QObject(target), cb_(cb), ctx_(ctx) {
        target->installEventFilter(this);
        target->setCursor(Qt::PointingHandCursor);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() != QEvent::MouseButtonRelease) {
            return false;
        }
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        auto *widget = qobject_cast<QWidget *>(obj);
        if (widget == nullptr) {
            return false;
        }
        for (QWidget *cur = widget->childAt(mouse->pos()); cur != nullptr && cur != widget;
             cur = cur->parentWidget()) {
            const QString name = cur->objectName();
            if (name == QLatin1String("MoreButton") || name == QLatin1String("PeersLink")) {
                return false;
            }
        }
        if (cb_) {
            const i2p_void_cb cb = cb_;
            void *ctx = ctx_;
            QTimer::singleShot(0, qApp, [cb, ctx]() {
                if (cb) {
                    cb(ctx);
                }
            });
            return true;
        }
        return false;
    }

private:
    i2p_void_cb cb_ = nullptr;
    void *ctx_ = nullptr;
};

class SpinFocusFilter final : public QObject {
public:
    explicit SpinFocusFilter(QFrame *row) : QObject(row), row_(row) {}

protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::FocusIn) {
            row_->setProperty("focused", true);
        } else if (event->type() == QEvent::FocusOut) {
            row_->setProperty("focused", false);
        } else {
            return false;
        }
        row_->style()->unpolish(row_);
        row_->style()->polish(row_);
        row_->update();
        return false;
    }

private:
    QFrame *row_ = nullptr;
};

class SpinStepButton final : public QToolButton {
public:
    enum class Direction { Up, Down };

    explicit SpinStepButton(Direction direction, QWidget *parent = nullptr)
        : QToolButton(parent), direction_(direction) {
        setAutoRaise(true);
        setToolButtonStyle(Qt::ToolButtonIconOnly);
        setCursor(Qt::ArrowCursor);
        setAutoRepeat(true);
        setAutoRepeatDelay(400);
        setAutoRepeatInterval(120);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QStyleOptionToolButton option;
        initStyleOption(&option);
        option.text.clear();
        option.icon = QIcon();
        QPainter painter(this);
        style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor color = palette().color(foregroundRole());
        QPen pen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        const qreal span = 3.6;
        QPainterPath path;
        if (direction_ == Direction::Up) {
            path.moveTo(cx - span, cy + 1.4);
            path.lineTo(cx, cy - 1.6);
            path.lineTo(cx + span, cy + 1.4);
        } else {
            path.moveTo(cx - span, cy - 1.4);
            path.lineTo(cx, cy + 1.6);
            path.lineTo(cx + span, cy - 1.4);
        }
        painter.drawPath(path);
    }

private:
    Direction direction_;
};

class SpinRowWidget final : public QFrame {
public:
    explicit SpinRowWidget(QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("SpinRow"));
        setFrameShape(QFrame::NoFrame);
        setProperty("focused", false);
        lock_dialog_control(this);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        spin_ = new QSpinBox(this);
        spin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        spin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(spin_, 1);
        auto *step_col = new QWidget(this);
        step_col->setObjectName(QStringLiteral("SpinStepColumn"));
        auto *steps = new QVBoxLayout(step_col);
        steps->setContentsMargins(0, 0, 0, 0);
        steps->setSpacing(0);
        auto *up = new SpinStepButton(SpinStepButton::Direction::Up, step_col);
        up->setObjectName(QStringLiteral("SpinStepUp"));
        auto *down = new SpinStepButton(SpinStepButton::Direction::Down, step_col);
        down->setObjectName(QStringLiteral("SpinStepDown"));
        QObject::connect(up, &QToolButton::clicked, this, [this] {
            spin_->stepUp();
            spin_->setFocus(Qt::OtherFocusReason);
        });
        QObject::connect(down, &QToolButton::clicked, this, [this] {
            spin_->stepDown();
            spin_->setFocus(Qt::OtherFocusReason);
        });
        steps->addWidget(up);
        steps->addWidget(down);
        layout->addWidget(step_col, 0);
        spin_->installEventFilter(new SpinFocusFilter(this));
        QObject::connect(spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            if (on_changed) {
                on_changed(value);
            }
        });
    }

    QSpinBox *spin() const { return spin_; }
    std::function<void(int)> on_changed;

private:
    QSpinBox *spin_ = nullptr;
};

QColor outline_color(const QColor &bg, const QColor &fg) {
    constexpr qreal mix = 0.18;
    return QColor(qRound(bg.red() * (1.0 - mix) + fg.red() * mix),
                  qRound(bg.green() * (1.0 - mix) + fg.green() * mix),
                  qRound(bg.blue() * (1.0 - mix) + fg.blue() * mix));
}

class RoundedTooltipWindow;

bool cursor_over_owner_or_tip(QWidget *owner);
bool tooltip_has_focus();

class RoundedTooltipWindow final : public QWidget {
public:
    RoundedTooltipWindow() : QWidget(nullptr, tooltipFlags()) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
#if defined(Q_OS_LINUX) || (defined(Q_OS_UNIX) && !defined(Q_OS_MAC) && !defined(Q_OS_WIN))
        // Tiling WMs tile Qt::Tool as a normal window; ToolTip stays transient.
        setAttribute(Qt::WA_X11DoNotAcceptFocus, true);
#endif
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
        label_ = new QLabel(this);
        label_->setWordWrap(true);
        label_->setMaximumWidth(440);
        label_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        label_->setFocusPolicy(Qt::ClickFocus);
        label_->setCursor(Qt::IBeamCursor);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(0);
        layout->addWidget(label_);
        hide_timer_.setSingleShot(true);
        QObject::connect(&hide_timer_, &QTimer::timeout, this, [this] { startFadeOut(); });
        fade_in_.setTargetObject(this);
        fade_in_.setPropertyName("windowOpacity");
        fade_in_.setEasingCurve(QEasingCurve::Linear);
        fade_out_.setTargetObject(this);
        fade_out_.setPropertyName("windowOpacity");
        fade_out_.setEasingCurve(QEasingCurve::Linear);
        dismiss_.setSingleShot(true);
        QObject::connect(&dismiss_, &QTimer::timeout, this, [this] { startFadeOut(); });
        QObject::connect(&fade_out_, &QAbstractAnimation::finished, this, [this] { afterFadeOutHide(); });
    }

    QPointer<QWidget> owner;

    void present(const QPoint &global_top_left, const QString &text, int msec, QWidget *owner_widget) {
        QApplication *app = qApp;
        const QPalette pal = app ? app->palette() : palette();
        const QColor fg = pal.color(QPalette::Active, QPalette::ToolTipText);
        label_->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none; margin: 0; padding: 0;")
                                  .arg(fg.name(QColor::HexRgb)));
        owner = owner_widget;
        stopOpacity();
        label_->setText(text);
        label_->clearFocus();
        adjustSize();
        move(clampGlobal(global_top_left, width(), height()));
        updateMask();
        set_popup_transient_parent(this, owner_widget);
        dismiss_.stop();
        hide_timer_.stop();
        if (platform_supports_window_opacity()) {
            stopOpacity();
            setWindowOpacity(0.0);
            show();
            raise();
            fade_in_.setDuration(150);
            fade_in_.setStartValue(0.0);
            fade_in_.setEndValue(1.0);
            fade_in_.start();
        } else {
            stopOpacity();
            setWindowOpacity(1.0);
            show();
            raise();
        }
        if (msec > 0) {
            hide_timer_.start(msec);
        }
    }

    void forceHide() {
        owner = nullptr;
        dismiss_.stop();
        hide_timer_.stop();
        stopOpacity();
        setWindowOpacity(1.0);
        if (isVisible()) {
            QWidget::hide();
        }
    }

    void startFadeOut() {
        if (cursor_over_owner_or_tip(owner.data())) {
            return;
        }
        dismiss_.stop();
        hide_timer_.stop();
        if (!isVisible()) {
            setWindowOpacity(1.0);
            return;
        }
        if (!platform_supports_window_opacity()) {
            afterFadeOutHide();
            return;
        }
        if (fade_out_.state() == QAbstractAnimation::Running) {
            return;
        }
        fade_in_.stop();
        fade_out_.setDuration(150);
        fade_out_.setStartValue(windowOpacity());
        fade_out_.setEndValue(0.0);
        fade_out_.start();
    }

    void scheduleDismiss() {
        if (cursor_over_owner_or_tip(owner.data())) {
            return;
        }
        if (!dismiss_.isActive()) {
            dismiss_.start(500);
        }
    }

    void cancelDismiss() { dismiss_.stop(); }

    void pauseAutoHide() {
        dismiss_.stop();
        hide_timer_.stop();
    }

    bool containsGlobalPoint(const QPoint &global_pos) const {
        return geometry().contains(global_pos);
    }

protected:
    void enterEvent(QEnterEvent *event) override {
        QWidget::enterEvent(event);
        pauseAutoHide();
    }

    void leaveEvent(QEvent *event) override {
        QWidget::leaveEvent(event);
        if (!cursor_over_owner_or_tip(owner.data())) {
            scheduleDismiss();
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), QColor(0, 0, 0, 0));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        const QRectF box = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        QApplication *app = qApp;
        const QPalette pal = app ? app->palette() : palette();
        const QColor bg = pal.color(QPalette::Active, QPalette::ToolTipBase);
        const QColor fg = pal.color(QPalette::Active, QPalette::ToolTipText);
        QPen pen(outline_color(bg, fg));
        pen.setWidth(1);
        pen.setCosmetic(true);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(bg);
        painter.drawRoundedRect(box, 12.0, 12.0);
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
#ifdef Q_OS_WIN
        if (isVisible()) {
            prepare_windows_popup_chrome(this, 12.0);
        }
#else
        updateMask();
#endif
    }

    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (owner) {
            set_popup_transient_parent(this, owner.data());
        }
        if (!dwm_patched_) {
#ifdef Q_OS_WIN
            disable_dwm_rounded_frame(this);
#endif
            dwm_patched_ = true;
        }
#ifdef Q_OS_WIN
        prepare_windows_popup_chrome(this, 12.0);
        QTimer::singleShot(0, this, [this] { prepare_windows_popup_chrome(this, 12.0); });
#else
        QTimer::singleShot(0, this, [this] { updateMask(); });
#endif
    }

private:
    static Qt::WindowFlags tooltipFlags() {
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
        Qt::WindowFlags flags =
            Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
        flags |= Qt::NoDropShadowWindowHint;
#else
        // Qt::Tool is treated as a tileable window on X11/Wayland tiling compositors.
        Qt::WindowFlags flags =
            Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
#endif
        return flags;
    }

    QPoint clampGlobal(QPoint top_left, int width, int height) const {
        QScreen *screen = QGuiApplication::screenAt(top_left);
        if (screen == nullptr) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen == nullptr) {
            return top_left;
        }
        return clamp_popup_top_left(top_left, width, height, screen->availableGeometry());
    }

    void updateMask() { update_popup_rounded_mask(this, 12.0); }

    void stopOpacity() {
        fade_in_.stop();
        fade_out_.stop();
    }

    void afterFadeOutHide() {
        if (isVisible()) {
            QWidget::hide();
        }
        setWindowOpacity(1.0);
        owner = nullptr;
    }

    QLabel *label_ = nullptr;
    QTimer hide_timer_;
    QTimer dismiss_;
    QPropertyAnimation fade_in_{this};
    QPropertyAnimation fade_out_{this};
    bool dwm_patched_ = false;
};

RoundedTooltipWindow *g_tip = nullptr;

class TooltipInterceptFilter;
TooltipInterceptFilter *g_tooltip_filter = nullptr;

void dismiss_transient_windows(QWidget *keep) {
    if (g_tip != nullptr) {
        g_tip->forceHide();
    }
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets) {
        if (widget == nullptr || widget == keep) {
            continue;
        }
        if (auto *dialog = qobject_cast<QDialog *>(widget)) {
            if (dialog->isModal()) {
                dialog->reject();
            } else {
                dialog->close();
            }
            continue;
        }
        const Qt::WindowFlags flags = widget->windowFlags();
        if (!widget->isVisible()) {
            continue;
        }
        if (flags.testFlag(Qt::Popup) || flags.testFlag(Qt::ToolTip) || flags.testFlag(Qt::Tool) ||
            flags.testFlag(Qt::SplashScreen)) {
            widget->close();
        }
    }
}

class MainWindowShutdownFilter final : public QObject {
public:
    explicit MainWindowShutdownFilter(QWidget *main) : QObject(main), main_(main) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == main_ && event->type() == QEvent::Close) {
            dismiss_transient_windows(main_);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *main_ = nullptr;
};

void install_app_shutdown_helper(QWidget *main_window) {
    if (main_window == nullptr || !main_window->isWindow() || qobject_cast<QDialog *>(main_window) != nullptr) {
        return;
    }
    static QPointer<QWidget> tracked_main;
    static bool app_hooks_installed = false;
    if (tracked_main == main_window) {
        return;
    }
    tracked_main = main_window;
    main_window->installEventFilter(new MainWindowShutdownFilter(main_window));
    if (app_hooks_installed || qApp == nullptr) {
        return;
    }
    app_hooks_installed = true;
    qApp->setQuitOnLastWindowClosed(true);
    QObject::connect(qApp, &QApplication::aboutToQuit, qApp, [] {
        if (tracked_main != nullptr) {
            dismiss_transient_windows(tracked_main.data());
        }
        shutdown_app_chrome();
    });
}

RoundedTooltipWindow *ensure_tip() {
    if (g_tip == nullptr) {
        g_tip = new RoundedTooltipWindow();
    }
    return g_tip;
}

class TableItemTooltipFilter final : public QObject {
public:
    explicit TableItemTooltipFilter(QTableWidget *table) : QObject(table), table_(table) {
        if (table_ != nullptr && table_->viewport() != nullptr) {
            table_->viewport()->installEventFilter(this);
            table_->viewport()->setMouseTracking(true);
        }
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (table_ == nullptr || table_->viewport() != obj || event->type() != QEvent::ToolTip) {
            return false;
        }
        auto *help = static_cast<QHelpEvent *>(event);
        const QModelIndex index = table_->indexAt(help->pos());
        if (!index.isValid()) {
            ensure_tip()->forceHide();
            return true;
        }
        QTableWidgetItem *item = table_->item(index.row(), index.column());
        if (item == nullptr) {
            ensure_tip()->forceHide();
            return true;
        }
        const QString visible = item->text().trimmed();
        QString tip = item->toolTip().trimmed();
        if (tip.isEmpty()) {
            tip = visible;
        }
        const QFontMetrics metrics(table_->fontMetrics());
        const int col_w = table_->columnWidth(index.column());
        constexpr int cell_pad = 16;
        const bool truncated = metrics.horizontalAdvance(visible) > std::max(0, col_w - cell_pad);
        const bool address_hash = index.column() == 0 && !tip.isEmpty() && tip != visible;
        if (tip.isEmpty() || (!address_hash && !truncated && tip == visible)) {
            ensure_tip()->forceHide();
            return true;
        }
        ensure_tip()->present(help->globalPos(), tip, -1, table_->viewport());
        return true;
    }

private:
    QTableWidget *table_ = nullptr;
};

bool cursor_over_owner_or_tip(QWidget *owner) {
    const QPoint global_pos = QCursor::pos();
    if (g_tip != nullptr && g_tip->isVisible() && g_tip->containsGlobalPoint(global_pos)) {
        return true;
    }
    QWidget *widget = QApplication::widgetAt(global_pos);
    if (widget == nullptr) {
        return false;
    }
    if (g_tip && (widget == g_tip || g_tip->isAncestorOf(widget))) {
        return true;
    }
    if (owner == nullptr) {
        return false;
    }
    return widget == owner || owner->isAncestorOf(widget);
}

bool tooltip_has_focus() {
    if (g_tip == nullptr || !g_tip->isVisible()) {
        return false;
    }
    QWidget *focus = QApplication::focusWidget();
    return focus != nullptr && (focus == g_tip || g_tip->isAncestorOf(focus));
}

class TooltipInterceptFilter final : public QObject {
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(event);
            auto *widget = qobject_cast<QWidget *>(obj);
            if (widget) {
                if (QWidget *parent = widget->parentWidget()) {
                    if (auto *view = qobject_cast<QAbstractItemView *>(parent)) {
                        if (view->viewport() == widget) {
                            return false;
                        }
                    }
                }
                const QString tip = widget->toolTip().trimmed();
                if (!tip.isEmpty()) {
                    ensure_tip()->present(help->globalPos(), tip, -1, widget);
                    return true;
                }
                ensure_tip()->forceHide();
                return true;
            }
        }
        if (g_tip == nullptr || !g_tip->isVisible()) {
            return false;
        }
        const QEvent::Type et = event->type();
        QWidget *owner = g_tip->owner.data();
        if (et == QEvent::MouseButtonPress || et == QEvent::Wheel) {
            if (!cursor_over_owner_or_tip(owner) && !tooltip_has_focus()) {
                g_tip->cancelDismiss();
                g_tip->startFadeOut();
            }
        } else if (et == QEvent::KeyPress) {
            if (!cursor_over_owner_or_tip(owner) && !tooltip_has_focus()) {
                g_tip->cancelDismiss();
                g_tip->startFadeOut();
            }
        } else if (et == QEvent::FocusOut || et == QEvent::WindowDeactivate) {
            if (!cursor_over_owner_or_tip(owner) && !tooltip_has_focus()) {
                g_tip->scheduleDismiss();
            }
        } else if (et == QEvent::MouseMove || et == QEvent::Leave) {
            if (cursor_over_owner_or_tip(owner)) {
                g_tip->cancelDismiss();
                g_tip->pauseAutoHide();
            } else {
                g_tip->scheduleDismiss();
            }
        } else if (et == QEvent::Hide || et == QEvent::Close || et == QEvent::Destroy) {
            auto *widget = qobject_cast<QWidget *>(obj);
            if (owner == nullptr) {
                g_tip->cancelDismiss();
                g_tip->forceHide();
            } else if (widget && (widget == owner || owner->isAncestorOf(widget) || widget->isAncestorOf(owner))) {
                g_tip->cancelDismiss();
                g_tip->forceHide();
            }
        }
        return false;
    }
};

void shutdown_app_chrome() {
    if (g_tip != nullptr) {
        g_tip->forceHide();
        delete g_tip;
        g_tip = nullptr;
    }
    if (QApplication *app = qApp) {
        if (g_tooltip_filter != nullptr) {
            app->removeEventFilter(g_tooltip_filter);
            delete g_tooltip_filter;
            g_tooltip_filter = nullptr;
        }
    }
}

thread_local std::string g_combo_data;
thread_local std::string g_line_edit_text;
thread_local std::string g_settings_rpc;
thread_local std::string g_settings_dir;
thread_local std::string g_settings_language;
thread_local std::string g_settings_theme;
thread_local std::string g_settings_view;
thread_local int g_settings_refresh = 2;
thread_local std::string g_open_file;
thread_local std::string g_create_torrent_path;
thread_local int g_create_torrent_add_after = 0;

class ComboPopupContainerChrome final : public QObject {
public:
    ComboPopupContainerChrome(QWidget *container, const QColor &bg, const QColor &border, qreal radius)
        : QObject(container), container_(container), bg_(bg), border_(border), radius_(radius) {
        container_->installEventFilter(this);
    }

    void updateColors(const QColor &bg, const QColor &border) {
        bg_ = bg;
        border_ = border;
        if (container_ != nullptr) {
            container_->update();
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != container_) {
            return QObject::eventFilter(watched, event);
        }
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
#ifdef Q_OS_WIN
            if (container_->isVisible()) {
                container_->clearMask();
                (void)container_->winId();
                const HWND hwnd = reinterpret_cast<HWND>(container_->winId());
                disable_dwm_rounded_frame(container_);
                if (hwnd != nullptr) {
                    disable_windows_popup_acrylic(hwnd);
                    clear_windows_popup_hwnd_region(hwnd);
                }
                if (auto *view = container_->findChild<QAbstractItemView *>()) {
                    constexpr int inset = 1;
                    view->setGeometry(container_->rect().adjusted(inset, inset, -inset, -inset));
                }
                container_->update();
            }
#endif
        } else if (event->type() == QEvent::Paint) {
#ifdef Q_OS_WIN
            QPainter painter(container_);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(container_->rect(), QColor(0, 0, 0, 0));
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(Qt::NoPen);
            painter.setBrush(bg_);
            painter.drawRoundedRect(QRectF(container_->rect()), radius_, radius_);
#else
            paint_popup_rounded_bg(container_, bg_, border_, radius_);
#endif
            return true;
        }
        return false;
    }

private:
    QWidget *container_ = nullptr;
    QColor bg_;
    QColor border_;
    qreal radius_ = 10.0;
};

void style_combo_popup(QComboBox *combo) {
    if (combo == nullptr) {
        return;
    }
    QAbstractItemView *view = combo->view();
    if (view == nullptr) {
        return;
    }
    QWidget *host = combo;
    while (host != nullptr && host->styleSheet().isEmpty()) {
        host = host->parentWidget();
    }
    const bool night = stylesheet_is_night(host != nullptr ? host->styleSheet() : QString());
    const QColor bg = night ? QColor(0x2c, 0x2c, 0x2e) : QColor(0xf6, 0xf7, 0xfa);
    const QColor fg = night ? QColor(0xf5, 0xf5, 0xf7) : QColor(0x1d, 0x1d, 0x1f);
    const QColor sel = night ? QColor(0x3a, 0x3a, 0x3c) : QColor(0xe5, 0xea, 0xf2);
    const QColor border = night ? QColor(0x48, 0x48, 0x4a) : QColor(0xd0, 0xd0, 0xd5);
    QPalette pal = view->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive}) {
        pal.setColor(group, QPalette::Base, Qt::transparent);
        pal.setColor(group, QPalette::Window, Qt::transparent);
        pal.setColor(group, QPalette::Text, fg);
        pal.setColor(group, QPalette::WindowText, fg);
        pal.setColor(group, QPalette::ButtonText, fg);
        pal.setColor(group, QPalette::Highlight, sel);
        pal.setColor(group, QPalette::HighlightedText, fg);
    }
    view->setAttribute(Qt::WA_TranslucentBackground, true);
    view->setAutoFillBackground(false);
    view->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    view->viewport()->setAutoFillBackground(false);
    view->setPalette(pal);
    view->viewport()->setPalette(pal);
    view->setObjectName(QStringLiteral("ComboPopupView"));
    view->setFrameShape(QFrame::NoFrame);
    if (QWidget *container = view->parentWidget()) {
#ifndef Q_OS_WIN
        container->clearMask();
#endif
        container->setAttribute(Qt::WA_TranslucentBackground, true);
        container->setAutoFillBackground(false);
        container->setObjectName(QStringLiteral("ComboPopupContainer"));
        container->setAttribute(Qt::WA_StyledBackground, false);
        container->setStyleSheet(
            QStringLiteral("QComboBoxPrivateContainer, QWidget#ComboPopupContainer { background: transparent; border: none; "
                           "padding: 0px; outline: none; }"
                           "QComboBox QAbstractItemView, QAbstractItemView#ComboPopupView { background: transparent; color: %1; "
                           "border: none; outline: none; selection-background-color: %2; selection-color: %1; }")
                .arg(fg.name(), sel.name()));
        ComboPopupContainerChrome *chrome = nullptr;
        for (QObject *child : container->children()) {
            if ((chrome = dynamic_cast<ComboPopupContainerChrome *>(child)) != nullptr) {
                break;
            }
        }
        if (chrome == nullptr) {
            chrome = new ComboPopupContainerChrome(container, bg, border, 10.0);
        } else {
            chrome->updateColors(bg, border);
        }
#ifdef Q_OS_MAC
        if (container != combo && (container->isWindow() || (container->windowFlags() & Qt::Popup))) {
            clip_native_rounded(container, night ? 0x48484au : 0xd0d0d5u, 0, 0);
        }
#endif
    }
}

class ComboPopupFilter final : public QObject {
public:
    explicit ComboPopupFilter(QComboBox *combo) : QObject(combo), combo_(combo) {
        if (QAbstractItemView *view = combo->view()) {
            view->installEventFilter(this);
            if (QWidget *container = view->parentWidget()) {
                container->installEventFilter(this);
            }
        }
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Show) {
            style_combo_popup(combo_);
            if (QAbstractItemView *view = combo_->view()) {
                if (QWidget *container = view->parentWidget()) {
                    container->installEventFilter(this);
                    set_popup_transient_parent(container, combo_);
                }
            }
        }
        return false;
    }

private:
    QComboBox *combo_ = nullptr;
};

QComboBox *settings_combo(QWidget *parent, bool night, const char *text_a, const char *data_a,
                          const char *text_b, const char *data_b, const char *current) {
    auto *widget = new StyledComboWidget(parent);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widget->combo()->addItem(qstr(text_a), qstr(data_a));
    widget->combo()->addItem(qstr(text_b), qstr(data_b));
    const int index = widget->combo()->findData(qstr(current));
    widget->combo()->setCurrentIndex(index >= 0 ? index : 0);
    lock_dialog_control(widget);
    widget->applyTheme(night ? QStringLiteral("night") : QStringLiteral("light"));
    return widget->combo();
}

std::string combo_data_string(QComboBox *combo) {
    return combo ? combo->currentData().toString().toStdString() : std::string();
}

} // namespace

extern "C" {

void i2p_widget_delete(void *widget) {
    delete as_widget(widget);
}

void i2p_widget_set_object_name(void *widget, const char *name) {
    if (QWidget *w = as_widget(widget)) {
        w->setObjectName(qstr(name));
        w->setAttribute(Qt::WA_StyledBackground, true);
        i2p_widget_repolish(widget);
    }
}

void i2p_widget_set_tooltip(void *widget, const char *tip) {
    if (QWidget *w = as_widget(widget)) {
        w->setToolTip(qstr(tip));
    }
}

void i2p_widget_set_cursor(void *widget, int shape) {
    if (QWidget *w = as_widget(widget)) {
        w->setCursor(static_cast<Qt::CursorShape>(shape));
    }
}

void i2p_widget_on_click(void *widget, i2p_void_cb cb, void *ctx) {
    if (QWidget *w = as_widget(widget)) {
        new CardClickFilter(w, cb, ctx);
    }
}

void i2p_label_set_text(void *widget, const char *text) {
    if (auto *label = qobject_cast<QLabel *>(as_widget(widget))) {
        label->setText(qstr(text));
    }
}

void i2p_widget_repolish(void *widget) {
    if (QWidget *w = as_widget(widget)) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}

void i2p_line_edit_set_placeholder(void *widget, const char *text) {
    if (auto *edit = qobject_cast<QLineEdit *>(as_widget(widget))) {
        edit->setPlaceholderText(qstr(text));
    }
}

const char *i2p_line_edit_text(void *widget) {
    g_line_edit_text.clear();
    if (auto *edit = qobject_cast<QLineEdit *>(as_widget(widget))) {
        g_line_edit_text = edit->text().toStdString();
    }
    return g_line_edit_text.c_str();
}

void i2p_line_edit_set_text(void *widget, const char *text) {
    if (auto *edit = qobject_cast<QLineEdit *>(as_widget(widget))) {
        edit->setText(qstr(text));
    }
}

void i2p_widget_set_stylesheet(void *widget, const char *css) {
    if (QWidget *w = as_widget(widget)) {
        w->setStyleSheet(qstr(css));
    }
}

void *i2p_dialog_new(void *parent, const char *title, int w, int h) {
    auto *dialog = new QDialog(as_widget(parent));
    dialog->setWindowTitle(qstr(title));
    apply_real_dialog_window(dialog);
    dialog->setMinimumSize(w, h);
    dialog->resize(w, h);
    return dialog;
}

int i2p_dialog_exec(void *dialog) {
    if (QWidget *w = as_widget(dialog)) {
        return static_cast<QDialog *>(w)->exec() == QDialog::Accepted ? 1 : 0;
    }
    return 0;
}

void i2p_dialog_accept(void *dialog) {
    if (QWidget *w = as_widget(dialog)) {
        static_cast<QDialog *>(w)->accept();
    }
}

void i2p_dialog_reject(void *dialog) {
    if (QWidget *w = as_widget(dialog)) {
        static_cast<QDialog *>(w)->reject();
    }
}

void i2p_dialog_finish(void *dialog) {
    if (QWidget *w = as_widget(dialog)) {
        auto *d = static_cast<QDialog *>(w);
        d->hide();
        d->setParent(nullptr);
        d->deleteLater();
    }
}

void i2p_push_button_set_checkable(void *widget, int checkable) {
    if (auto *button = qobject_cast<QAbstractButton *>(as_widget(widget))) {
        button->setCheckable(checkable != 0);
    }
}

void i2p_push_button_set_checked(void *widget, int checked) {
    if (auto *button = qobject_cast<QAbstractButton *>(as_widget(widget))) {
        button->setChecked(checked != 0);
        i2p_widget_repolish(widget);
    }
}

void i2p_push_button_set_auto_exclusive(void *widget, int exclusive) {
    if (auto *button = qobject_cast<QAbstractButton *>(as_widget(widget))) {
        button->setAutoExclusive(exclusive != 0);
    }
}

void i2p_push_button_set_text(void *widget, const char *text) {
    if (auto *button = qobject_cast<QAbstractButton *>(as_widget(widget))) {
        button->setText(qstr(text));
    }
}

void *i2p_piece_map_new(const uint8_t *have, int len) {
    auto *map = new PieceMapWidget();
    map->setHave(have, len);
    return map;
}

void *i2p_torrent_card_new(const char *theme) {
    const bool night = qstr(theme) != QLatin1String("light");
    return new TorrentCardWidget(night);
}

void i2p_piece_map_set_have(void *widget, const uint8_t *have, int len) {
    if (auto *map = static_cast<PieceMapWidget *>(as_widget(widget))) {
        map->setHave(have, len);
    }
}

void *i2p_overlay_scroll_new(void) { return new OverlayScrollArea(); }

void i2p_overlay_scroll_set_widget(void *scroll, void *child) {
    if (auto *area = qobject_cast<QScrollArea *>(as_widget(scroll))) {
        area->setWidget(as_widget(child));
    }
}

void i2p_overlay_scroll_apply_theme(void *scroll, const char *theme) {
    QWidget *widget = as_widget(scroll);
    if (widget == nullptr) {
        return;
    }
    if (auto *area = dynamic_cast<OverlayScrollArea *>(widget)) {
        area->applyTheme(qstr(theme));
    }
}

void *i2p_styled_combo_new(void) { return new StyledComboWidget(); }

void i2p_styled_combo_clear(void *combo) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        widget->combo()->clear();
    }
}

void i2p_styled_combo_add_item(void *combo, const char *text, const char *data) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        widget->combo()->addItem(qstr(text), qstr(data));
    }
}

void i2p_styled_combo_set_index(void *combo, int index) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        widget->combo()->setCurrentIndex(index);
    }
}

int i2p_styled_combo_index(void *combo) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        return widget->combo()->currentIndex();
    }
    return 0;
}

const char *i2p_styled_combo_data(void *combo) {
    g_combo_data.clear();
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        g_combo_data = widget->combo()->currentData().toString().toStdString();
    }
    return g_combo_data.c_str();
}

void i2p_styled_combo_apply_theme(void *combo, const char *theme) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        widget->applyTheme(qstr(theme));
    }
}

void i2p_styled_combo_on_changed(void *combo, i2p_int_cb cb, void *ctx) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        widget->on_changed = [cb, ctx](int index) {
            if (cb) {
                cb(ctx, index);
            }
        };
    }
}

void i2p_styled_combo_block_signals(void *combo, int block) {
    if (auto *widget = static_cast<StyledComboWidget *>(as_widget(combo))) {
        widget->combo()->blockSignals(block != 0);
    }
}

void *i2p_actions_popup_new(void *parent) { return new ActionsPopupWidget(as_widget(parent)); }

void i2p_actions_popup_add_action(void *popup, const char *text, int enabled, i2p_void_cb cb, void *ctx) {
    if (auto *widget = static_cast<ActionsPopupWidget *>(as_widget(popup))) {
        widget->addAction(qstr(text), enabled != 0, cb, ctx);
    }
}

void i2p_actions_popup_add_separator(void *popup) {
    if (auto *widget = static_cast<ActionsPopupWidget *>(as_widget(popup))) {
        widget->addSeparator();
    }
}

void i2p_actions_popup_apply_theme(void *popup, const char *theme) {
    if (auto *widget = static_cast<ActionsPopupWidget *>(as_widget(popup))) {
        widget->applyTheme(qstr(theme));
    }
}

void i2p_actions_popup_show_below(void *popup, void *anchor) {
    if (auto *widget = static_cast<ActionsPopupWidget *>(as_widget(popup))) {
        widget->showBelow(as_widget(anchor));
    }
}

void *i2p_spin_row_new(int min, int max, int value, const char *suffix) {
    auto *row = new SpinRowWidget();
    row->spin()->setRange(min, max);
    row->spin()->setValue(value);
    row->spin()->setSuffix(qstr(suffix));
    return row;
}

int i2p_spin_row_value(void *row) {
    if (auto *widget = static_cast<SpinRowWidget *>(as_widget(row))) {
        return widget->spin()->value();
    }
    return 0;
}

void i2p_spin_row_set_value(void *row, int value) {
    if (auto *widget = static_cast<SpinRowWidget *>(as_widget(row))) {
        widget->spin()->setValue(value);
    }
}

void i2p_spin_row_set_suffix(void *row, const char *suffix) {
    if (auto *widget = static_cast<SpinRowWidget *>(as_widget(row))) {
        widget->spin()->setSuffix(qstr(suffix));
    }
}

void i2p_spin_row_on_changed(void *row, i2p_int_cb cb, void *ctx) {
    if (auto *widget = static_cast<SpinRowWidget *>(as_widget(row))) {
        widget->on_changed = [cb, ctx](int value) {
            if (cb) {
                cb(ctx, value);
            }
        };
    }
}

void i2p_install_rounded_tooltips(void) {
    if (QApplication *app = qApp) {
        if (g_tooltip_filter == nullptr) {
            g_tooltip_filter = new TooltipInterceptFilter();
            app->installEventFilter(g_tooltip_filter);
        }
    }
}

void i2p_apply_tooltip_palette(const char *theme) {
    QApplication *app = qApp;
    if (app == nullptr) {
        return;
    }
    const bool night = qstr(theme) == QLatin1String("night");
    const QColor bg(night ? "#2c2c2e" : "#f2f2f7");
    const QColor fg(night ? "#f5f5f7" : "#1d1d1f");
    QPalette pal = app->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        pal.setColor(group, QPalette::ToolTipBase, bg);
        pal.setColor(group, QPalette::ToolTipText, fg);
    }
    app->setPalette(pal);
}

int i2p_settings_exec(void *parent, const i2p_settings_in *in) {
    if (in == nullptr) {
        return 0;
    }
    QWidget *host = as_widget(parent);
    QDialog dialog(nullptr, hosted_dialog_flags());
    prepare_hosted_dialog(&dialog);
    dialog.setWindowTitle(qstr(in->title));
    dialog.setFixedWidth(DialogMetrics::kSettingsW);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }

    const QMargins margins = DialogMetrics::dialog_margins(18, 18);
    const int inner_w = DialogMetrics::inner_w(DialogMetrics::kSettingsW, margins);
    const bool night = stylesheet_is_night(dialog.styleSheet());

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(margins);
    layout->setSpacing(10);

    auto *rpc_label = new QLabel(qstr(in->rpc_label), &dialog);
    rpc_label->setToolTip(qstr(in->rpc_tip));
    layout->addWidget(rpc_label);
    auto *rpc_row = new QFrame(&dialog);
    rpc_row->setObjectName(QStringLiteral("PathRow"));
    rpc_row->setFrameShape(QFrame::NoFrame);
    lock_dialog_control(rpc_row);
    rpc_row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *rpc_layout = new QHBoxLayout(rpc_row);
    rpc_layout->setContentsMargins(0, 0, 0, 0);
    rpc_layout->setSpacing(0);
    auto *rpc = new QLineEdit(qstr(in->rpc_value), rpc_row);
    rpc->setObjectName(QStringLiteral("ReadOnlyPath"));
    rpc->setReadOnly(true);
    rpc->setFocusPolicy(Qt::NoFocus);
    rpc->setPlaceholderText(qstr(in->rpc_placeholder));
    rpc->setToolTip(qstr(in->rpc_tip));
    rpc->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rpc->setTextMargins(4, 0, 4, 0);
    rpc->setFrame(false);
    rpc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
#ifdef Q_OS_MAC
    rpc->setAttribute(Qt::WA_MacNormalSize, true);
    rpc->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
#endif
    rpc_layout->addWidget(rpc, 1);
    layout->addWidget(rpc_row);

    auto *dir_label = new QLabel(qstr(in->dir_label), &dialog);
    dir_label->setToolTip(qstr(in->dir_tip));
    layout->addWidget(dir_label);
    auto *dir_row = new QWidget(&dialog);
    dir_row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dir_row->setFixedHeight(DialogMetrics::kControlMinH);
#ifdef Q_OS_MAC
    dir_row->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
#endif
    auto *dir_layout = new QHBoxLayout(dir_row);
    dir_layout->setContentsMargins(0, 0, 0, 0);
    dir_layout->setSpacing(8);
    auto *path_row = new QFrame(dir_row);
    path_row->setObjectName(QStringLiteral("PathRow"));
    path_row->setFrameShape(QFrame::NoFrame);
    lock_dialog_control(path_row);
    path_row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *path_layout = new QHBoxLayout(path_row);
    path_layout->setContentsMargins(0, 0, 0, 0);
    path_layout->setSpacing(0);
    auto *dir = new QLineEdit(qstr(in->dir_value), path_row);
    dir->setObjectName(QStringLiteral("ReadOnlyPath"));
    dir->setReadOnly(true);
    dir->setFocusPolicy(Qt::NoFocus);
    dir->setToolTip(qstr(in->dir_tip));
    dir->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    dir->setTextMargins(4, 0, 4, 0);
    dir->setFrame(false);
    dir->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
#ifdef Q_OS_MAC
    dir->setAttribute(Qt::WA_MacNormalSize, true);
    dir->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
#endif
    path_layout->addWidget(dir, 1);
    dir_layout->addWidget(path_row, 1, Qt::AlignTop);
    auto *browse = new QPushButton(qstr(in->browse), dir_row);
    lock_dialog_control(browse);
    browse->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    browse->setToolTip(qstr(in->dir_tip));
    QObject::connect(browse, &QPushButton::clicked, &dialog, [dir, &dialog, in] {
        const QString path = dir->text().trimmed();
        if (path.isEmpty() || !QFileInfo(path).exists()) {
            QMessageBox::information(&dialog, qstr(in->dir_label), qstr(in->open_dir_missing));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    dir_layout->addWidget(browse, 0, Qt::AlignTop);
    layout->addWidget(dir_row);

    layout->addWidget(new QLabel(qstr(in->refresh_label), &dialog));
    auto *refresh = new SpinRowWidget(&dialog);
    refresh->spin()->setRange(2, 60);
    refresh->spin()->setValue(std::clamp(in->refresh_value, 2, 60));
    refresh->spin()->setSuffix(QStringLiteral(" ") + qstr(in->seconds_suffix));
    layout->addWidget(refresh);

    layout->addWidget(new QLabel(qstr(in->lang_label), &dialog));
    auto *language = settings_combo(&dialog, night, in->lang_en, "en", in->lang_ru, "ru", in->lang_current);
    layout->addWidget(language->parentWidget());

    layout->addWidget(new QLabel(qstr(in->theme_label), &dialog));
    auto *theme = settings_combo(&dialog, night, in->theme_light, "light", in->theme_night, "night", in->theme_current);
    layout->addWidget(theme->parentWidget());

    layout->addWidget(new QLabel(qstr(in->view_label), &dialog));
    auto *view = settings_combo(&dialog, night, in->view_simple, "simple", in->view_detailed, "detailed",
                                in->view_current);
    layout->addWidget(view->parentWidget());

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    note->setToolTip(qstr(in->rpc_tip));
    lock_wrapped_label(note, inner_w);
    layout->addWidget(note);
    layout->addStretch(1);

    auto *cancel = new QPushButton(qstr(in->cancel), &dialog);
    auto *save = new QPushButton(qstr(in->save), &dialog);
    save->setObjectName(QStringLiteral("Primary"));
    save->setDefault(true);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
    add_dialog_buttons(layout, {cancel, save});

    exec_app_modal_dialog(&dialog, host, inner_w, QSize(DialogMetrics::kSettingsW, 0), [&] {
        lock_wrapped_label(note, inner_w);
        lock_dialog_control(rpc_row);
        dir_row->setMinimumWidth(inner_w);
        lock_dialog_control(path_row);
        lock_dialog_control(browse);
    });
    if (dialog.result() != QDialog::Accepted) {
        return 0;
    }
    g_settings_rpc = rpc->text().toStdString();
    g_settings_dir = dir->text().toStdString();
    g_settings_refresh = refresh->spin()->value();
    g_settings_language = combo_data_string(language);
    g_settings_theme = combo_data_string(theme);
    g_settings_view = combo_data_string(view);
    return 1;
}

const char *i2p_settings_rpc(void) { return g_settings_rpc.c_str(); }
const char *i2p_settings_dir(void) { return g_settings_dir.c_str(); }
int i2p_settings_refresh(void) { return g_settings_refresh; }
const char *i2p_settings_language(void) { return g_settings_language.c_str(); }
const char *i2p_settings_theme(void) { return g_settings_theme.c_str(); }
const char *i2p_settings_view(void) { return g_settings_view.c_str(); }

const char *i2p_open_file(void *parent, const char *title, const char *filter) {
    g_open_file.clear();
    const QString path = QFileDialog::getOpenFileName(as_widget(parent), qstr(title), QString(),
                                                     qstr(filter));
    if (!path.isEmpty()) {
        g_open_file = path.toStdString();
    }
    return g_open_file.c_str();
}

int i2p_create_torrent_exec(void *parent, const i2p_create_torrent_in *in) {
    if (in == nullptr) {
        return 0;
    }
    g_create_torrent_path.clear();
    g_create_torrent_add_after = 0;

    QWidget *host = as_widget(parent);
    QDialog dialog(nullptr, hosted_dialog_flags());
    prepare_hosted_dialog(&dialog);
    dialog.setWindowTitle(qstr(in->title));
    dialog.setFixedWidth(DialogMetrics::kCreateW);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }

    const QMargins margins = DialogMetrics::dialog_margins(18, 18);
    const int inner_w = DialogMetrics::inner_w(DialogMetrics::kCreateW, margins);
    const bool night = stylesheet_is_night(dialog.styleSheet());

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(margins);
    layout->setSpacing(10);

    layout->addWidget(new QLabel(qstr(in->source_label), &dialog));
    auto *source_row = new QWidget(&dialog);
    source_row->setFixedHeight(DialogMetrics::kControlMinH);
    auto *source_layout = new QHBoxLayout(source_row);
    source_layout->setContentsMargins(0, 0, 0, 0);
    source_layout->setSpacing(8);
    auto *source_frame = new QFrame(source_row);
    source_frame->setObjectName(QStringLiteral("PathRow"));
    lock_dialog_control(source_frame);
    auto *source_frame_layout = new QHBoxLayout(source_frame);
    source_frame_layout->setContentsMargins(0, 0, 0, 0);
    auto *source = new QLineEdit(qstr(in->source_value), source_frame);
    source->setFrame(false);
    source->setTextMargins(4, 0, 4, 0);
    source_frame_layout->addWidget(source, 1);
    source_layout->addWidget(source_frame, 1);
    auto *browse_file = new QPushButton(qstr(in->browse_file), source_row);
    auto *browse_folder = new QPushButton(qstr(in->browse_folder), source_row);
    lock_dialog_control(browse_file);
    lock_dialog_control(browse_folder);
    source_layout->addWidget(browse_file);
    source_layout->addWidget(browse_folder);
    layout->addWidget(source_row);

    layout->addWidget(new QLabel(qstr(in->trackers_label), &dialog));
    auto *trackers = new QTextEdit(&dialog);
    trackers->setObjectName(QStringLiteral("CreateTrackers"));
    trackers->setAcceptRichText(false);
    trackers->setFrameShape(QFrame::NoFrame);
    trackers->setPlainText(qstr(in->trackers_value));
    trackers->setFixedHeight(72);
    trackers->setTabChangesFocus(true);
    layout->addWidget(trackers);

    layout->addWidget(new QLabel(qstr(in->piece_label), &dialog));
    auto *piece = settings_combo(&dialog, night, in->piece_auto, "0", in->piece_auto, "0", "0");
    // Rebuild piece size options properly
    {
        QComboBox *combo = piece;
        combo->clear();
        combo->addItem(qstr(in->piece_auto), QStringLiteral("0"));
        const quint32 sizes[] = {16u, 32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u, 16384u};
        for (quint32 kib : sizes) {
            const QString label =
                kib >= 1024u ? QStringLiteral("%1 MiB").arg(kib / 1024u) : QStringLiteral("%1 KiB").arg(kib);
            combo->addItem(label, QString::number(kib * 1024u));
        }
        combo->setCurrentIndex(0);
    }
    layout->addWidget(piece->parentWidget());

    auto *is_private = new QCheckBox(qstr(in->private_label), &dialog);
    layout->addWidget(is_private);

    layout->addWidget(new QLabel(qstr(in->comment_label), &dialog));
    auto *comment = new QLineEdit(qstr(in->comment_value), &dialog);
    polish_line_edit(comment);
    layout->addWidget(comment);

    layout->addWidget(new QLabel(qstr(in->save_label), &dialog));
    auto *save_row = new QWidget(&dialog);
    save_row->setFixedHeight(DialogMetrics::kControlMinH);
    auto *save_layout = new QHBoxLayout(save_row);
    save_layout->setContentsMargins(0, 0, 0, 0);
    save_layout->setSpacing(8);
    auto *save_frame = new QFrame(save_row);
    save_frame->setObjectName(QStringLiteral("PathRow"));
    lock_dialog_control(save_frame);
    auto *save_frame_layout = new QHBoxLayout(save_frame);
    save_frame_layout->setContentsMargins(0, 0, 0, 0);
    auto *save_path = new QLineEdit(qstr(in->save_value), save_frame);
    save_path->setFrame(false);
    save_path->setTextMargins(4, 0, 4, 0);
    save_frame_layout->addWidget(save_path, 1);
    save_layout->addWidget(save_frame, 1);
    auto *browse_save = new QPushButton(qstr(in->browse_save), save_row);
    lock_dialog_control(browse_save);
    save_layout->addWidget(browse_save);
    layout->addWidget(save_row);

    auto *add_after = new QCheckBox(qstr(in->add_after_label), &dialog);
    add_after->setChecked(in->add_after_default != 0);
    layout->addWidget(add_after);

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    lock_wrapped_label(note, inner_w);
    layout->addWidget(note);

    auto *progress = new QProgressBar(&dialog);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setTextVisible(true);
    progress->setVisible(false);
    layout->addWidget(progress);

    auto *status = new QLabel(&dialog);
    status->setObjectName(QStringLiteral("Secondary"));
    status->setVisible(false);
    layout->addWidget(status);

    auto *cancel = new QPushButton(qstr(in->cancel), &dialog);
    auto *create = new QPushButton(qstr(in->create), &dialog);
    create->setObjectName(QStringLiteral("Primary"));
    create->setDefault(true);
    add_dialog_buttons(layout, {cancel, create});

    QObject::connect(browse_file, &QPushButton::clicked, &dialog, [source, &dialog, in] {
        const QString path =
            QFileDialog::getOpenFileName(&dialog, qstr(in->source_label), source->text());
        if (!path.isEmpty()) {
            source->setText(path);
        }
    });
    QObject::connect(browse_folder, &QPushButton::clicked, &dialog, [source, &dialog, in] {
        const QString path =
            QFileDialog::getExistingDirectory(&dialog, qstr(in->source_label), source->text());
        if (!path.isEmpty()) {
            source->setText(path);
        }
    });
    QObject::connect(browse_save, &QPushButton::clicked, &dialog, [save_path, source, &dialog, in] {
        QString suggestion = save_path->text();
        if (suggestion.isEmpty()) {
            const QFileInfo info(source->text());
            if (!info.fileName().isEmpty()) {
                suggestion = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() +
                             QStringLiteral(".torrent");
            }
        }
        const QString path = QFileDialog::getSaveFileName(
            &dialog, qstr(in->save_label), suggestion, QStringLiteral("*.torrent"));
        if (!path.isEmpty()) {
            save_path->setText(path.endsWith(QStringLiteral(".torrent"), Qt::CaseInsensitive)
                                   ? path
                                   : path + QStringLiteral(".torrent"));
        }
    });

    std::atomic_bool hashing{false};
    std::atomic_bool cancel_hash{false};
    QObject::connect(cancel, &QPushButton::clicked, &dialog, [&] {
        if (hashing.load()) {
            cancel_hash.store(true);
            return;
        }
        dialog.reject();
    });

    QObject::connect(create, &QPushButton::clicked, &dialog, [&] {
        if (hashing.load()) {
            return;
        }
        const QString src = source->text().trimmed();
        QString out = save_path->text().trimmed();
        if (src.isEmpty() || !QFileInfo::exists(src)) {
            QMessageBox::warning(&dialog, qstr(in->title), qstr(in->need_source));
            return;
        }
        if (out.isEmpty()) {
            const QFileInfo info(src);
            out = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() +
                  QStringLiteral(".torrent");
            save_path->setText(out);
        }
        if (out.trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, qstr(in->title), qstr(in->need_save));
            return;
        }

        i2p::CreateTorrentRequest req;
        req.sourcePath = src;
        req.trackers = trackers->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        req.pieceSize = static_cast<quint32>(combo_data_string(piece).empty()
                                                 ? 0
                                                 : QString::fromStdString(combo_data_string(piece)).toUInt());
        req.isPrivate = is_private->isChecked();
        req.comment = comment->text();
        req.createdBy = QStringLiteral("I2P Torrents");

        hashing.store(true);
        cancel_hash.store(false);
        create->setEnabled(false);
        source->setEnabled(false);
        browse_file->setEnabled(false);
        browse_folder->setEnabled(false);
        trackers->setEnabled(false);
        piece->setEnabled(false);
        is_private->setEnabled(false);
        comment->setEnabled(false);
        save_path->setEnabled(false);
        browse_save->setEnabled(false);
        add_after->setEnabled(false);
        progress->setVisible(true);
        progress->setValue(0);
        status->setVisible(true);
        status->setText(qstr(in->hashing));

        std::optional<QByteArray> metainfo;
        QString error;
        QEventLoop loop;
        QThread *thread = QThread::create([&] {
            metainfo = i2p::createTorrentMetainfo(
                req,
                &error,
                &cancel_hash,
                [&](const i2p::CreateTorrentProgress &p) {
                    const int pct =
                        p.pieceCount == 0
                            ? 0
                            : static_cast<int>((p.pieceIndex * 100ull) / p.pieceCount);
                    QMetaObject::invokeMethod(
                        progress,
                        [progress, status, in, pct, p] {
                            progress->setValue(std::clamp(pct, 0, 100));
                            status->setText(QStringLiteral("%1 (%2/%3)")
                                                .arg(qstr(in->hashing))
                                                .arg(p.pieceIndex)
                                                .arg(p.pieceCount));
                        },
                        Qt::QueuedConnection);
                });
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        });
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
        loop.exec();
        hashing.store(false);

        if (!metainfo.has_value()) {
            create->setEnabled(true);
            source->setEnabled(true);
            browse_file->setEnabled(true);
            browse_folder->setEnabled(true);
            trackers->setEnabled(true);
            piece->setEnabled(true);
            is_private->setEnabled(true);
            comment->setEnabled(true);
            save_path->setEnabled(true);
            browse_save->setEnabled(true);
            add_after->setEnabled(true);
            progress->setVisible(false);
            status->setVisible(false);
            if (error != QStringLiteral("Cancelled")) {
                QMessageBox::warning(&dialog,
                                     qstr(in->create_failed),
                                     error.isEmpty() ? qstr(in->create_failed) : error);
            }
            return;
        }

        QString saveError;
        if (!i2p::saveTorrentMetainfo(*metainfo, out, &saveError)) {
            create->setEnabled(true);
            source->setEnabled(true);
            browse_file->setEnabled(true);
            browse_folder->setEnabled(true);
            trackers->setEnabled(true);
            piece->setEnabled(true);
            is_private->setEnabled(true);
            comment->setEnabled(true);
            save_path->setEnabled(true);
            browse_save->setEnabled(true);
            add_after->setEnabled(true);
            progress->setVisible(false);
            status->setVisible(false);
            QMessageBox::warning(&dialog, qstr(in->create_failed), saveError);
            return;
        }

        g_create_torrent_path = out.toStdString();
        g_create_torrent_add_after = add_after->isChecked() ? 1 : 0;
        dialog.accept();
    });

    exec_app_modal_dialog(&dialog, host, inner_w, QSize(DialogMetrics::kCreateW, 0), [&] {
        lock_wrapped_label(note, inner_w);
        lock_dialog_control(source_frame);
        lock_dialog_control(save_frame);
        lock_dialog_control(browse_file);
        lock_dialog_control(browse_folder);
        lock_dialog_control(browse_save);
    });
    return dialog.result() == QDialog::Accepted ? 1 : 0;
}

const char *i2p_create_torrent_path(void) { return g_create_torrent_path.c_str(); }
int i2p_create_torrent_add_after(void) { return g_create_torrent_add_after; }

static bool stylesheet_is_night(const char *stylesheet) {
    if (stylesheet == nullptr || stylesheet[0] == '\0') {
        return false;
    }
    return qstr(stylesheet).contains(QStringLiteral("#f5f5f7"));
}

static QColor about_link_color(bool night) {
    return night ? QColor(0x9a, 0xa3, 0xb5) : QColor(0x62, 0x68, 0x75);
}

void i2p_about_exec(void *parent, const i2p_about_in *in) {
    if (in == nullptr) {
        return;
    }
    QWidget *host = as_widget(parent);
    QDialog dialog(nullptr, hosted_dialog_flags());
    prepare_hosted_dialog(&dialog);
    dialog.setWindowTitle(qstr(in->title));
    dialog.setMinimumWidth(DialogMetrics::kAboutW);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }
    const QMargins margins = DialogMetrics::dialog_margins(16, 16);
    const int inner_w = DialogMetrics::inner_w(DialogMetrics::kAboutW, margins);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(margins);
    layout->setSpacing(8);

    auto *heading = new QLabel(qstr(in->heading), &dialog);
    heading->setObjectName(QStringLiteral("Title"));
    layout->addWidget(heading);

    auto *body = new QLabel(qstr(in->body), &dialog);
    body->setObjectName(QStringLiteral("Secondary"));
    body->setWordWrap(true);
    layout->addWidget(body);

    auto *github = new QLabel(&dialog);
    github->setObjectName(QStringLiteral("AboutLink"));
    github->setOpenExternalLinks(true);
    github->setTextInteractionFlags(Qt::TextBrowserInteraction);
    github->setWordWrap(true);
    const bool night = stylesheet_is_night(in->stylesheet);
    const QColor link_color = about_link_color(night);
    QPalette link_palette = github->palette();
    link_palette.setColor(QPalette::Link, link_color);
    link_palette.setColor(QPalette::LinkVisited, link_color);
    github->setPalette(link_palette);
    github->setText(QStringLiteral("%1: <a href=\"%2\" style=\"color: %3; text-decoration: underline;\">%2</a>")
                        .arg(qstr(in->github_label).toHtmlEscaped(), qstr(in->github_url).toHtmlEscaped(),
                             link_color.name()));
    layout->addWidget(github);

    auto *donate = new QLabel(qstr(in->donate_label), &dialog);
    donate->setObjectName(QStringLiteral("Secondary"));
    donate->setWordWrap(true);
    layout->addWidget(donate);

    if (in->qr_path && in->qr_path[0] != '\0') {
        QPixmap qr(qstr(in->qr_path));
        if (!qr.isNull()) {
            auto *qr_label = new QLabel(&dialog);
            qr_label->setAlignment(Qt::AlignCenter);
            qr_label->setPixmap(qr.scaled(DialogMetrics::kQrSide, DialogMetrics::kQrSide, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
            layout->addWidget(qr_label, 0, Qt::AlignHCenter);
        }
    }

    auto *address = new QLabel(qstr(in->donate_address), &dialog);
    address->setObjectName(QStringLiteral("Secondary"));
    address->setWordWrap(true);
    address->setTextInteractionFlags(Qt::TextSelectableByMouse);
    address->setAlignment(Qt::AlignHCenter);
    layout->addWidget(address);

    auto *ok = new QPushButton(qstr(in->ok), &dialog);
    ok->setObjectName(QStringLiteral("Primary"));
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    add_dialog_buttons(layout, {ok});

    exec_app_modal_dialog(&dialog, host, inner_w, QSize(DialogMetrics::kAboutW, 0));
}

static int file_combo_index(int wanted, int priority) {
    if (wanted == 0) {
        return 0;
    }
    if (priority < 0) {
        return 1;
    }
    if (priority > 0) {
        return 3;
    }
    return 2;
}

void i2p_files_exec(void *parent, const i2p_files_in *in, i2p_file_change_cb cb, void *ctx) {
    if (in == nullptr) {
        return;
    }
    QWidget *host = as_widget(parent);
    QDialog dialog(nullptr, hosted_dialog_flags());
    prepare_hosted_dialog(&dialog);
#ifdef Q_OS_MAC
    // Solid chrome so rounded table wedges can match the dialog fill (no black ears on glass).
    dialog.setProperty("i2pOpaqueChrome", true);
#endif
    dialog.setWindowTitle(qstr(in->title));
    dialog.setFixedWidth(DialogMetrics::kFilesW);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }
    const bool night = stylesheet_is_night(dialog.styleSheet());
#ifdef Q_OS_MAC
    dialog.setStyleSheet(dialog.styleSheet() +
                         (night ? QStringLiteral("\nQDialog { background: #1c1c1e; }")
                                : QStringLiteral("\nQDialog { background: #ffffff; }")));
#endif
    const QMargins margins = DialogMetrics::dialog_margins(16, 16);
    const int inner_w = DialogMetrics::inner_w(DialogMetrics::kFilesW, margins);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(margins);
    layout->setSpacing(10);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    note->setWordWrap(true);
    layout->addWidget(note);

    QTableWidget *files_table = nullptr;
    std::vector<FilesPriorityButton *> priority_buttons;

    if (in->file_count <= 0) {
        auto *empty = new QLabel(qstr(in->empty), &dialog);
        empty->setObjectName(QStringLiteral("Secondary"));
        empty->setWordWrap(true);
        layout->addWidget(empty);
    } else {
        const bool night = stylesheet_is_night(dialog.styleSheet());
        auto *pane = new FilesTablePane(&dialog, night);
        auto *pane_layout = new QVBoxLayout(pane);
        pane_layout->setContentsMargins(0, 0, 0, 0);
        pane_layout->setSpacing(0);

        auto *table = new QTableWidget(in->file_count, 4, pane);
        table->setObjectName(QStringLiteral("FilesTable"));
        style_files_table(table, &dialog);
        table->setHorizontalHeaderLabels({qstr(in->col_name), qstr(in->col_size), qstr(in->col_progress),
                                          qstr(in->col_priority)});
        table->verticalHeader()->setVisible(false);
        table->setShowGrid(false);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->horizontalHeader()->setMinimumSectionSize(28);
#ifdef Q_OS_WIN
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
#else
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
#endif
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setMinimumSectionSize(40);

        const QStringList priority_labels = {qstr(in->priority_skip), qstr(in->priority_low),
                                             qstr(in->priority_normal), qstr(in->priority_high)};
        const int priority_button_w = files_priority_button_width(table->font(), priority_labels);

        files_table = table;
        priority_buttons.reserve(static_cast<size_t>(in->file_count));
        for (int row = 0; row < in->file_count; ++row) {
            const i2p_file_row &file = in->files[row];
            const QString name = qstr(file.name);
            const QString full_name = qstr(file.full_name);
            const QString tip = full_name.isEmpty() ? name : full_name;
            auto *name_label = new FilesNameLabel(name, tip, table);
            table->setCellWidget(row, 0, name_label);

            auto add_text = [&](int column, const char *text) {
                auto *item = new QTableWidgetItem(qstr(text));
                item->setFlags(Qt::ItemIsEnabled);
                item->setToolTip(tip);
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                table->setItem(row, column, item);
            };
            add_text(1, file.size);
            add_text(2, file.progress);

            const int start = file_combo_index(file.wanted, file.priority);
            auto *priority = new FilesPriorityButton(priority_labels, start, table);
            if (QStyle *style = table->style()) {
                priority->setStyle(style);
            }
            priority->setCompactWidth(priority_button_w);
            priority->setToolTip(tip);
            priority->setProperty("prev", start);
            priority->setProperty("fileIndex", file.index);

            auto *priority_wrap = new QWidget(table);
            priority_wrap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            priority_wrap->setFixedWidth(priority_button_w);
            auto *wrap_layout = new QHBoxLayout(priority_wrap);
            wrap_layout->setContentsMargins(0, 0, 0, 0);
            wrap_layout->setSpacing(0);
            wrap_layout->addWidget(priority);

            auto *priority_cell = new QWidget(table);
            priority_cell->setObjectName(QStringLiteral("FilesPriorityCell"));
            priority_cell->setAttribute(Qt::WA_TranslucentBackground, false);
            priority_cell->setAutoFillBackground(false);
            auto *priority_layout = new QHBoxLayout(priority_cell);
            priority_layout->setContentsMargins(0, 4, DialogMetrics::kFilesPriorityRightPad, 4);
            priority_layout->setSpacing(0);
            priority_layout->addWidget(priority_wrap, 0, Qt::AlignLeft | Qt::AlignVCenter);
            priority_layout->addStretch(1);
            table->setCellWidget(row, 3, priority_cell);
            priority_buttons.push_back(priority);
        }

        const QString unsupported = qstr(in->unsupported_note);
        for (FilesPriorityButton *button : priority_buttons) {
            button->on_changed = [button, &priority_buttons, note, unsupported, cb, ctx](int index) {
                if (cb == nullptr) {
                    return;
                }
                const int wanted = index == 0 ? 0 : 1;
                int priority = 0;
                if (index == 1) {
                    priority = -1;
                } else if (index == 3) {
                    priority = 1;
                }
                const int rc = cb(ctx, button->property("fileIndex").toInt(), wanted, priority);
                if (rc == 0) {
                    button->setProperty("prev", index);
                    return;
                }
                button->revertTo(button->property("prev").toInt());
                if (rc == 1) {
                    note->setText(unsupported);
                    for (FilesPriorityButton *item : priority_buttons) {
                        item->setEnabled(false);
                    }
                }
            };
        }
        pane_layout->addWidget(table);
        layout->addWidget(pane);
    }

    auto *close = new QPushButton(qstr(in->close), &dialog);
    close->setObjectName(QStringLiteral("Primary"));
    close->setDefault(true);
    QObject::connect(close, &QPushButton::clicked, &dialog, [&dialog] {
        if (g_tip) {
            g_tip->forceHide();
        }
        QTimer::singleShot(0, &dialog, &QDialog::accept);
    });
    add_dialog_buttons(layout, {close});

    exec_app_modal_dialog(
        &dialog, host, inner_w, QSize(DialogMetrics::kFilesW, 0),
        [&] {
            if (files_table != nullptr) {
                layout_files_table(files_table, layout, DialogMetrics::kFilesW, DialogMetrics::kFilesH, note,
                                   dialog_buttons_row_height(close), qstr(in->col_progress));
            }
            layout->activate();
            const int height = std::min(DialogMetrics::kFilesH, dialog.sizeHint().height());
            dialog.setFixedSize(DialogMetrics::kFilesW, std::max(height, 160));
        });
    if (g_tip) {
        g_tip->forceHide();
    }
}

static int peers_table_content_width(QTableWidget *table, int column, const QFontMetrics &metrics,
                                     const QString &floor_sample, int pad) {
    int width = metrics.horizontalAdvance(floor_sample);
    if (table->horizontalHeaderItem(column) != nullptr) {
        width = std::max(width, metrics.horizontalAdvance(table->horizontalHeaderItem(column)->text()));
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        if (QTableWidgetItem *item = table->item(row, column)) {
            width = std::max(width, metrics.horizontalAdvance(item->text()));
        }
    }
    return width + pad;
}

int layout_peers_table(QTableWidget *table, QVBoxLayout *layout, int dialog_w, int dialog_h, QLabel *note,
                       int button_row_h) {
    if (table == nullptr || layout == nullptr || note == nullptr) {
        return dialog_w > 0 ? dialog_w : DialogMetrics::kPeersW;
    }
    const QMargins margins = layout->contentsMargins();
    const int inner_w = DialogMetrics::inner_w(dialog_w, margins);
    const int spacing = layout->spacing();

    note->setFixedWidth(inner_w);
    int note_h = note->heightForWidth(inner_w);
    if (note_h <= 0) {
        note_h = wrapped_label_height(note, inner_w);
    }
    if (note_h > 0) {
        note->setFixedHeight(note_h);
    }

    const int chrome_h = margins.top() + margins.bottom() + note_h + button_row_h + spacing * 2;
    const int max_dialog_h = dialog_h > 0 ? dialog_h : DialogMetrics::kFilesH;
    const int max_table_h = std::max(120, max_dialog_h - chrome_h);

    QHeaderView *header = table->horizontalHeader();
    header->setHighlightSections(false);
    header->setFixedHeight(std::max(DialogMetrics::kFilesHeaderMinH, header->sizeHint().height()));
    const int header_h = header->height();

    const int table_inner =
        std::max(80, inner_w - DialogMetrics::kFilesTableEdge * 2);
    const QFont hash_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QFontMetrics hash_metrics(hash_font);
    const QFontMetrics metrics(table->fontMetrics());
    constexpr int cell_pad = 16;

    int col_address = peers_table_content_width(table, 0, hash_metrics,
                                                QStringLiteral("HUK-AbCd1234~EfGh5678-IjKl9012"), cell_pad);
    int col_client =
        peers_table_content_width(table, 1, metrics, QStringLiteral("BiglyBT"), cell_pad);
    int col_down =
        peers_table_content_width(table, 2, metrics, QStringLiteral("999.9 KiB/s"), cell_pad);
    int col_up =
        peers_table_content_width(table, 3, metrics, QStringLiteral("999.9 KiB/s"), cell_pad);
    int col_flags =
        peers_table_content_width(table, 4, metrics, QStringLiteral("IDU?"), cell_pad);
    int col_progress =
        peers_table_content_width(table, 5, metrics, QStringLiteral("100%"), cell_pad);
    col_down = std::max(col_down, 96);
    col_up = std::max(col_up, 96);
    col_flags = std::max(col_flags, 40);
    col_progress = std::max(col_progress, 56);

    const int content_w = col_address + col_client + col_down + col_up + col_flags + col_progress;

#ifndef Q_OS_WIN
    const int v_scrollbar_w = table->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, table);
#else
    const int v_scrollbar_w = 0;
#endif

    int rows_h = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        rows_h += table->rowHeight(row);
    }

    const int content_table_h = header_h + rows_h + DialogMetrics::kFilesTableEdge * 2;
    const bool need_v_scroll = content_table_h > max_table_h;
    const int v_scroll_reserve = need_v_scroll ? v_scrollbar_w : 0;
    const int viewport_w = table_inner - v_scroll_reserve;
    const bool need_h_scroll = content_w > viewport_w;

    if (!need_h_scroll) {
        const int slack = viewport_w - content_w;
        col_down += slack / 2;
        col_up += slack - slack / 2;
    }

    table->setColumnWidth(0, col_address);
    table->setColumnWidth(1, col_client);
    table->setColumnWidth(2, col_down);
    table->setColumnWidth(3, col_up);
    table->setColumnWidth(4, col_flags);
    table->setColumnWidth(5, col_progress);

    const int table_h = need_v_scroll ? max_table_h : content_table_h;
    table->setFixedSize(inner_w, table_h);
    table->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    table->setMaximumHeight(table_h);
    table->setHorizontalScrollBarPolicy(need_h_scroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(need_v_scroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (QWidget *parent = table->parentWidget()) {
        if (parent->objectName() == QLatin1String("FilesTablePane") ||
            parent->objectName() == QLatin1String("PeersTablePane") ||
            parent->objectName() == QLatin1String("TrackersTablePane")) {
            parent->setFixedSize(inner_w, table_h);
            parent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            static_cast<FilesTablePane *>(parent)->applyClip();
        }
    }

    const int preferred_dialog_w = std::clamp(content_w + margins.left() + margins.right() + v_scroll_reserve,
                                              DialogMetrics::kPeersW, DialogMetrics::kPeersMaxW);
    return preferred_dialog_w;
}

void i2p_peers_exec(void *parent, const i2p_peers_in *in) {
    if (in == nullptr) {
        return;
    }
    QWidget *host = as_widget(parent);
    QDialog dialog(nullptr, hosted_dialog_flags());
    prepare_hosted_dialog(&dialog);
#ifdef Q_OS_MAC
    dialog.setProperty("i2pOpaqueChrome", true);
#endif
    dialog.setWindowTitle(qstr(in->title));
    dialog.setFixedWidth(DialogMetrics::kPeersW);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }
    const bool night = stylesheet_is_night(dialog.styleSheet());
#ifdef Q_OS_MAC
    dialog.setStyleSheet(dialog.styleSheet() +
                         (night ? QStringLiteral("\nQDialog { background: #1c1c1e; }")
                                : QStringLiteral("\nQDialog { background: #ffffff; }")));
#endif
    const QMargins margins = DialogMetrics::dialog_margins(16, 16);
    const int inner_w = DialogMetrics::inner_w(DialogMetrics::kPeersW, margins);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(margins);
    layout->setSpacing(10);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    note->setWordWrap(true);
    layout->addWidget(note);

    QTableWidget *peers_table = nullptr;
    std::vector<QString> copy_texts;

    if (in->peer_count <= 0) {
        auto *empty = new QLabel(qstr(in->empty), &dialog);
        empty->setObjectName(QStringLiteral("Secondary"));
        empty->setWordWrap(true);
        layout->addWidget(empty);
    } else {
        auto *pane = new FilesTablePane(&dialog, night);
        pane->setObjectName(QStringLiteral("PeersTablePane"));
        auto *pane_layout = new QVBoxLayout(pane);
        pane_layout->setContentsMargins(0, 0, 0, 0);
        pane_layout->setSpacing(0);

        auto *table = new QTableWidget(in->peer_count, 6, pane);
        table->setObjectName(QStringLiteral("PeersTable"));
        style_peers_table(table, &dialog);
        table->setHorizontalHeaderLabels({qstr(in->col_address), qstr(in->col_client), qstr(in->col_down),
                                          qstr(in->col_up), qstr(in->col_flags), qstr(in->col_progress)});
        table->verticalHeader()->setVisible(false);
        table->setShowGrid(false);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setMinimumSectionSize(28);
        table->horizontalHeader()->setStretchLastSection(false);
        for (int column = 0; column < 6; ++column) {
            table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
        }
        if (QTableWidgetItem *down_hdr = table->horizontalHeaderItem(2)) {
            down_hdr->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (QTableWidgetItem *up_hdr = table->horizontalHeaderItem(3)) {
            up_hdr->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (QTableWidgetItem *flags_hdr = table->horizontalHeaderItem(4)) {
            flags_hdr->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        if (QTableWidgetItem *progress_hdr = table->horizontalHeaderItem(5)) {
            progress_hdr->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }

        const QFont hash_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        copy_texts.reserve(static_cast<size_t>(in->peer_count));
        for (int row = 0; row < in->peer_count; ++row) {
            const i2p_peer_row &peer = in->peers[row];
            const QString address = qstr(peer.address);
            const QString tip = qstr(peer.address_tip);
            copy_texts.push_back(tip.isEmpty() ? address : tip);

            auto *address_item = new QTableWidgetItem(address);
            address_item->setFont(hash_font);
            address_item->setToolTip(tip.isEmpty() ? address : tip);
            table->setItem(row, 0, address_item);

            const QString client = qstr(peer.client);
            auto *client_item = new QTableWidgetItem(client);
            client_item->setToolTip(client);
            table->setItem(row, 1, client_item);

            const QString rate_down = qstr(peer.rate_down);
            auto *down_item = new QTableWidgetItem(rate_down);
            down_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            down_item->setToolTip(rate_down);
            table->setItem(row, 2, down_item);

            const QString rate_up = qstr(peer.rate_up);
            auto *up_item = new QTableWidgetItem(rate_up);
            up_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            up_item->setToolTip(rate_up);
            table->setItem(row, 3, up_item);

            const QString flags = qstr(peer.flags);
            const QString flags_label = flags.isEmpty() ? QStringLiteral("—") : flags;
            auto *flags_item = new QTableWidgetItem(flags_label);
            flags_item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            if (!flags.isEmpty()) {
                flags_item->setToolTip(flags);
            }
            table->setItem(row, 4, flags_item);

            const QString progress = qstr(peer.progress);
            auto *progress_item = new QTableWidgetItem(progress.isEmpty() ? QStringLiteral("—") : progress);
            progress_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (!progress.isEmpty()) {
                progress_item->setToolTip(progress);
            }
            table->setItem(row, 5, progress_item);
        }

        QObject::connect(table, &QTableWidget::cellDoubleClicked, &dialog,
                         [table, texts = copy_texts](int row, int column) {
                             if (column != 0 || row < 0 || row >= static_cast<int>(texts.size())) {
                                 return;
                             }
                             if (QClipboard *clipboard = QApplication::clipboard()) {
                                 clipboard->setText(texts[static_cast<size_t>(row)]);
                             }
                         });

        new TableItemTooltipFilter(table);
        peers_table = table;
        pane_layout->addWidget(table);
        layout->addWidget(pane);
    }

    auto *close = new QPushButton(qstr(in->close), &dialog);
    close->setObjectName(QStringLiteral("Primary"));
    close->setDefault(true);
    QObject::connect(close, &QPushButton::clicked, &dialog, [&dialog] {
        if (g_tip) {
            g_tip->forceHide();
        }
        QTimer::singleShot(0, &dialog, &QDialog::accept);
    });
    add_dialog_buttons(layout, {close});

    exec_app_modal_dialog(
        &dialog, host, inner_w, QSize(DialogMetrics::kPeersW, 0),
        [&] {
            int dialog_w = DialogMetrics::kPeersW;
            if (peers_table != nullptr) {
                dialog_w = layout_peers_table(peers_table, layout, dialog_w, DialogMetrics::kFilesH, note,
                                              dialog_buttons_row_height(close));
                dialog_w = std::clamp(dialog_w, DialogMetrics::kPeersW, DialogMetrics::kPeersMaxW);
                if (dialog.width() != dialog_w) {
                    dialog.setFixedWidth(dialog_w);
                }
                layout_peers_table(peers_table, layout, dialog_w, DialogMetrics::kFilesH, note,
                                   dialog_buttons_row_height(close));
            }
            layout->activate();
            const int height = std::min(DialogMetrics::kFilesH, dialog.sizeHint().height());
            dialog.setFixedSize(dialog_w, std::max(height, 160));
        });
    if (g_tip) {
        g_tip->forceHide();
    }
}

void i2p_trackers_exec(void *parent, const i2p_trackers_in *in) {
    if (in == nullptr) {
        return;
    }
    QWidget *host = as_widget(parent);
    QDialog dialog(nullptr, hosted_dialog_flags());
    prepare_hosted_dialog(&dialog);
#ifdef Q_OS_MAC
    dialog.setProperty("i2pOpaqueChrome", true);
#endif
    dialog.setWindowTitle(qstr(in->title));
    dialog.setFixedWidth(DialogMetrics::kPeersW);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }
    const bool night = stylesheet_is_night(dialog.styleSheet());
#ifdef Q_OS_MAC
    dialog.setStyleSheet(dialog.styleSheet() +
                         (night ? QStringLiteral("\nQDialog { background: #1c1c1e; }")
                                : QStringLiteral("\nQDialog { background: #ffffff; }")));
#endif
    const QMargins margins = DialogMetrics::dialog_margins(16, 16);
    const int inner_w = DialogMetrics::inner_w(DialogMetrics::kPeersW, margins);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(margins);
    layout->setSpacing(10);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    note->setWordWrap(true);
    layout->addWidget(note);

    QTableWidget *trackers_table = nullptr;
    if (in->tracker_count <= 0) {
        auto *empty = new QLabel(qstr(in->empty), &dialog);
        empty->setObjectName(QStringLiteral("Secondary"));
        empty->setWordWrap(true);
        layout->addWidget(empty);
    } else {
        auto *pane = new FilesTablePane(&dialog, night);
        pane->setObjectName(QStringLiteral("TrackersTablePane"));
        auto *pane_layout = new QVBoxLayout(pane);
        pane_layout->setContentsMargins(0, 0, 0, 0);
        pane_layout->setSpacing(0);

        auto *table = new QTableWidget(in->tracker_count, 2, pane);
        table->setObjectName(QStringLiteral("TrackersTable"));
        style_peers_table(table, &dialog);
        table->setHorizontalHeaderLabels({qstr(in->col_announce), qstr(in->col_tier)});
        table->verticalHeader()->setVisible(false);
        table->setShowGrid(false);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setMinimumSectionSize(28);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        if (QTableWidgetItem *tier_hdr = table->horizontalHeaderItem(1)) {
            tier_hdr->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        for (int row = 0; row < in->tracker_count; ++row) {
            const i2p_tracker_row &tracker = in->trackers[row];
            const QString announce = qstr(tracker.announce);
            auto *announce_item = new QTableWidgetItem(announce);
            announce_item->setToolTip(announce);
            table->setItem(row, 0, announce_item);

            const QString tier = qstr(tracker.tier);
            auto *tier_item = new QTableWidgetItem(tier);
            tier_item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            table->setItem(row, 1, tier_item);
        }

        new TableItemTooltipFilter(table);
        trackers_table = table;
        pane_layout->addWidget(table);
        layout->addWidget(pane);
    }

    auto *close = new QPushButton(qstr(in->close), &dialog);
    close->setObjectName(QStringLiteral("Primary"));
    close->setDefault(true);
    QObject::connect(close, &QPushButton::clicked, &dialog, [&dialog] {
        if (g_tip) {
            g_tip->forceHide();
        }
        QTimer::singleShot(0, &dialog, &QDialog::accept);
    });
    add_dialog_buttons(layout, {close});

    exec_app_modal_dialog(
        &dialog, host, inner_w, QSize(DialogMetrics::kPeersW, 0),
        [&] {
            const int dialog_w = DialogMetrics::kPeersW;
            if (trackers_table != nullptr) {
                QHeaderView *header = trackers_table->horizontalHeader();
                header->setFixedHeight(std::max(DialogMetrics::kFilesHeaderMinH, header->sizeHint().height()));
                const int header_h = header->height();
                int rows_h = 0;
                for (int row = 0; row < trackers_table->rowCount(); ++row) {
                    trackers_table->setRowHeight(row, std::max(28, trackers_table->rowHeight(row)));
                    rows_h += trackers_table->rowHeight(row);
                }
                note->setFixedWidth(inner_w);
                int note_h = note->heightForWidth(inner_w);
                if (note_h <= 0) {
                    note_h = wrapped_label_height(note, inner_w);
                }
                if (note_h > 0) {
                    note->setFixedHeight(note_h);
                }
                const int chrome_h = margins.top() + margins.bottom() + note_h + dialog_buttons_row_height(close) +
                                     layout->spacing() * 2;
                const int max_table_h = std::max(120, DialogMetrics::kFilesH - chrome_h);
                const int content_table_h = header_h + rows_h;
                const int table_h = std::min(content_table_h, max_table_h);
                trackers_table->setColumnWidth(1, 64);
                trackers_table->setFixedSize(inner_w, table_h);
                if (QWidget *pane = trackers_table->parentWidget()) {
                    pane->setFixedSize(inner_w, table_h);
                    if (pane->objectName() == QLatin1String("TrackersTablePane")) {
                        static_cast<FilesTablePane *>(pane)->applyClip();
                    }
                }
            }
            layout->activate();
            const int height = std::min(DialogMetrics::kFilesH, dialog.sizeHint().height());
            dialog.setFixedSize(dialog_w, std::max(height, 160));
        });
    if (g_tip) {
        g_tip->forceHide();
    }
}

void i2p_defer(i2p_void_cb cb, void *ctx) {
    QTimer::singleShot(0, qApp, [cb, ctx] {
        if (cb) {
            cb(ctx);
        }
    });
}

int i2p_confirm_remove(void *parent, const char *title, const char *text, const char *checkbox,
                       const char *yes_label, const char *cancel_label, int *delete_data) {
    QWidget *host = as_widget(parent);
    QMessageBox box(host);
    apply_real_dialog_window(&box);
    box.setWindowTitle(qstr(title));
    box.setText(qstr(text));
    auto *check = new QCheckBox(qstr(checkbox), &box);
    box.setCheckBox(check);
    box.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
    box.setDefaultButton(QMessageBox::Cancel);
    if (QAbstractButton *yes = box.button(QMessageBox::Yes)) {
        yes->setText(qstr(yes_label));
    }
    if (QAbstractButton *cancel = box.button(QMessageBox::Cancel)) {
        cancel->setText(qstr(cancel_label));
    }
    const int result = box.exec();
    if (delete_data) {
        *delete_data = check->isChecked() ? 1 : 0;
    }
    return result == QMessageBox::Yes ? 1 : 0;
}

void i2p_set_named_text(void *parent, const char *name, const char *text) {
    QWidget *root = as_widget(parent);
    if (root == nullptr) {
        return;
    }
    const QString object_name = qstr(name);
    if (auto *label = root->findChild<QLabel *>(object_name)) {
        label->setText(qstr(text));
        return;
    }
    if (auto *button = root->findChild<QAbstractButton *>(object_name)) {
        button->setText(qstr(text));
    }
}

void i2p_set_dialog_title(void *parent, const char *title) {
    QWidget *root = as_widget(parent);
    if (root == nullptr) {
        return;
    }
    if (auto *dialog = qobject_cast<QDialog *>(root)) {
        dialog->setWindowTitle(qstr(title));
        return;
    }
    if (auto *dialog = root->findChild<QDialog *>()) {
        dialog->setWindowTitle(qstr(title));
    }
}

void i2p_apply_window_material(void *widget, int night) {
    apply_window_material(as_widget(widget), night != 0);
}

QString register_fonts_from_dir(const QString &dir_path) {
    QDir dir(dir_path);
    if (!dir.exists()) {
        return QString();
    }
    QString primary;
    const QStringList filters = {QStringLiteral("*.otf"), QStringLiteral("*.ttf"),
                                 QStringLiteral("*.OTF"), QStringLiteral("*.TTF")};
    for (const QFileInfo &info : dir.entryInfoList(filters, QDir::Files)) {
        if (!info.fileName().startsWith(QStringLiteral("Inter"), Qt::CaseInsensitive)) {
            continue;
        }
        const int id = QFontDatabase::addApplicationFont(info.absoluteFilePath());
        if (id < 0) {
            continue;
        }
        for (const QString &family : QFontDatabase::applicationFontFamilies(id)) {
            if (family.startsWith(QStringLiteral("Inter"), Qt::CaseInsensitive)) {
                primary = family;
            }
        }
    }
    return primary;
}

QString resolve_system_apple_ui_family() {
    const QFontDatabase db;
    static const char *candidates[] = {"SF Pro Text", ".SF NS Text", "SF Pro Display", "SF Pro"};
    for (const char *candidate : candidates) {
        const QString family = QString::fromUtf8(candidate);
        if (db.hasFamily(family)) {
            return family;
        }
    }
    return QString();
}

QString resolve_apple_ui_family() {
    const QFontDatabase db;
    static const char *candidates[] = {"SF Pro Text", ".SF NS Text", "SF Pro Display", "SF Pro",
                                     "Inter"};
    for (const char *candidate : candidates) {
        const QString family = QString::fromUtf8(candidate);
        if (db.hasFamily(family)) {
            return family;
        }
    }
    return QString();
}

QString resolve_system_ui_family() {
    const QFontDatabase db;
    static const char *candidates[] = {"Inter",           "SF Pro Text", ".SF NS Text",
                                       "Segoe UI Variable", "Segoe UI",    "Noto Sans"};
    for (const char *candidate : candidates) {
        const QString family = QString::fromUtf8(candidate);
        if (db.hasFamily(family)) {
            return family;
        }
    }
    return QStringLiteral("sans-serif");
}

void i2p_apply_app_font(int point_size, const char *fonts_dir, const char *language) {
    QApplication *app = qApp;
    if (app == nullptr) {
        return;
    }
    Q_UNUSED(language);
    QString family;
    QStringList fallbacks;
#ifdef Q_OS_MAC
    family = resolve_system_apple_ui_family();
    fallbacks = {QStringLiteral("SF Pro Display"), QStringLiteral(".AppleSystemUIFont"),
                 QStringLiteral(".SF NS Text"), QStringLiteral("Inter"), QStringLiteral("sans-serif")};
#else
    if (fonts_dir != nullptr && fonts_dir[0] != '\0') {
        family = register_fonts_from_dir(qstr(fonts_dir));
    }
    const QString installed_apple = resolve_apple_ui_family();
    if (!installed_apple.isEmpty() &&
        !installed_apple.startsWith(QStringLiteral("Inter"), Qt::CaseInsensitive)) {
        family = installed_apple;
    }
    if (family.isEmpty()) {
        family = installed_apple.isEmpty() ? resolve_system_ui_family() : installed_apple;
    }
    fallbacks = {QStringLiteral("Inter"), QStringLiteral("SF Pro Text"), QStringLiteral("SF Pro Display"),
                 QStringLiteral(".SF NS Text"), QStringLiteral("sans-serif")};
    fallbacks.removeAll(QString());
#endif
    if (family.isEmpty()) {
        family = QStringLiteral("Inter");
    }
    QFont font(family);
    font.setPointSize(point_size);
    QStringList families = {family};
    for (const QString &fallback : fallbacks) {
        if (!fallback.isEmpty() && fallback != family && !families.contains(fallback)) {
            families.push_back(fallback);
        }
    }
    font.setFamilies(families);
    font.setStyleStrategy(QFont::PreferAntialias);
#ifdef Q_OS_WIN
    font.setHintingPreference(QFont::PreferDefaultHinting);
#else
    font.setHintingPreference(QFont::PreferFullHinting);
#endif
    app->setFont(font);
}

void i2p_shortcut_new(void *parent, const char *key, i2p_void_cb cb, void *ctx) {
    auto *shortcut = new QShortcut(QKeySequence(qstr(key)), as_widget(parent));
    QObject::connect(shortcut, &QShortcut::activated, [cb, ctx]() {
        if (cb) {
            cb(ctx);
        }
    });
}

} // extern "C"
