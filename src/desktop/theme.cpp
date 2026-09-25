#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
using Colors = QHash<QString, QColor>;

double luminance(const QColor& color) {
    const auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

double contrast(const QColor& a, const QColor& b) {
    const double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

QColor mix(const QColor& a, const QColor& b, double amount) {
    return QColor::fromRgbF(a.redF() * (1 - amount) + b.redF() * amount,
                           a.greenF() * (1 - amount) + b.greenF() * amount,
                           a.blueF() * (1 - amount) + b.blueF() * amount);
}

QColor readable(QColor color, const QColor& background, const QColor& surface) {
    const auto score = [&](QColor candidate) { return std::min(contrast(candidate, background), contrast(candidate, surface)); };
    const QColor target = score(Qt::black) > score(Qt::white) ? QColor(Qt::black) : QColor(Qt::white);
    for (int i = 0; i <= 100; ++i) {
        const QColor candidate = mix(color, target, i / 100.0);
        if (contrast(candidate, background) >= 4.5 && contrast(candidate, surface) >= 4.5)
            return candidate;
    }
    return target;
}

// Accept only scalar color data. Theme files never enter a shell or a QML engine.
Colors readColors(const QString& path, QString* mode) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 128 * 1024)
        return {};
    static const QRegularExpression section(R"(^\s*\[([A-Za-z0-9_.]+)\]\s*(?:#.*)?$)");
    static const QRegularExpression color(R"color(^\s*([A-Za-z0-9_.]+)\s*=\s*(?:"(?:#|0[xX])?([0-9a-fA-F]{6})"|'(?:#|0[xX])?([0-9a-fA-F]{6})'|(?:0[xX])?([0-9a-fA-F]{6}))\s*(?:#.*)?$)color");
    static const QRegularExpression modePattern(R"(^\s*mode\s*=\s*["'](light|dark)["']\s*(?:#.*)?$)");
    Colors result;
    QString prefix;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine());
        const auto sectionMatch = section.match(line);
        if (sectionMatch.hasMatch()) {
            prefix = sectionMatch.captured(1) + '.';
            continue;
        }
        const auto modeMatch = modePattern.match(line);
        if (modeMatch.hasMatch() && prefix.isEmpty())
            *mode = modeMatch.captured(1);
        const auto match = color.match(line);
        if (!match.hasMatch())
            continue;
        QString hex;
        for (int group = 2; group <= 4; ++group)
            if (!match.captured(group).isEmpty()) hex = match.captured(group);
        const QString key = prefix + match.captured(1);
        if (!result.contains(key)) result.insert(key, QColor('#' + hex));
    }
    return result;
}

Colors paletteAt(const QString& directory, QString* mode) {
    const QString path = directory + "/colors.toml";
    Colors palette = readColors(path, mode);
    if (!QFileInfo::exists(path)) {
        const Colors old = readColors(directory + "/alacritty.toml", mode);
        const QStringList names{"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"};
        for (int i = 0; i < names.size(); ++i)
            if (old.contains("colors.normal." + names[i]))
                palette.insert("color" + QString::number(i), old.value("colors.normal." + names[i]));
        for (const auto& name : {QString("background"), QString("foreground")})
            if (old.contains("colors.primary." + name)) palette.insert(name, old.value("colors.primary." + name));
        if (old.contains("colors.selection.background")) palette.insert("selection", old.value("colors.selection.background"));
    }
    const QStringList aliases{"background", "red", "green", "yellow", "blue", "magenta", "cyan", "foreground"};
    for (int i = 0; i < aliases.size(); ++i) {
        const QString key = "color" + QString::number(i);
        if (!palette.contains(aliases[i]) && palette.contains(key)) palette.insert(aliases[i], palette.value(key));
    }
    if (!palette.contains("background") || !palette.contains("foreground")) return {};
    return palette;
}

Colors derive(Colors palette) {
    const QColor bg = palette.value("background");
    // Raised areas stay close enough to the background for one readable text palette.
    const QColor fg = readable(palette.value("foreground"), bg, bg);
    QColor surface = palette.value("lighter_background", mix(bg, fg, 0.06));
    if (contrast(surface, bg) > 1.45) surface = mix(bg, fg, 0.06);
    if (contrast(fg, surface) < 4.5) surface = bg;
    palette.insert("surface", surface);
    palette.insert("foreground", readable(fg, bg, surface));
    palette.insert("muted", readable(palette.value("muted", mix(bg, fg, 0.55)), bg, surface));
    palette.insert("border", mix(bg, fg, 0.22));
    const QColor accent = palette.value("accent", palette.value("blue", fg));
    palette.insert("accent", readable(accent, bg, surface));
    const QStringList categories{"red", "orange", "yellow", "green", "cyan", "blue", "magenta"};
    for (const QString& key : categories) {
        const QColor fallback = key == "orange" ? palette.value("yellow", accent) : accent;
        palette.insert(key, palette.value(key, fallback));
    }
    QColor selection = palette.value("selection", mix(bg, accent, 0.2));
    if (contrast(selection, palette.value("foreground")) < 4.5) selection = mix(bg, accent, 0.15);
    if (contrast(selection, palette.value("foreground")) < 4.5) selection = bg;
    palette.insert("selection", selection);
    return palette;
}
}

