#include "qt_chrome.h"

#include <QAbstractItemView>
#include <QAbstractAnimation>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QEasingCurve>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
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
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QUrl>
#include <QVariant>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
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

void disable_dwm_rounded_frame(QWidget *widget) {
#ifdef Q_OS_WIN
    if (widget == nullptr) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    const int pref = 1; // DWMWCP_DONOTROUND
    DwmSetWindowAttribute(hwnd, 33, &pref, sizeof(pref));
#else
    Q_UNUSED(widget);
#endif
}

Qt::WindowFlags popup_window_flags() {
    Qt::WindowFlags flags = Qt::Popup | Qt::FramelessWindowHint;
#ifdef Q_OS_WIN
    flags |= Qt::NoDropShadowWindowHint;
#endif
    return flags;
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

void update_popup_rounded_mask(QWidget *widget, qreal radius) {
    const int width = widget->width();
    const int height = widget->height();
    if (width < 2 || height < 2) {
        return;
    }
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width, height), radius, radius);
    widget->setMask(QRegion(path.toFillPolygon().toPolygon()));
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
        QObject::connect(sb_, &QScrollBar::valueChanged, this, [this](int) { update(); });
        QObject::connect(sb_, &QScrollBar::rangeChanged, this, [this](int, int) { update(); });
    }

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
        if (track_h <= 0) {
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

    QScrollBar *sb_;
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
        bar_ = new RoundedVerticalScrollbar(verticalScrollBar(), this);
        bar_->hide();
        QObject::connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) { syncBar(); });
        QObject::connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { syncBar(); });
    }

    void applyTheme(const QString &theme) { bar_->applyTheme(theme); }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QScrollArea::resizeEvent(event);
        syncBar();
    }

private:
    void syncBar() {
        QScrollBar *bar = verticalScrollBar();
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
        setObjectName(object_name);
        hide();
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        surface_ = new QFrame(this);
        surface_->setObjectName(object_name + QStringLiteral("Surface"));
        root->addWidget(surface_);
    }

    QFrame *surface() const { return surface_; }
    qreal radius() const { return radius_; }

    void presentAsPopup() {
        if (!(windowFlags() & Qt::Popup)) {
            setWindowFlags(popup_window_flags());
            setAttribute(Qt::WA_TranslucentBackground, true);
        }
        show();
        raise();
    }

    void setPopupColors(bool night) {
        if (night) {
            popup_bg_ = QColor(28, 31, 40, 250);
            popup_border_ = QColor(58, 62, 74);
        } else {
            popup_bg_ = QColor(246, 247, 250);
            popup_border_ = QColor(208, 211, 218);
        }
    }

    QString shellStylesheet(bool night, const QString &extra) const {
        const QString name = objectName();
        const QString surface = name + QStringLiteral("Surface");
        if (paint_bg_) {
            return QStringLiteral("#%1 { background: transparent; }\n#%2 { background: transparent; border: none; "
                                  "border-radius: %3px; }\n%4")
                .arg(name, surface, QString::number(static_cast<int>(radius_)), extra);
        }
        const char *bg = night ? "rgba(28, 31, 40, 0.98)" : "#f6f7fa";
        const char *border = night ? "rgba(255, 255, 255, 0.14)" : "rgba(0, 0, 0, 0.12)";
        return QStringLiteral("#%1 { background: transparent; }\n#%2 { background: %3; border: 1px solid %4; "
                              "border-radius: %5px; }\n%6")
            .arg(name, surface, QLatin1String(bg), QLatin1String(border),
                 QString::number(static_cast<int>(radius_)), extra);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QFrame::paintEvent(event);
        if (paint_bg_) {
            paint_popup_rounded_bg(this, popup_bg_, popup_border_, radius_);
        }
    }

    void resizeEvent(QResizeEvent *event) override {
        QFrame::resizeEvent(event);
        applyLinuxMask();
    }

    void showEvent(QShowEvent *event) override {
        QFrame::showEvent(event);
        if (!dwm_patched_) {
            disable_dwm_rounded_frame(this);
            dwm_patched_ = true;
        }
        if (paint_bg_) {
            QTimer::singleShot(0, this, [this] { applyLinuxMask(); });
        }
    }

