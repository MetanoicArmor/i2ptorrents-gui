#pragma once

#include "config.hpp"
#include "models.hpp"

#include <QWidget>
#include <functional>

struct i2p_files_in;
struct i2p_settings_in;

namespace i2p {

class NativeWidget {
public:
    NativeWidget() = default;
    explicit NativeWidget(void *ptr, bool hasParent = false);
    NativeWidget(NativeWidget &&other) noexcept;
    NativeWidget &operator=(NativeWidget &&other) noexcept;
    ~NativeWidget();

    NativeWidget(const NativeWidget &) = delete;
    NativeWidget &operator=(const NativeWidget &) = delete;

    QWidget *widget() const;
    void *ptr() const { return ptr_; }

    static NativeWidget pieceMap(const QVector<bool> &have);
    static NativeWidget overlayScroll();
    static NativeWidget torrentCard(const QString &theme);

    void setObjectName(const QString &name) const;
    void setTooltip(const QString &tip) const;
    void releaseOwnership() { hasParent_ = true; }

private:
    void *ptr_ = nullptr;
    bool hasParent_ = false;
};

struct SettingsResult {
    QString rpcUrl;
    QString torrentsDir;
    quint32 refreshSeconds = 5;
    QString language;
    QString theme;
    QString torrentView;
};

void installRoundedTooltips();
void applyTooltipPalette(const QString &theme);
void applyWindowMaterial(QWidget *widget, const QString &theme);
void applyAppFont();

void setLabelText(quintptr ptr, const QString &text);
void setObjectNamePtr(quintptr ptr, const QString &name);
void setCursorPtr(quintptr ptr, int shape);
void setPlaceholderPtr(quintptr ptr, const QString &text);
void setButtonText(quintptr ptr, const QString &text);
void setCheckable(QWidget *widget, bool checkable);
void setChecked(quintptr ptr, bool checked);
QString lineEditText(quintptr ptr);

void overlaySetWidget(quintptr scrollPtr, QWidget *child);
void overlayApplyTheme(quintptr scrollPtr, const QString &theme);

void onClick(QWidget *widget, std::function<void()> callback);
void defer(std::function<void()> callback);
void addShortcut(QWidget *parent, const QString &key, std::function<void()> callback);

std::optional<QString> openFile(QWidget *parent, const QString &title, const QString &filter);
void aboutExec(QWidget *parent, const QString &stylesheet);
std::optional<SettingsResult> settingsExec(QWidget *parent,
                                           const QString &stylesheet,
                                           const AppSettings &settings);
void filesExec(QWidget *parent,
               const QString &stylesheet,
               const QString &title,
               const QVector<TorrentFile> &files,
               const std::function<int(int, int, int)> &onChange);
void peersExec(QWidget *parent,
               const QString &stylesheet,
               const QString &title,
               const QVector<Peer> &peers);
void trackersExec(QWidget *parent,
                  const QString &stylesheet,
                  const QString &title,
                  const QVector<Tracker> &trackers);
std::optional<bool> confirmRemove(QWidget *parent,
                                  const QString &title,
                                  const QString &text,
                                  const QString &checkbox,
                                  const QString &yesLabel,
                                  const QString &cancelLabel);

void showPopupBelow(QWidget *parent,
                    quintptr anchorPtr,
                    const QString &theme,
                    const std::function<void(void *popup)> &build);

void popupAddAction(void *popup,
                    const QString &text,
                    bool enabled,
                    std::function<void()> callback);
void popupSeparator(void *popup);

} // namespace i2p
