#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>
#include "../../src/treemap.h"
#include "../../src/desktop/theme.h"

class UiTest final : public QObject {
    Q_OBJECT
    QTemporaryDir fixtureHome;
    std::unique_ptr<QQmlApplicationEngine> engine;
    std::unique_ptr<Theme> theme;
    QObject* controller = nullptr;
    QQuickWindow* window = nullptr;
    QStringList warnings;
private slots:
    void initTestCase() {
        QVERIFY(fixtureHome.isValid());
        qmlRegisterType<Treemap>("Blockyard", 1, 0, "Treemap");
        engine = std::make_unique<QQmlApplicationEngine>();
        connect(engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& errors) {
            for (const auto& error : errors) warnings.append(error.toString());
        });
        QQmlComponent fixture(engine.get(), QUrl::fromLocalFile(QStringLiteral(BLOCKYARD_SOURCE_DIR "/tests/qml/Controller.qml")));
        controller = fixture.create();
        QVERIFY2(controller, qPrintable(fixture.errorString()));
        controller->setParent(engine.get());
        theme = std::make_unique<Theme>(fixtureHome.path());
        engine->rootContext()->setContextProperty("theme", theme.get());
        engine->rootContext()->setContextProperty("appController", controller);
        engine->load(QUrl::fromLocalFile(QStringLiteral(BLOCKYARD_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY2(!engine->rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void reviewDoesNotMutateUntilChosenAction() {
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(controller->property("queueBytes").toDouble(), 4096.0);
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        QObject* review = window->findChild<QObject*>("reviewDialog");
        QVERIFY(review);
        QTRY_VERIFY(review->property("visible").toBool());
        QCOMPARE(controller->property("mutationCalls").toInt(), 0);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!review->property("visible").toBool());
        QCOMPARE(controller->property("mutationCalls").toInt(), 0);
    }
    void compactLayoutKeepsMapUsable() {
        window->resize(640, 480);
        QTRY_VERIFY(window->property("compact").toBool());
        auto* map = window->findChild<QQuickItem*>("storageMap");
        QVERIFY(map);
        QVERIFY(map->width() > 500);
        QVERIFY(map->height() > 200);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void permanentRemovalRequiresExplicitConfirmation() {
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        auto* choice = window->findChild<QObject*>("permanentChoice");
        auto* confirmation = window->findChild<QObject*>("permanentDialog");
        auto* confirmButton = window->findChild<QObject*>("confirmPermanent");
        QVERIFY(choice && confirmation && confirmButton);
        QVERIFY(QMetaObject::invokeMethod(choice, "clicked"));
        QTRY_VERIFY(confirmation->property("visible").toBool());
        QCOMPARE(controller->property("mutationCalls").toInt(), 0);
        bool exactPathVisible = false;
        for (QObject* child : confirmation->findChildren<QObject*>()) {
            if (child->property("text").toString() == "/fixture/Disposable") exactPathVisible = true;
        }
        QVERIFY(exactPathVisible);
        QVERIFY(QMetaObject::invokeMethod(confirmButton, "clicked"));
        QCOMPARE(controller->property("mutationCalls").toInt(), 1);
        QCOMPARE(controller->property("lastAction").toString(), QStringLiteral("permanent"));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void screenshot() {
        window->resize(1200, 720);
        QTest::qWait(50);
        const auto image = window->grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(QStringLiteral(BLOCKYARD_SOURCE_DIR "/tests/qml/atlas-fixture.png")));
    }
    void filenamesRemainLiteralInReview() {
        QVERIFY(QMetaObject::invokeMethod(controller, "useHostileName"));
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        auto* review = window->findChild<QObject*>("reviewDialog");
        QTRY_VERIFY(review->property("visible").toBool());
        QCoreApplication::processEvents();
        std::function<QQuickItem*(QQuickItem*)> findPath = [&](QQuickItem* item) -> QQuickItem* {
            if (item->property("text").toString() == "/fixture/<b>literal</b>") return item;
            for (auto* child : item->childItems()) if (auto* match = findPath(child)) return match;
            return nullptr;
        };
        auto* reviewContent = review->property("contentItem").value<QQuickItem*>();
        QVERIFY(reviewContent);
        QTRY_VERIFY(findPath(reviewContent));
        QCOMPARE(findPath(reviewContent)->property("textFormat").toInt(), 0);
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void cleanupTestCase() { engine.reset(); theme.reset(); }
};
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    UiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
