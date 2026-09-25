#include "desktop/theme.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

namespace {
bool write(const QString& path, const QByteArray& contents) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
double luminance(QColor color) {
    const auto channel = [](double n) { return n <= 0.04045 ? n / 12.92 : std::pow((n + 0.055) / 1.055, 2.4); };
    return channel(color.redF()) * 0.2126 + channel(color.greenF()) * 0.7152 + channel(color.blueF()) * 0.0722;
}
double contrast(QColor a, QColor b) {
    return (std::max(luminance(a), luminance(b)) + 0.05) / (std::min(luminance(a), luminance(b)) + 0.05);
}
const QByteArray dark = "background = '#121212'\nforeground = '#efefef'\nblue = '#77aacc'\n";
const QByteArray light = "mode = 'light'\nbackground = '#fffdf1'\nforeground = '#242424'\nmuted = '#eeeeee'\nselection = '#222222'\n";
}

class ThemeTest : public QObject {
    Q_OBJECT
private slots:
    void stockPalettes() {
        const QDir stock("/usr/share/omarchy/themes");
        if (!stock.exists()) QSKIP("Omarchy stock palettes are not installed; synthetic tests still cover adapters.");
        int tested = 0;
        for (const QString& slug : stock.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile palette(stock.filePath(slug + "/colors.toml"));
            if (!palette.open(QIODevice::ReadOnly)) continue;
            QTemporaryDir home;
            QVERIFY(home.isValid());
            const QString current = home.path() + "/.local/state/omarchy/current";
            QVERIFY(write(current + "/theme/colors.toml", palette.readAll()));
            QVERIFY(write(current + "/theme.name", slug.toUtf8()));
            Theme theme(home.path());
            QVERIFY2(theme.name() != "Blockyard", qPrintable(slug));
            QVERIFY2(contrast(theme.foreground(), theme.background()) >= 4.49, qPrintable(slug));
            QVERIFY2(contrast(theme.foreground(), theme.surface()) >= 4.49, qPrintable(slug));
            QVERIFY2(contrast(theme.muted(), theme.background()) >= 4.49, qPrintable(slug));
            QVERIFY2(contrast(theme.muted(), theme.surface()) >= 4.49, qPrintable(slug));
            QVERIFY2(contrast(theme.foreground(), theme.selection()) >= 4.49, qPrintable(slug));
            QVERIFY(theme.orange().isValid());
            QTRY_VERIFY(theme.fontFamily() != "monospace");
            ++tested;
        }
        QVERIFY(tested > 0);
        qInfo() << "Validated stock palettes:" << tested;
    }

    void legacyAndMissingFields() {
        QTemporaryDir home;
        const QString root = home.path() + "/.config/omarchy/current/theme";
        QVERIFY(write(root + "/colors.toml", "color0 = '#fffdf1'\ncolor7 = '#111111'\ncolor4 = '#123456'\n"));
        Theme theme(home.path());
        QCOMPARE(theme.mode(), "light");
        QCOMPARE(theme.blue(), QColor("#123456"));
        QVERIFY(contrast(theme.muted(), theme.surface()) >= 4.49);
        QVERIFY(QFile::remove(root + "/colors.toml"));
        QVERIFY(write(root + "/alacritty.toml", "[colors.primary]\nbackground = '0x121212'\nforeground = '#efefef'\n[colors]\nnormal.blue = '#77aacc'\n"));
        theme.refresh();
        QCOMPARE(theme.background(), QColor("#121212"));
        QCOMPARE(theme.blue(), QColor("#77aacc"));
        QTRY_VERIFY(theme.fontFamily() != "monospace");
    }

    void preservesValidPalette() {
        QTemporaryDir home;
        const QString root = home.path() + "/.local/state/omarchy/current/theme";
        QVERIFY(write(root + "/colors.toml", dark));
        Theme theme(home.path());
        QVERIFY(write(root + "/colors.toml", "background = '#ffffff'\nforeground = 'unterminated"));
        theme.refresh();
        QCOMPARE(theme.background(), QColor("#121212"));
        QVERIFY(QFile::remove(root + "/colors.toml"));
        theme.refresh();
        QCOMPARE(theme.background(), QColor("#121212"));
        QVERIFY(write(root + "/colors.toml", light));
        theme.refresh();
        QCOMPARE(theme.mode(), "light");
        QCOMPARE(theme.background(), QColor("#fffdf1"));
        QTRY_VERIFY(theme.fontFamily() != "monospace");
    }

    void directoryReplacement() {
        QTemporaryDir home;
        const QString current = home.path() + "/.local/state/omarchy/current";
        QVERIFY(write(current + "/theme/colors.toml", dark));
        Theme theme(home.path());
        QSignalSpy changes(&theme, &Theme::changed);
        QVERIFY(write(current + "/next-theme/colors.toml", light));
        QVERIFY(QDir(current + "/theme").removeRecursively());
        QVERIFY(QDir().rename(current + "/next-theme", current + "/theme"));
        QTRY_COMPARE(theme.background(), QColor("#fffdf1"));
        QVERIFY(!changes.isEmpty());
        QVERIFY(write(current + "/theme/colors.toml", dark));
        QTRY_COMPARE(theme.background(), QColor("#121212"));
    }

    void symlinkReplacement() {
        QTemporaryDir home;
        const QString current = home.path() + "/.config/omarchy/current";
        QVERIFY(QDir().mkpath(current));
        QVERIFY(write(home.path() + "/one/colors.toml", dark));
        QVERIFY(write(home.path() + "/two/colors.toml", light));
        QVERIFY(QFile::link(home.path() + "/one", current + "/theme"));
        Theme theme(home.path());
        QCOMPARE(theme.background(), QColor("#121212"));
        QVERIFY(QFile::remove(current + "/theme"));
        QVERIFY(QFile::link(home.path() + "/two", current + "/theme"));
        QTRY_COMPARE(theme.background(), QColor("#fffdf1"));
        QVERIFY(write(home.path() + "/two/colors.toml", dark));
        QTRY_COMPARE(theme.background(), QColor("#121212"));
    }

    void startsWithoutOmarchy() {
        QTemporaryDir home;
        Theme theme(home.path());
        QCOMPARE(theme.name(), "Blockyard");
        QVERIFY(contrast(theme.foreground(), theme.background()) >= 4.5);
        QVERIFY(write(home.path() + "/.local/state/omarchy/current/theme/colors.toml", light));
        QTRY_COMPARE(theme.background(), QColor("#fffdf1"));
    }

    void midtoneContrast() {
        QTemporaryDir home;
        const QString palette = home.path() + "/.local/state/omarchy/current/theme/colors.toml";
        Theme theme(home.path());
        for (const QByteArray hex : {QByteArray("777777"), QByteArray("888888"), QByteArray("aaaaaa")}) {
            QVERIFY(write(palette, "background = '#" + hex + "'\nforeground = '#888888'\nmuted = '#999999'\n"));
            theme.refresh();
            QVERIFY(contrast(theme.foreground(), theme.background()) >= 4.49);
            QVERIFY(contrast(theme.foreground(), theme.surface()) >= 4.49);
            QVERIFY(contrast(theme.muted(), theme.surface()) >= 4.49);
        }
    }
};

QTEST_GUILESS_MAIN(ThemeTest)
#include "theme_test.moc"
