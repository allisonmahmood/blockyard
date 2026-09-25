#include "controller.h"
#include "desktop/theme.h"
#include "treemap.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {
bool writeFile(const QString& path, const QByteArray& contents) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QQuickItem* visibleText(QQuickItem* root, const QString& text) {
    if (root->isVisible() && root->property("text").toString() == text) return root;
    for (auto* child : root->childItems())
        if (auto* match = visibleText(child, text)) return match;
    return nullptr;
}

QByteArray palette(const QString& stockName, const QByteArray& fallback) {
    QFile file("/usr/share/omarchy/themes/" + stockName + "/colors.toml");
    return file.open(QIODevice::ReadOnly) ? file.readAll() : fallback;
}
}

class LiveAppTest final : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> fixture;
    std::unique_ptr<Theme> theme;
    std::unique_ptr<Controller> controller;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow* window = nullptr;
    QStringList warnings;
    QString scanRoot, currentTheme;

    void click(QQuickItem* item) {
        QVERIFY(item);
        QTRY_VERIFY(item->isVisible() && item->isEnabled());
        const QPoint point = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        QVERIFY(window->geometry().size().isValid());
        QVERIFY(point.x() >= 0 && point.y() >= 0 && point.x() < window->width() && point.y() < window->height());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
    }

