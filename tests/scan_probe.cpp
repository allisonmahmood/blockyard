#include "engine/engine.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <sys/resource.h>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  if (argc != 2)
    return 2;
  std::atomic_bool cancelled = false;
  try {
    auto tree = blockyard::scan(
        QFile::encodeName(QString::fromLocal8Bit(argv[1])), cancelled);
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    QJsonObject data{{"entries", static_cast<double>(tree->entries)},
                     {"elapsedMs", tree->elapsedMs},
                     {"issues", tree->issueCount},
                     {"peakRssKiB", static_cast<double>(usage.ru_maxrss)}};
    std::cout << QJsonDocument(data).toJson(QJsonDocument::Compact).constData()
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
