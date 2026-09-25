#include "controller.h"
#include "engine/engine.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace blockyard;

namespace {
bool writeFile(const QByteArray& path, const QByteArray& contents = "sentinel") {
    QFile file(QString::fromUtf8(path));
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
int findNode(const Tree& tree, const QByteArray& relative) {
    for (const auto& node : tree.nodes)
        if (tree.pathFor(node.id) == tree.rootPath + '/' + relative) return node.id;
    return -1;
}
bool exists(const QByteArray& path) {
    struct stat st {};
    return ::lstat(path.constData(), &st) == 0;
}
struct DataHome {
    QByteArray old = qgetenv("XDG_DATA_HOME");
    bool wasSet = qEnvironmentVariableIsSet("XDG_DATA_HOME");
    explicit DataHome(const QByteArray& path) { qputenv("XDG_DATA_HOME", path); }
    ~DataHome() { if (wasSet) qputenv("XDG_DATA_HOME", old); else qunsetenv("XDG_DATA_HOME"); }
};
}

class AdversarialTest : public QObject {
    Q_OBJECT
private slots:
    void intermediateAncestorReplacedButImmediateParentKept() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/outer/inner"));
        QVERIFY(writeFile(root + "/outer/inner/item"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        const auto review = prepareCleanup(tree, {findNode(*tree, "outer/inner/item")});
        QVERIFY(QDir().rename(tmp.path() + "/outer", tmp.path() + "/saved"));
        QVERIFY(QDir().mkdir(tmp.path() + "/outer"));
        QVERIFY(QDir().rename(tmp.path() + "/saved/inner", tmp.path() + "/outer/inner"));
        const auto outcomes = executeCleanup(review, CleanupAction::Permanent, cancel);
        QVERIFY2(!outcomes.front().success, "Cleanup must reject a changed intermediate ancestor, even if the immediate parent inode survives.");
        QVERIFY(exists(root + "/outer/inner/item"));
    }

    void newWorktreeMarkerProtectsPreviouslyReviewedDescendant() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/project/inner"));
        QVERIFY(writeFile(root + "/project/inner/item"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        const auto review = prepareCleanup(tree, {findNode(*tree, "project/inner/item")});
        QVERIFY(writeFile(root + "/project/.git", "gitdir: /fixture-only\n"));
        const auto outcomes = executeCleanup(review, CleanupAction::Permanent, cancel);
        QVERIFY2(!outcomes.front().success, "The ancestor became a protected Git worktree after review; cleanup must reject the stale classification.");
        QVERIFY(exists(root + "/project/inner/item"));
    }

    void newHardlinkAfterReviewFailsWithoutRemovingEitherName() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(writeFile(root + "/selected"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        const auto review = prepareCleanup(tree, {findNode(*tree, "selected")});
        QCOMPARE(::link((root + "/selected").constData(), (root + "/new-link").constData()), 0);
        const auto outcomes = executeCleanup(review, CleanupAction::Permanent, cancel);
        QVERIFY(!outcomes.front().success);
        QVERIFY(exists(root + "/selected"));
        QVERIFY(exists(root + "/new-link"));
    }

    void symlinkLeafReplacementPreservesBothTargets() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(writeFile(root + "/one"));
        QVERIFY(writeFile(root + "/two"));
        QCOMPARE(::symlink("one", (root + "/selected").constData()), 0);
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        const auto review = prepareCleanup(tree, {findNode(*tree, "selected")});
        // Keep the original inode alive so filesystem inode reuse cannot mask replacement.
        QCOMPARE(::rename((root + "/selected").constData(), (root + "/saved-link").constData()), 0);
        QCOMPARE(::symlink("two", (root + "/selected").constData()), 0);
        const auto outcomes = executeCleanup(review, CleanupAction::Permanent, cancel);
        QVERIFY(!outcomes.front().success);
        QVERIFY(exists(root + "/one"));
        QVERIFY(exists(root + "/two"));
        QVERIFY(exists(root + "/selected"));
    }

    void trashMetadataFailureRestoresSelection() {
        if (::getuid() == 0) QSKIP("Read-only directory fixture needs an unprivileged process.");
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/scan"));
        QVERIFY(QDir().mkpath(tmp.path() + "/data/Trash/info"));
        QVERIFY(QDir().mkpath(tmp.path() + "/data/Trash/files"));
        QCOMPARE(::chmod((root + "/data/Trash").constData(), 0700), 0);
        QCOMPARE(::chmod((root + "/data/Trash/info").constData(), 0500), 0);
        QCOMPARE(::chmod((root + "/data/Trash/files").constData(), 0700), 0);
        DataHome dataHome(root + "/data");
        QVERIFY(writeFile(root + "/scan/selected"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root + "/scan", cancel);
        const auto review = prepareCleanup(tree, {findNode(*tree, "selected")});
        const auto outcomes = executeCleanup(review, CleanupAction::Trash, cancel);
        QCOMPARE(::chmod((root + "/data/Trash/info").constData(), 0700), 0);
        QVERIFY(!outcomes.front().success);
        QVERIFY(exists(root + "/scan/selected"));
        QVERIFY(QDir(tmp.path() + "/data/Trash/files").isEmpty());
        QCOMPARE(QDir(tmp.path() + "/scan").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{"selected"});
    }

    void replacingScanRootPreservesOldAndNewSelections() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/scan"));
        QVERIFY(writeFile(root + "/scan/selected"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root + "/scan", cancel);
        const auto review = prepareCleanup(tree, {findNode(*tree, "selected")});
        QVERIFY(QDir().rename(tmp.path() + "/scan", tmp.path() + "/saved"));
        QVERIFY(QDir().mkpath(tmp.path() + "/scan"));
        QVERIFY(writeFile(root + "/scan/selected"));
        const auto outcomes = executeCleanup(review, CleanupAction::Permanent, cancel);
        QVERIFY(!outcomes.front().success);
        QVERIFY(exists(root + "/scan/selected"));
        QVERIFY(exists(root + "/saved/selected"));
    }

    void controllerQueueMutationInvalidatesPreparedReview() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(writeFile(root + "/one"));
        QVERIFY(writeFile(root + "/two"));
        Controller controller;
        controller.startScan(tmp.path());
        QTRY_COMPARE(controller.scanStatus(), "complete");
        const auto rows = controller.rows();
        QCOMPARE(rows.size(), 2);
        controller.toggleMark(rows[0].toMap().value("id").toInt());
        QVERIFY(controller.prepareReview().value("error").toString().isEmpty());
        controller.toggleMark(rows[1].toMap().value("id").toInt());
        controller.executeReview("permanent");
        QVERIFY(!controller.busy());
        QVERIFY(exists(root + "/one"));
        QVERIFY(exists(root + "/two"));
        QVERIFY(controller.prepareReview().value("error").toString().isEmpty());
        controller.clearQueue();
        controller.executeReview("permanent");
        QVERIFY(!controller.busy());
        QVERIFY(exists(root + "/one"));
        QVERIFY(exists(root + "/two"));
    }

    void controllerNewScanInvalidatesPreparedReview() {
        QTemporaryDir tmp;
        const QByteArray root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/first"));
        QVERIFY(QDir().mkpath(tmp.path() + "/second"));
        QVERIFY(writeFile(root + "/first/one"));
        QVERIFY(writeFile(root + "/second/two"));
        Controller controller;
        controller.startScan(tmp.path() + "/first");
        QTRY_COMPARE(controller.scanStatus(), "complete");
        controller.toggleMark(controller.rows()[0].toMap().value("id").toInt());
        QVERIFY(controller.prepareReview().value("error").toString().isEmpty());
        controller.startScan(tmp.path() + "/second");
        controller.executeReview("permanent");
        QTRY_COMPARE(controller.scanStatus(), "complete");
        controller.executeReview("permanent");
        QVERIFY(!controller.busy());
        QVERIFY(exists(root + "/first/one"));
        QVERIFY(exists(root + "/second/two"));
    }
};

QTEST_GUILESS_MAIN(AdversarialTest)
#include "adversarial_test.moc"