private:
    void applyLinuxMask() {
#ifdef Q_OS_LINUX
        if (paint_bg_) {
            update_popup_rounded_mask(this, radius_);
        }
#else
        Q_UNUSED(radius_);
#endif
    }

    QFrame *surface_ = nullptr;
    qreal radius_ = 12.0;
    bool paint_bg_ = true;
    bool dwm_patched_ = false;
    QColor popup_bg_ = QColor(246, 247, 250);
    QColor popup_border_ = QColor(208, 211, 218);
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
        const QColor sel_bg(night_ ? "#3a5588" : "#dbe9ff");
        const QColor hov_bg(night_ ? "#2c3039" : "#e8eef8");
        const QColor sel_fg(night_ ? "#f4f7ff" : "#1b4f9f");
        const QColor txt_fg(night_ ? "#d8deea" : "#2f3644");
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
        presentAsPopup();
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
        const QString color = night ? QStringLiteral("#d8deea") : QStringLiteral("#2f3644");
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
    std::function<void()> on_popup;
    void showPopup() override {
        if (on_popup) {
            on_popup();
        }
    }
    void hidePopup() override {}
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
        arrow_ = new QLabel(QStringLiteral("∨"), this);
        arrow_->setAlignment(Qt::AlignCenter);
        arrow_->setAttribute(Qt::WA_TransparentForMouseEvents);
        popup_ = new ComboPopup(this);
        popup_->on_chosen = [this](const QString &text) {
            const int index = combo_->findText(text);
            if (index >= 0) {
                combo_->setCurrentIndex(index);
            }
        };
        combo_->on_popup = [this] {
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
        const QString color = theme_ == QLatin1String("night") ? QStringLiteral("#9fa1b5") : QStringLiteral("#8c8d94");
        arrow_->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background: transparent;").arg(color));
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
    QLabel *arrow_ = nullptr;
    ComboPopup *popup_ = nullptr;
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
        const QString color = night ? QStringLiteral("#eceff4") : QStringLiteral("#1d1d1f");
        const QString disabled = night ? QStringLiteral("#8b93a5") : QStringLiteral("#8e8e93");
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
        presentAsPopup();
        QTimer::singleShot(0, this, [this] { setFocus(Qt::PopupFocusReason); });
    }

    void applyTheme(const QString &theme) {
        night_ = theme == QLatin1String("night");
        setPopupColors(night_);
        QString items;
        if (night_) {
            items = QStringLiteral(
                "QFrame#ActionsPopupItem { background: transparent; border: none; border-radius: 10px; }"
                "QFrame#ActionsPopupItem:hover { background: rgba(255, 255, 255, 0.10); }"
                "QFrame#ActionsPopupItem:disabled { background: transparent; }"
                "QFrame#ActionsPopupSeparator { background: #343a46; max-height: 1px; min-height: 1px; "
                "border: none; margin: 4px 8px; }");
        } else {
            items = QStringLiteral(
                "QFrame#ActionsPopupItem { background: transparent; border: none; border-radius: 10px; }"
                "QFrame#ActionsPopupItem:hover { background: #e5eaf2; }"
                "QFrame#ActionsPopupItem:disabled { background: transparent; }"
                "QFrame#ActionsPopupSeparator { background: #d6dce7; max-height: 1px; min-height: 1px; "
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
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
    }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int w) const override {
        const int text_w = std::max(1, w - pad_x_ * 2 - extraBearing());
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
        painter.fillRect(rect(), rowBackground());
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
        option.setWrapMode(QTextOption::WrapAnywhere);
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

    QColor rowBackground() const {
        const QTableWidget *table = nullptr;
        for (QWidget *p = parentWidget(); p != nullptr; p = p->parentWidget()) {
            table = qobject_cast<QTableWidget *>(p);
            if (table != nullptr) {
                break;
            }
        }
        if (table == nullptr) {
            return palette().color(QPalette::Base);
        }
        int row = -1;
        for (int r = 0; r < table->rowCount(); ++r) {
            if (table->cellWidget(r, 0) == this) {
                row = r;
                break;
            }
        }
        const QPalette pal = table->palette();
        if (row >= 0 && table->alternatingRowColors() && (row % 2) == 1) {
            return pal.color(QPalette::AlternateBase);
        }
        return pal.color(QPalette::Base);
    }

    int textHeight(int width) const {
        QTextLayout layout(text(), font());
        fillLayout(&layout, width);
        if (layout.lineCount() == 0) {
            return static_cast<int>(std::ceil(QFontMetricsF(font()).lineSpacing()));
        }
        const QTextLine last = layout.lineAt(layout.lineCount() - 1);
        const qreal bottom = last.y() + std::max(last.height(), QFontMetricsF(font()).lineSpacing());
        return static_cast<int>(std::ceil(bottom + 2));
    }

    static constexpr int pad_x_ = 8;
    static constexpr int pad_y_ = 8;
};

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
            if (cur->objectName() == QLatin1String("MoreButton")) {
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

class SpinRowWidget final : public QFrame {
public:
    explicit SpinRowWidget(QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("SpinRow"));
        setFrameShape(QFrame::NoFrame);
        setProperty("focused", false);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        spin_ = new QSpinBox(this);
        spin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        layout->addWidget(spin_, 1);
        auto *step_col = new QWidget(this);
        step_col->setObjectName(QStringLiteral("SpinStepColumn"));
        auto *steps = new QVBoxLayout(step_col);
        steps->setContentsMargins(0, 0, 0, 0);
        steps->setSpacing(0);
        auto *up = new QToolButton(step_col);
        up->setObjectName(QStringLiteral("SpinStepUp"));
        auto *down = new QToolButton(step_col);
        down->setObjectName(QStringLiteral("SpinStepDown"));
        for (auto *button : {up, down}) {
            button->setAutoRaise(true);
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setCursor(Qt::PointingHandCursor);
            button->setAutoRepeat(true);
            button->setAutoRepeatDelay(400);
            button->setAutoRepeatInterval(120);
            QFont font = button->font();
            font.setPixelSize(10);
            button->setFont(font);
        }
        up->setText(QStringLiteral("▲"));
        down->setText(QStringLiteral("▼"));
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

class RoundedTooltipWindow final : public QWidget {
public:
    RoundedTooltipWindow() : QWidget(nullptr, tooltipFlags()) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
        label_ = new QLabel(this);
        label_->setWordWrap(true);
        label_->setMaximumWidth(440);
        label_->setTextInteractionFlags(Qt::NoTextInteraction);
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
        adjustSize();
        move(clampGlobal(global_top_left, width(), height()));
        updateMask();
        setWindowOpacity(0.0);
        show();
        raise();
        fade_in_.setDuration(150);
        fade_in_.setStartValue(0.0);
        fade_in_.setEndValue(1.0);
        fade_in_.start();
        dismiss_.stop();
        hide_timer_.stop();
        hide_timer_.start(msec > 0 ? msec : 30000);
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
        dismiss_.stop();
        hide_timer_.stop();
        if (!isVisible()) {
            setWindowOpacity(1.0);
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
        if (!dismiss_.isActive()) {
            dismiss_.start(300);
        }
    }

    void cancelDismiss() { dismiss_.stop(); }

protected:
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
        updateMask();
    }

    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (!dwm_patched_) {
            disable_dwm_rounded_frame(this);
            dwm_patched_ = true;
        }
        QTimer::singleShot(0, this, [this] { updateMask(); });
    }

private:
    static Qt::WindowFlags tooltipFlags() {
        Qt::WindowFlags flags = Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus |
                                Qt::WindowStaysOnTopHint;
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
        flags |= Qt::NoDropShadowWindowHint;
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

    void updateMask() {
#ifdef Q_OS_WIN
        return;
#else
        const QRect box = rect();
        if (box.width() < 2 || box.height() < 2) {
            return;
        }
        QPainterPath path;
        path.addRoundedRect(QRectF(box), 12.0, 12.0);
        setMask(QRegion(path.toFillPolygon().toPolygon()));
#endif
    }

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

RoundedTooltipWindow *ensure_tip() {
    if (g_tip == nullptr) {
        g_tip = new RoundedTooltipWindow();
    }
    return g_tip;
}

bool cursor_over_owner_or_tip(QWidget *owner) {
    QWidget *widget = QApplication::widgetAt(QCursor::pos());
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

class TooltipInterceptFilter final : public QObject {
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(event);
            auto *widget = qobject_cast<QWidget *>(obj);
            if (widget) {
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
        if (et == QEvent::MouseButtonPress || et == QEvent::Wheel || et == QEvent::KeyPress ||
            et == QEvent::FocusOut || et == QEvent::WindowDeactivate) {
            g_tip->cancelDismiss();
            g_tip->startFadeOut();
        } else if (et == QEvent::MouseMove || et == QEvent::Leave) {
            if (cursor_over_owner_or_tip(owner)) {
                g_tip->cancelDismiss();
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

thread_local std::string g_combo_data;
thread_local std::string g_line_edit_text;
thread_local std::string g_settings_rpc;
thread_local std::string g_settings_dir;
thread_local std::string g_settings_language;
thread_local std::string g_settings_theme;
thread_local std::string g_settings_view;
thread_local int g_settings_refresh = 2;
thread_local std::string g_open_file;

QComboBox *settings_combo(QWidget *parent, const char *text_a, const char *data_a,
                          const char *text_b, const char *data_b, const char *current) {
    auto *combo = new QComboBox(parent);
    combo->setEditable(false);
    combo->addItem(qstr(text_a), qstr(data_a));
    combo->addItem(qstr(text_b), qstr(data_b));
    const int index = combo->findData(qstr(current));
    combo->setCurrentIndex(index >= 0 ? index : 0);
    return combo;
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
    // A QWidget parent (not QMainWindow) becomes a macOS sheet that often
    // never appears. Force a real dialog window and application modality.
    Qt::WindowFlags flags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                            Qt::WindowCloseButtonHint;
#ifdef Q_OS_MAC
    flags |= Qt::Window;
#endif
    dialog->setWindowFlags(flags);
    dialog->setWindowModality(Qt::ApplicationModal);
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
    if (auto *area = static_cast<OverlayScrollArea *>(as_widget(scroll))) {
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
        static TooltipInterceptFilter *filter = nullptr;
        if (filter == nullptr) {
            filter = new TooltipInterceptFilter();
            app->installEventFilter(filter);
        }
    }
}

void i2p_apply_tooltip_palette(const char *theme) {
    QApplication *app = qApp;
    if (app == nullptr) {
        return;
    }
    const bool night = qstr(theme) == QLatin1String("night");
    const QColor bg(night ? "#22252d" : "#f2f4f8");
    const QColor fg(night ? "#e3e8f1" : "#1d1d1f");
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
    QDialog dialog(as_widget(parent));
    dialog.setWindowTitle(qstr(in->title));
    dialog.setModal(true);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumSize(520, 480);
    dialog.resize(520, 520);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(8);

    layout->addWidget(new QLabel(qstr(in->rpc_label), &dialog));
    auto *rpc = new QLineEdit(qstr(in->rpc_value), &dialog);
    rpc->setPlaceholderText(qstr(in->rpc_placeholder));
    rpc->setToolTip(qstr(in->rpc_tip));
    layout->addWidget(rpc);

    layout->addWidget(new QLabel(qstr(in->dir_label), &dialog));
    auto *dir_row = new QWidget(&dialog);
    auto *dir_layout = new QHBoxLayout(dir_row);
    dir_layout->setContentsMargins(0, 0, 0, 0);
    auto *dir = new QLineEdit(qstr(in->dir_value), dir_row);
    dir_layout->addWidget(dir, 1);
    auto *browse = new QPushButton(qstr(in->browse), dir_row);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [dir, &dialog, in] {
        const QString path =
            QFileDialog::getExistingDirectory(&dialog, qstr(in->dir_label), dir->text());
        if (!path.isEmpty()) {
            dir->setText(path);
        }
    });
    dir_layout->addWidget(browse);
    layout->addWidget(dir_row);

    layout->addWidget(new QLabel(qstr(in->refresh_label), &dialog));
    auto *refresh = new QSpinBox(&dialog);
    refresh->setRange(2, 60);
    refresh->setValue(std::clamp(in->refresh_value, 2, 60));
    refresh->setSuffix(QStringLiteral(" ") + qstr(in->seconds_suffix));
    layout->addWidget(refresh);

    layout->addWidget(new QLabel(qstr(in->lang_label), &dialog));
    auto *language = settings_combo(&dialog, in->lang_en, "en", in->lang_ru, "ru", in->lang_current);
    layout->addWidget(language);

    layout->addWidget(new QLabel(qstr(in->theme_label), &dialog));
    auto *theme =
        settings_combo(&dialog, in->theme_light, "light", in->theme_night, "night", in->theme_current);
    layout->addWidget(theme);

    layout->addWidget(new QLabel(qstr(in->view_label), &dialog));
    auto *view = settings_combo(&dialog, in->view_simple, "simple", in->view_detailed, "detailed",
                                in->view_current);
    layout->addWidget(view);

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    note->setWordWrap(true);
    note->setToolTip(qstr(in->rpc_tip));
    layout->addWidget(note);
    layout->addStretch(1);

    auto *buttons = new QWidget(&dialog);
    auto *button_row = new QHBoxLayout(buttons);
    button_row->setContentsMargins(0, 8, 0, 0);
    button_row->addStretch(1);
    auto *cancel = new QPushButton(qstr(in->cancel), buttons);
    auto *save = new QPushButton(qstr(in->save), buttons);
    save->setObjectName(QStringLiteral("Primary"));
    save->setDefault(true);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
    button_row->addWidget(cancel);
    button_row->addWidget(save);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return 0;
    }
    g_settings_rpc = rpc->text().toStdString();
    g_settings_dir = dir->text().toStdString();
    g_settings_refresh = refresh->value();
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

void i2p_about_exec(void *parent, const i2p_about_in *in) {
    if (in == nullptr) {
        return;
    }
    QDialog dialog(as_widget(parent));
    dialog.setWindowTitle(qstr(in->title));
    dialog.setModal(true);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumWidth(440);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(8);
    layout->setSizeConstraint(QLayout::SetFixedSize);

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
    github->setText(QStringLiteral("%1: <a href=\"%2\">%2</a>")
                        .arg(qstr(in->github_label).toHtmlEscaped(), qstr(in->github_url).toHtmlEscaped()));
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
            qr_label->setPixmap(qr.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    layout->addWidget(ok, 0, Qt::AlignRight);

    dialog.exec();
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
    QDialog dialog(as_widget(parent));
    dialog.setWindowTitle(qstr(in->title));
    dialog.setModal(true);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumSize(560, 380);
    dialog.resize(640, 460);
    if (in->stylesheet && in->stylesheet[0] != '\0') {
        dialog.setStyleSheet(qstr(in->stylesheet));
    }

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *note = new QLabel(qstr(in->note), &dialog);
    note->setObjectName(QStringLiteral("Secondary"));
    note->setWordWrap(true);
    layout->addWidget(note);

    if (in->file_count <= 0) {
        auto *empty = new QLabel(qstr(in->empty), &dialog);
        empty->setObjectName(QStringLiteral("Secondary"));
        empty->setWordWrap(true);
        layout->addWidget(empty, 1);
    } else {
        auto *table = new QTableWidget(in->file_count, 4, &dialog);
        table->setObjectName(QStringLiteral("FilesTable"));
        table->setHorizontalHeaderLabels({qstr(in->col_name), qstr(in->col_size), qstr(in->col_progress),
                                          qstr(in->col_priority)});
        table->verticalHeader()->setVisible(false);
        table->setShowGrid(false);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setMinimumSectionSize(28);
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setMinimumSectionSize(40);

        const QStringList priority_labels = {qstr(in->priority_skip), qstr(in->priority_low),
                                             qstr(in->priority_normal), qstr(in->priority_high)};
        const QFontMetrics metrics(table->font());
        int priority_text_w = metrics.horizontalAdvance(qstr(in->col_priority));
        for (const QString &label : priority_labels) {
            priority_text_w = std::max(priority_text_w, metrics.horizontalAdvance(label));
        }

        std::vector<QComboBox *> combos;
        combos.reserve(static_cast<size_t>(in->file_count));
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

            auto *combo = new QComboBox(table);
            combo->addItem(priority_labels[0], 0);
            combo->addItem(priority_labels[1], -1);
            combo->addItem(priority_labels[2], 0);
            combo->addItem(priority_labels[3], 1);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            combo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            combo->setMinimumWidth(priority_text_w + 48);
            combo->setToolTip(tip);
            const int start = file_combo_index(file.wanted, file.priority);
            combo->setCurrentIndex(start);
            combo->setProperty("prev", start);
            combo->setProperty("fileIndex", file.index);

            auto *priority_cell = new QWidget(table);
            auto *priority_layout = new QVBoxLayout(priority_cell);
            priority_layout->setContentsMargins(2, 4, 2, 4);
            priority_layout->setSpacing(0);
            priority_layout->addStretch();
            priority_layout->addWidget(combo);
            priority_layout->addStretch();
            table->setCellWidget(row, 3, priority_cell);
            combos.push_back(combo);
        }

        table->setColumnWidth(2, std::max(metrics.horizontalAdvance(QStringLiteral("100%")),
                                          metrics.horizontalAdvance(qstr(in->col_progress))) +
                                     20);
        table->setColumnWidth(3, priority_text_w + 56);

        auto relayout_rows = [table] {
            for (int row = 0; row < table->rowCount(); ++row) {
                QWidget *cell = table->cellWidget(row, 0);
                if (cell == nullptr || cell->objectName() != QLatin1String("FilesName")) {
                    continue;
                }
                const int name_w = cell->width() > 1 ? cell->width() : std::max(1, table->columnWidth(0));
                table->setRowHeight(row, static_cast<FilesNameLabel *>(cell)->heightForWidth(name_w));
            }
        };
        QObject::connect(table->horizontalHeader(), &QHeaderView::sectionResized, table,
                         [relayout_rows](int index, int, int) {
                             if (index == 0) {
                                 relayout_rows();
                             }
                         });
        QTimer::singleShot(0, table, relayout_rows);

        const QString unsupported = qstr(in->unsupported_note);
        for (QComboBox *combo : combos) {
            QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                             [combo, &combos, note, unsupported, cb, ctx](int index) {
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
                                 const int rc = cb(ctx, combo->property("fileIndex").toInt(), wanted, priority);
                                 if (rc == 0) {
                                     combo->setProperty("prev", index);
                                     return;
                                 }
                                 combo->blockSignals(true);
                                 combo->setCurrentIndex(combo->property("prev").toInt());
                                 combo->blockSignals(false);
                                 if (rc == 1) {
                                     note->setText(unsupported);
                                     for (QComboBox *item : combos) {
                                         item->setEnabled(false);
                                     }
                                 }
                             });
        }
        layout->addWidget(table, 1);
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
    layout->addWidget(close, 0, Qt::AlignRight);

    dialog.exec();
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
    QMessageBox box(as_widget(parent));
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

void i2p_apply_app_font(int point_size) {
    QApplication *app = qApp;
    if (app == nullptr) {
        return;
    }
#ifdef Q_OS_MAC
    QFont font = app->font();
    font.setPointSize(point_size);
#else
    QFont font(QStringLiteral("Inter"), point_size);
    font.setFamilies({QStringLiteral("Inter"), QStringLiteral("Segoe UI"), QStringLiteral("Noto Sans"),
                      QStringLiteral("sans-serif")});
    font.setStyleHint(QFont::SansSerif);
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