Theme::Theme(QObject* parent) : Theme(QDir::homePath(), parent) {}

Theme::Theme(QString homePath, QObject* parent)
    : QObject(parent), homePath_(std::move(homePath)) {
    colors_ = derive({{"background", QColor("#151719")}, {"foreground", QColor("#e6e9e7")},
                      {"accent", QColor("#8db8a0")}, {"red", QColor("#e58383")},
                      {"yellow", QColor("#d0bb79")}, {"green", QColor("#8db8a0")},
                      {"cyan", QColor("#82b8bf")}, {"blue", QColor("#8ba8d7")},
                      {"magenta", QColor("#bba0d4")}});
    debounce_.setSingleShot(true);
    debounce_.setInterval(90);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, &debounce_, qOverload<>(&QTimer::start));
    connect(&watcher_, &QFileSystemWatcher::fileChanged, &debounce_, qOverload<>(&QTimer::start));
    connect(&debounce_, &QTimer::timeout, this, &Theme::refresh);
    fontTimeout_.setSingleShot(true);
    fontTimeout_.setInterval(1500);
    connect(&fontTimeout_, &QTimer::timeout, &fontProcess_, &QProcess::kill);
    connect(&fontProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        fontTimeout_.stop();
        if (code != 0 || status != QProcess::NormalExit) return;
        const QString family = QString::fromUtf8(fontProcess_.readAllStandardOutput()).split('\n').first().split(',').first().trimmed();
        if (!family.isEmpty() && family.size() < 256 && family != fontFamily_) {
            fontFamily_ = family;
            emit changed();
        }
    });
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        connect(app, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive) refresh();
        });
    }
    refresh();
}

Theme::~Theme() {
    if (fontProcess_.state() != QProcess::NotRunning) {
        fontProcess_.kill();
        fontProcess_.waitForFinished(1500);
    }
}

void Theme::refresh() {
    reloadPalette();
    if (fontProcess_.state() == QProcess::NotRunning) {
        fontProcess_.start("fc-match", {"monospace", "-f", "%{family}\\n"});
        fontTimeout_.start();
    }
}

void Theme::reloadPalette() {
    const QStringList roots{homePath_ + "/.local/state/omarchy/current", homePath_ + "/.config/omarchy/current"};
    for (const QString& root : roots) {
        QString mode;
        Colors colors = paletteAt(root + "/theme", &mode);
        if (colors.isEmpty()) continue;
        colors = derive(std::move(colors));
        if (mode.isEmpty()) mode = luminance(colors.value("background")) > 0.45 ? "light" : "dark";
        QFile nameFile(root + "/theme.name");
        QString name = "Omarchy";
        if (nameFile.open(QIODevice::ReadOnly)) {
            QString stored = QString::fromUtf8(nameFile.read(256)).trimmed();
            if (!stored.isEmpty()) name = stored.replace('-', ' ');
        } else {
            const QFileInfo themeDir(root + "/theme");
            if (themeDir.isSymLink()) name = QFileInfo(themeDir.symLinkTarget()).fileName().replace('-', ' ');
        }
        if (colors_ != colors || mode_ != mode || name_ != name) {
            colors_ = std::move(colors);
            name_ = std::move(name);
            mode_ = std::move(mode);
            emit changed();
        }
        break;
    }
    updateWatches();
}

void Theme::updateWatches() {
    QSet<QString> desired;
    const QStringList roots{homePath_ + "/.local/state/omarchy/current", homePath_ + "/.config/omarchy/current"};
    for (const QString& root : roots) {
        for (const QString& path : {root + "/theme", root + "/theme/colors.toml", root + "/theme/alacritty.toml", root + "/theme.name"}) {
            QFileInfo info(path);
            if (info.exists()) {
                desired.insert(path);
                if (info.isSymLink()) desired.insert(info.canonicalFilePath());
            }
        }
        QString ancestor = QFileInfo(root).absolutePath();
        if (QFileInfo::exists(root)) desired.insert(root);
        while (ancestor.startsWith(homePath_) && ancestor.size() >= homePath_.size()) {
            if (QFileInfo::exists(ancestor)) { desired.insert(ancestor); break; }
            const QString parent = QFileInfo(ancestor).absolutePath();
            if (parent == ancestor) break;
            ancestor = parent;
        }
    }
    const QString fontDirectory = homePath_ + "/.config/fontconfig";
    if (QFileInfo::exists(fontDirectory)) {
        desired.insert(fontDirectory);
        if (QFileInfo::exists(fontDirectory + "/fonts.conf")) desired.insert(fontDirectory + "/fonts.conf");
    }
    // Rebind even unchanged logical paths: a symlink may now point at another inode.
    const QStringList current = watcher_.directories() + watcher_.files();
    if (!current.isEmpty()) watcher_.removePaths(current);
    for (const QString& path : std::as_const(desired)) watcher_.addPath(path);
}
