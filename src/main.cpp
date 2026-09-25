#include "controller.h"
#include "desktop/theme.h"
#include "treemap.h"
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  app.setApplicationName("Blockyard");
  app.setApplicationVersion("0.1.0");
  app.setOrganizationName("Blockyard");
  app.setDesktopFileName("com.blockyard.app");
  app.setWindowIcon(QIcon(":/assets/blockyard.svg"));
  QQuickStyle::setStyle("Basic");
  QCommandLineParser parser;
  parser.setApplicationDescription(
      "A native, theme-aware disk explorer for Omarchy.");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(
      "folder", "Folder to scan. Defaults to your home directory.");
  parser.addOption({"theme-home",
                    "Read theme fixtures from this home directory.",
                    "directory"});
  parser.addOption({"screenshot",
                    "Save the application window after scanning, then exit.",
                    "file"});
  parser.process(app);
  Theme theme(parser.isSet("theme-home") ? parser.value("theme-home")
                                         : QDir::homePath());
  Controller controller;
  qmlRegisterType<Treemap>("Blockyard", 1, 0, "Treemap");
  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlEngine::warnings, [](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
          std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
      });
  engine.rootContext()->setContextProperty("theme", &theme);
  engine.rootContext()->setContextProperty("appController", &controller);
  engine.load(QUrl("qrc:/qml/Main.qml"));
  if (engine.rootObjects().isEmpty()) {
    std::fprintf(stderr, "Could not load the Atlas interface.\n");
    return 1;
  }
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  bool capturing = false;
  if (parser.isSet("screenshot")) {
    QObject::connect(&controller, &Controller::stateChanged, &app, [&] {
      if (capturing || controller.scanStatus() == "idle" ||
          controller.scanning())
        return;
      capturing = true;
      QTimer::singleShot(400, &app, [&] {
        const bool saved =
            window && !window->grabWindow().isNull() &&
            window->grabWindow().save(parser.value("screenshot"));
        app.exit(saved ? 0 : 2);
      });
    });
    QTimer::singleShot(120000, &app, [&] { app.exit(3); });
  }
  const auto args = parser.positionalArguments();
  QTimer::singleShot(0, &controller, [&controller, args] {
    controller.startScan(args.isEmpty() ? QDir::homePath() : args.first());
  });
  return app.exec();
}
