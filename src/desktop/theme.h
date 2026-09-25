#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor orange READ orange NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY changed)
    Q_PROPERTY(QColor blue READ blue NOTIFY changed)
    Q_PROPERTY(QColor magenta READ magenta NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)

public:
    explicit Theme(QObject* parent = nullptr);
    explicit Theme(QString homePath, QObject* parent = nullptr);
    ~Theme() override;
    QString name() const { return name_; }
    QString fontFamily() const { return fontFamily_; }
    QString mode() const { return mode_; }
    QColor background() const { return colors_.value("background"); }
    QColor foreground() const { return colors_.value("foreground"); }
    QColor accent() const { return colors_.value("accent"); }
    QColor red() const { return colors_.value("red"); }
    QColor orange() const { return colors_.value("orange"); }
    QColor yellow() const { return colors_.value("yellow"); }
    QColor green() const { return colors_.value("green"); }
    QColor cyan() const { return colors_.value("cyan"); }
    QColor blue() const { return colors_.value("blue"); }
    QColor magenta() const { return colors_.value("magenta"); }
    QColor surface() const { return colors_.value("surface"); }
    QColor muted() const { return colors_.value("muted"); }
    QColor border() const { return colors_.value("border"); }
    QColor selection() const { return colors_.value("selection"); }
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    void reloadPalette();
    void updateWatches();
    QString homePath_;
    QString name_ = "Blockyard";
    QString fontFamily_ = "monospace";
    QString mode_ = "dark";
    QHash<QString, QColor> colors_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    QProcess fontProcess_;
    QTimer fontTimeout_;
};
