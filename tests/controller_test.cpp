#include "controller.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class ControllerTest : public QObject {
  Q_OBJECT
  static void write(const QString &path, QByteArray bytes = "hello") {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bytes), bytes.size());
  }
  static int find(const QVariantList &rows, const QString &name) {
    for (const auto &row : rows)
      if (row.toMap().value("name") == name)
        return row.toMap().value("id").toInt();
    return -1;
  }
private slots:
  void scanNavigationAndQueue() {
    QTemporaryDir dir;
    write(dir.filePath("alpha/one.txt"));
    write(dir.filePath("alpha/two.txt"));
    write(dir.filePath("beta.txt"));
    Controller c;
    c.startScan(dir.path());
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    QCOMPARE(c.progressEntries(), 5);
    QCOMPARE(c.rowsTotal(), 2);
    const int alpha = find(c.rows(), "alpha");
    QVERIFY(alpha > 0);
    c.navigate(alpha);
    QCOMPARE(c.breadcrumbs().size(), 2);
    QCOMPARE(c.rowsTotal(), 2);
    const int one = find(c.rows(), "one.txt");
    c.toggleMark(one);
    QCOMPARE(c.queue().size(), 1);
    c.goUp();
    c.toggleMark(alpha);
    QCOMPARE(c.queue().size(), 1);
    QCOMPARE(c.queue().first().toMap().value("id").toInt(), alpha);
    c.setFilter("two");
    QTRY_COMPARE(c.rowsTotal(), 1);
    QCOMPARE(c.rows().first().toMap().value("name").toString(),
             QString("two.txt"));
    c.clearQueue();
    QVERIFY(c.prepareReview().value("error").toString().size() > 0);
  }
  void newScanInvalidatesReview() {
    QTemporaryDir dir;
    write(dir.filePath("keep.txt"));
    Controller c;
    c.startScan(dir.path());
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    c.toggleMark(find(c.rows(), "keep.txt"));
    QVERIFY(c.prepareReview().value("error").toString().isEmpty());
    c.rescan();
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    QSignalSpy result(&c, &Controller::cleanupFinished);
    c.executeReview("permanent");
    QVERIFY(!c.busy());
    QCOMPARE(result.count(), 0);
    QVERIFY(QFile::exists(dir.filePath("keep.txt")));
  }
  void reviewedFixtureRemovalAndRescan() {
    QTemporaryDir dir;
    write(dir.filePath("remove.txt"));
    write(dir.filePath("keep.txt"));
    Controller c;
    c.startScan(dir.path());
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    c.toggleMark(find(c.rows(), "remove.txt"));
    QVERIFY(c.prepareReview().value("error").toString().isEmpty());
    QSignalSpy result(&c, &Controller::cleanupFinished);
    c.executeReview("permanent");
    QTRY_COMPARE(result.count(), 1);
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    QVERIFY(!QFile::exists(dir.filePath("remove.txt")));
    QVERIFY(QFile::exists(dir.filePath("keep.txt")));
    QCOMPARE(c.rowsTotal(), 1);
    const auto outcomes = result.first().first().toList();
    QVERIFY(outcomes.first().toMap().value("success").toBool());
  }
  void changedSelectionFailsWithoutRemovingReplacement() {
    QTemporaryDir dir;
    write(dir.filePath("keep.txt"));
    Controller c;
    c.startScan(dir.path());
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    c.toggleMark(find(c.rows(), "keep.txt"));
    c.prepareReview();
    QVERIFY(
        QFile::rename(dir.filePath("keep.txt"), dir.filePath("original.txt")));
    write(dir.filePath("keep.txt"), "different");
    QSignalSpy result(&c, &Controller::cleanupFinished);
    c.executeReview("permanent");
    QTRY_COMPARE(result.count(), 1);
    QVERIFY(QFile::exists(dir.filePath("keep.txt")));
    QVERIFY(QFile::exists(dir.filePath("original.txt")));
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    QCOMPARE(c.queue().size(), 1);
    QVERIFY(!result.first()
                 .first()
                 .toList()
                 .first()
                 .toMap()
                 .value("success")
                 .toBool());
  }
  void scanSwitchUsesNewRoot() {
    QTemporaryDir first, second;
    for (int i = 0; i < 600; ++i)
      write(first.filePath(QString::number(i)));
    write(second.filePath("final.txt"));
    Controller c;
    c.startScan(first.path());
    c.startScan(second.path());
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    QCOMPARE(c.rootPath(), second.path());
    QCOMPARE(c.progressEntries(), 2);
    QCOMPARE(c.rows().first().toMap().value("name").toString(),
             QString("final.txt"));
  }
  void paginatesWideFolderAndBoundedMap() {
    QTemporaryDir dir;
    for (int i = 0; i < 425; ++i)
      write(dir.filePath(QString("item-%1").arg(i)));
    Controller c;
    c.startScan(dir.path());
    QTRY_COMPARE(c.scanStatus(), QString("complete"));
    QCOMPARE(c.rowsTotal(), 425);
    QCOMPARE(c.rows().size(), 200);
    c.loadMoreRows();
    QCOMPARE(c.rows().size(), 400);
    QVERIFY(c.mapCells().size() <= 161);
    c.setMetric("files");
    QCOMPARE(c.scanBytes(), 425.0);
  }
};
QTEST_GUILESS_MAIN(ControllerTest)
#include "controller_test.moc"