private slots:
    void initTestCase() {
        qmlRegisterType<Treemap>("Blockyard", 1, 0, "Treemap");
    }

    void init() {
        warnings.clear();
        fixture = std::make_unique<QTemporaryDir>();
        QVERIFY(fixture->isValid());
        scanRoot = fixture->path() + "/scan";
        currentTheme = fixture->path() + "/.local/state/omarchy/current";
        QVERIFY(QDir().mkpath(scanRoot));
        QVERIFY(QDir().mkpath(fixture->path() + "/data"));
        qputenv("HOME", QFile::encodeName(fixture->path()));
        qputenv("XDG_DATA_HOME", QFile::encodeName(fixture->path() + "/data"));
        QVERIFY(writeFile(currentTheme + "/theme/colors.toml", palette("tokyo-night", "background='#151719'\nforeground='#efefef'\n")));
        theme = std::make_unique<Theme>(fixture->path());
        controller = std::make_unique<Controller>();
        engine = std::make_unique<QQmlApplicationEngine>();
        connect(engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& errors) {
            for (const auto& error : errors) warnings.append(error.toString());
        });
        engine->rootContext()->setContextProperty("theme", theme.get());
        engine->rootContext()->setContextProperty("appController", controller.get());
        engine->load(QUrl::fromLocalFile(QStringLiteral(BLOCKYARD_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY2(!engine->rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTRY_VERIFY(theme->fontFamily() != "monospace");
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }

    void reviewedCleanup_data() {
        QTest::addColumn<bool>("permanent");
        QTest::newRow("trash") << false;
        QTest::newRow("explicit-permanent-confirmation") << true;
    }

    void reviewedCleanup() {
        QFETCH(bool, permanent);
        // A single ordinary file gives one treemap box and an unambiguous click target.
        const QString nativePath = scanRoot + "/<b>literal & selected.txt";
        QVERIFY(writeFile(nativePath, QByteArray(8192, 'x')));
        controller->startScan(scanRoot);
        QTRY_COMPARE(controller->scanStatus(), QString("complete"));
        QCOMPARE(controller->rows().size(), 1);
        auto* map = window->findChild<QQuickItem*>("storageMap");
        QVERIFY(map);
        QTRY_VERIFY(map->width() > 0 && map->height() > 0);
        // Wait for the real painted treemap to build its hit regions.
        QTRY_VERIFY(!window->grabWindow().isNull());
        click(map);
        QTRY_COMPARE(controller->selected().value("path").toString(), nativePath);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(controller->queue().size(), 1);
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        auto* review = window->findChild<QObject*>("reviewDialog");
        QVERIFY(review);
        QTRY_VERIFY(review->property("visible").toBool());
        auto* content = review->property("contentItem").value<QQuickItem*>();
        QVERIFY(content);
        QTRY_VERIFY(visibleText(content, nativePath));
        QCOMPARE(visibleText(content, nativePath)->property("textFormat").toInt(), 0);
        QVERIFY(QFileInfo::exists(nativePath));
        QVERIFY(!controller->busy());

        QSignalSpy finished(controller.get(), &Controller::cleanupFinished);
        if (permanent) {
            click(window->findChild<QQuickItem*>("permanentChoice"));
            auto* confirmation = window->findChild<QObject*>("permanentDialog");
            QVERIFY(confirmation);
            QTRY_VERIFY(confirmation->property("visible").toBool());
            QVERIFY(QFileInfo::exists(nativePath));
            QCOMPARE(finished.size(), 0);
            auto* confirmationContent = confirmation->property("contentItem").value<QQuickItem*>();
            QVERIFY(confirmationContent);
            QTRY_VERIFY(visibleText(confirmationContent, nativePath));
            click(window->findChild<QQuickItem*>("confirmPermanent"));
        } else {
            click(window->findChild<QQuickItem*>("moveToTrash"));
        }
        QTRY_COMPARE(finished.size(), 1);
        const QVariantList outcomes = finished[0][0].toList();
        QCOMPARE(outcomes.size(), 1);
        QCOMPARE(outcomes[0].toMap().value("path").toString(), nativePath);
        QVERIFY2(outcomes[0].toMap().value("success").toBool(), qPrintable(outcomes[0].toMap().value("message").toString()));
        auto* result = window->findChild<QObject*>("resultDialog");
        QVERIFY(result);
        QTRY_VERIFY(result->property("visible").toBool());
        QTRY_COMPARE(controller->scanStatus(), QString("complete"));
        QCOMPARE(controller->rows().size(), 0);
        QCOMPARE(controller->queue().size(), 0);
        QVERIFY(!QFileInfo::exists(nativePath));
        if (!permanent) {
            const QDir trash(fixture->path() + "/data/Trash/files");
            QCOMPARE(trash.entryList(QDir::Files).size(), 1);
            const QDir info(fixture->path() + "/data/Trash/info");
            const auto entries = info.entryList({"*.trashinfo"}, QDir::Files);
            QCOMPARE(entries.size(), 1);
            QFile metadata(info.filePath(entries.front()));
            QVERIFY(metadata.open(QIODevice::ReadOnly));
            const auto lines = metadata.readAll().split('\n');
            QVERIFY(lines.size() >= 3);
            QCOMPARE(QByteArray::fromPercentEncoding(lines[1].mid(5)), QFile::encodeName(nativePath));
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }

    void themeReplacementRepaintsOpenApplication() {
        QVERIFY(writeFile(scanRoot + "/fixture.txt", QByteArray(4096, 'x')));
        controller->startScan(scanRoot);
        QTRY_COMPARE(controller->scanStatus(), QString("complete"));
        QTRY_COMPARE(window->color(), theme->background());
        QImage before;
        QTRY_VERIFY(!(before = window->grabWindow()).isNull());
        const QColor oldBackground = theme->background();
        QCOMPARE(before.pixelColor(1, 1), oldBackground);
        QSignalSpy changed(theme.get(), &Theme::changed);
        QVERIFY(writeFile(currentTheme + "/next-theme/colors.toml", palette("catppuccin-latte", "mode='light'\nbackground='#fffdf1'\nforeground='#242424'\n")));
        QVERIFY(QDir(currentTheme + "/theme").removeRecursively());
        QVERIFY(QDir().rename(currentTheme + "/next-theme", currentTheme + "/theme"));
        QTRY_VERIFY(theme->background() != oldBackground);
        QTRY_COMPARE(theme->mode(), QString("light"));
        QVERIFY(!changed.isEmpty());
        QTRY_COMPARE(window->color(), theme->background());
        QImage after;
        QTRY_VERIFY(!(after = window->grabWindow()).isNull() && after.pixelColor(1, 1) == theme->background());
        QVERIFY(before != after);
        QCOMPARE(controller->rows().size(), 1);
        QCOMPARE(controller->scanStatus(), QString("complete"));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }

    void cleanup() {
        engine.reset();
        window = nullptr;
        controller.reset();
        theme.reset();
        fixture.reset();
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
};

int main(int argc, char** argv) {
    QTemporaryDir isolatedHome;
    if (!isolatedHome.isValid()) return 1;
    qputenv("HOME", QFile::encodeName(isolatedHome.path()));
    qputenv("XDG_DATA_HOME", QFile::encodeName(isolatedHome.path() + "/data"));
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Basic");
    LiveAppTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "live_app_test.moc"
