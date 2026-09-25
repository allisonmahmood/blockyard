#include "engine/engine.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace blockyard;

namespace {
bool writeFile(const QByteArray &path, const QByteArray &bytes = "payload") {
    const int fd = ::open(path.constData(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    const bool ok = ::write(fd, bytes.constData(), size_t(bytes.size())) == bytes.size();
    ::close(fd);
    return ok;
}
bool exists(const QByteArray &path) { struct stat st {}; return ::lstat(path.constData(), &st) == 0; }
int find(const Tree &tree, const QByteArray &relative) {
    for (const auto &node : tree.nodes) if (tree.pathFor(node.id) == tree.rootPath + '/' + relative) return node.id;
    return -1;
}
struct Environment {
    QByteArray home = qgetenv("HOME"), data = qgetenv("XDG_DATA_HOME");
    bool hadHome = qEnvironmentVariableIsSet("HOME"), hadData = qEnvironmentVariableIsSet("XDG_DATA_HOME");
    ~Environment() {
        if (hadHome) qputenv("HOME", home); else qunsetenv("HOME");
        if (hadData) qputenv("XDG_DATA_HOME", data); else qunsetenv("XDG_DATA_HOME");
    }
};
}

class EngineTest : public QObject {
    Q_OBJECT
private slots:
    void displayNamesRemainDistinctAndCannotHideControlCharacters() {
        QCOMPARE(displayPath(QByteArray("file-") + char(0xff)), QStringLiteral("file-\\xFF"));
        QCOMPARE(displayPath(QByteArray("file-") + char(0xfe)), QStringLiteral("file-\\xFE"));
        QCOMPARE(displayPath(QByteArray("truncated-") + char(0xc3)), QStringLiteral("truncated-\\xC3"));
        QCOMPARE(displayPath("file-\\xFF"), QStringLiteral("file-\\\\xFF"));
        QCOMPARE(displayPath("line\nreturn\rtab\t"), QStringLiteral("line\\nreturn\\rtab\\t"));
        QCOMPARE(displayPath(QStringLiteral("café-猫").toUtf8()), QStringLiteral("café-猫"));
        QCOMPARE(displayPath(QStringLiteral("name\u202Etxt\u2069").toUtf8()), QStringLiteral("name\\u202Etxt\\u2069"));
        QCOMPARE(displayPath(QStringLiteral("\uFEFFname").toUtf8()), QStringLiteral("\\uFEFFname"));
        QCOMPARE(displayPath(QByteArray("nul\0name", 8)), QStringLiteral("nul\\x00name"));
    }

    void nativeNamesHiddenSparseAndHardlinks() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto root = QFile::encodeName(tmp.path());
        const auto odd = QByteArray("line\nquote'-$()--") + char(0xff);
        QVERIFY(writeFile(root + '/' + odd, "abc"));
        QVERIFY(writeFile(root + "/.hidden", "hidden"));
        QVERIFY(writeFile(root + "/original", "linked payload"));
        QVERIFY(::link((root + "/original").constData(), (root + "/second-link").constData()) == 0);
        int fd = ::open((root + "/sparse").constData(), O_CREAT | O_WRONLY, 0600);
        QVERIFY(fd >= 0);
        QVERIFY(::ftruncate(fd, 32 * 1024 * 1024) == 0);
        ::close(fd);
        QVERIFY(::symlink("missing", (root + "/broken").constData()) == 0);
        QVERIFY(::symlink(".", (root + "/loop").constData()) == 0);
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        QCOMPARE(tree->entries, quint64(8));
        QVERIFY(find(*tree, odd) > 0);
        QVERIFY(find(*tree, ".hidden") > 0);
        const auto &sparse = tree->nodes[size_t(find(*tree, "sparse"))];
        QCOMPARE(sparse.apparent, quint64(32 * 1024 * 1024));
        QVERIFY(sparse.allocated < sparse.apparent);
        const auto &one = tree->nodes[size_t(find(*tree, "original"))];
        const auto &two = tree->nodes[size_t(find(*tree, "second-link"))];
        QCOMPARE(one.apparent + two.apparent, quint64(14));
        QVERIFY(tree->nodes[size_t(find(*tree, "broken"))].symlink);
        QVERIFY(tree->nodes[size_t(find(*tree, "loop"))].children.empty());
        QCOMPARE(tree->nodes[0].files, quint64(7));
        QVERIFY(::unlink((root + '/' + odd).constData()) == 0);
    }

    void cancellationLeavesPartialTreeAndBlocksCleanup() {
        QTemporaryDir tmp;
        const auto root = QFile::encodeName(tmp.path());
        for (int i = 0; i < 100; ++i) QVERIFY(writeFile(root + '/' + QByteArray::number(i)));
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel, [&](quint64, double) { cancel = true; });
        QVERIFY(tree->cancelled);
        QVERIFY(tree->entries < 101);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(tree, {1}));
    }

    void normalizesSelectionAndRejectsProtectedData() {
        QTemporaryDir tmp;
        const auto root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/build/nested"));
        QVERIFY(writeFile(root + "/build/nested/output"));
        QVERIFY(QDir().mkpath(tmp.path() + "/worktree"));
        QVERIFY(writeFile(root + "/worktree/.git", "gitdir: /not-read"));
        QVERIFY(writeFile(root + "/worktree/precious"));
        QVERIFY(QDir().mkpath(tmp.path() + "/.config/app"));
        QVERIFY(writeFile(root + "/.config/app/settings"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        const int build = find(*tree, "build"), output = find(*tree, "build/nested/output");
        auto prepared = prepareCleanup(tree, {output, build, build});
        QCOMPARE(prepared.ids.size(), size_t(1));
        QCOMPARE(prepared.ids[0], build);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(tree, {0}));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(tree, {find(*tree, "worktree")}));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(tree, {find(*tree, "worktree/precious")}));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(tree, {find(*tree, ".config/app/settings")}));
        const auto worktree = scan(root + "/worktree", cancel);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(worktree, {find(*worktree, "precious")}));
        QVERIFY(QDir().mkpath(tmp.path() + "/worktree/inside"));
        QVERIFY(writeFile(root + "/worktree/inside/data"));
        const auto nested = scan(root + "/worktree/inside", cancel);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(nested, {find(*nested, "data")}));
    }

    void permanentRemovalKeepsOutsideSymlinkTarget() {
        QTemporaryDir tmp;
        const auto base = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/scan/build/nested"));
        QVERIFY(writeFile(base + "/sentinel", "keep"));
        QVERIFY(writeFile(base + "/scan/build/nested/output"));
        QVERIFY(::symlink((base + "/sentinel").constData(), (base + "/scan/build/link").constData()) == 0);
        std::atomic_bool cancel = false;
        const auto tree = scan(base + "/scan", cancel);
        const auto result = executeCleanup(prepareCleanup(tree, {find(*tree, "build")}), CleanupAction::Permanent, cancel);
        QCOMPARE(result.size(), size_t(1));
        QVERIFY2(result[0].success, qPrintable(result[0].message));
        QVERIFY(!exists(base + "/scan/build"));
        QVERIFY(exists(base + "/sentinel"));
        QCOMPARE(QDir(tmp.path() + "/scan").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    }

    void permanentRemovalHandlesHardlinksWithinFolder() {
        QTemporaryDir tmp;
        const auto base = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/build"));
        QVERIFY(writeFile(base + "/build/one"));
        QVERIFY(::link((base + "/build/one").constData(), (base + "/build/two").constData()) == 0);
        QVERIFY(::link((base + "/build/one").constData(), (base + "/retained").constData()) == 0);
        std::atomic_bool cancel = false;
        const auto tree = scan(base, cancel);
        const auto result = executeCleanup(prepareCleanup(tree, {find(*tree, "build")}), CleanupAction::Permanent, cancel);
        QVERIFY2(result[0].success, qPrintable(result[0].message));
        QVERIFY(exists(base + "/retained"));
        QVERIFY(!exists(base + "/build"));
    }

    void replacementAndChangedDescendantsFailClosed() {
        QTemporaryDir tmp;
        const auto base = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/build"));
        QVERIFY(writeFile(base + "/build/output"));
        std::atomic_bool cancel = false;
        auto tree = scan(base, cancel);
        auto prepared = prepareCleanup(tree, {find(*tree, "build/output")});
        QVERIFY(::rename((base + "/build/output").constData(), (base + "/saved").constData()) == 0);
        QVERIFY(writeFile(base + "/build/output", "replacement"));
        auto result = executeCleanup(prepared, CleanupAction::Permanent, cancel);
        QVERIFY(!result[0].success);
        QVERIFY(exists(base + "/build/output"));
        tree = scan(base, cancel);
        prepared = prepareCleanup(tree, {find(*tree, "build")});
        QVERIFY(writeFile(base + "/build/new-user-file"));
        result = executeCleanup(prepared, CleanupAction::Permanent, cancel);
        QVERIFY(!result[0].success);
        QVERIFY(exists(base + "/build/new-user-file"));
        QVERIFY(exists(base + "/saved"));
    }

    void separatelySelectedHardlinksCanBothBeRemoved() {
        QTemporaryDir tmp;
        const auto root = QFile::encodeName(tmp.path());
        QVERIFY(writeFile(root + "/one"));
        QVERIFY(::link((root + "/one").constData(), (root + "/two").constData()) == 0);
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        const auto results = executeCleanup(prepareCleanup(tree, {find(*tree, "one"), find(*tree, "two")}), CleanupAction::Permanent, cancel);
        QCOMPARE(results.size(), size_t(2));
        QVERIFY2(results[0].success, qPrintable(results[0].message));
        QVERIFY2(results[1].success, qPrintable(results[1].message));
        QVERIFY(!exists(root + "/one"));
        QVERIFY(!exists(root + "/two"));
    }

    void ancestorSymlinkSwapCannotEscape() {
        QTemporaryDir tmp;
        const auto base = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/scan/parent"));
        QVERIFY(QDir().mkpath(tmp.path() + "/outside"));
        QVERIFY(writeFile(base + "/scan/parent/item"));
        QVERIFY(writeFile(base + "/outside/item", "sentinel"));
        std::atomic_bool cancel = false;
        const auto tree = scan(base + "/scan", cancel);
        const auto prepared = prepareCleanup(tree, {find(*tree, "parent/item")});
        QVERIFY(::rename((base + "/scan/parent").constData(), (base + "/saved-parent").constData()) == 0);
        QVERIFY(::symlink((base + "/outside").constData(), (base + "/scan/parent").constData()) == 0);
        const auto result = executeCleanup(prepared, CleanupAction::Permanent, cancel);
        QVERIFY(!result[0].success);
        QVERIFY(exists(base + "/outside/item"));
        QVERIFY(exists(base + "/saved-parent/item"));
    }

    void cancelledCleanupDoesNotMutate() {
        QTemporaryDir tmp;
        const auto root = QFile::encodeName(tmp.path());
        QVERIFY(writeFile(root + "/item"));
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        auto prepared = prepareCleanup(tree, {find(*tree, "item")});
        cancel = true;
        const auto result = executeCleanup(prepared, CleanupAction::Permanent, cancel);
        QVERIFY(!result[0].success);
        QVERIFY(exists(root + "/item"));
    }

    void trashPreservesNativePathAndSupportsSystemRestore() {
        QTemporaryDir tmp;
        Environment environment;
        const auto base = QFile::encodeName(tmp.path());
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        QVERIFY(QDir().mkpath(tmp.path() + "/data"));
        QVERIFY(QDir().mkpath(tmp.path() + "/scan"));
        const auto name = QByteArray("odd\n#% name-") + char(0xff);
        const auto original = base + "/scan/" + name;
        QVERIFY(writeFile(original));
        std::atomic_bool cancel = false;
        auto tree = scan(base + "/scan", cancel);
        auto result = executeCleanup(prepareCleanup(tree, {find(*tree, name)}), CleanupAction::Trash, cancel);
        QVERIFY2(result[0].success, qPrintable(result[0].message));
        QVERIFY(!exists(original));
        const QDir metadata(tmp.path() + "/data/Trash/info");
        const auto entries = metadata.entryList({"*.trashinfo"}, QDir::Files);
        QCOMPARE(entries.size(), 1);
        QFile info(metadata.filePath(entries[0]));
        QVERIFY(info.open(QIODevice::ReadOnly));
        const auto content = info.readAll();
        QVERIFY(content.contains("%0A%23%25%20name-%FF"));
        const auto encodedPath = content.split('\n')[1].mid(5);
        QCOMPARE(QByteArray::fromPercentEncoding(encodedPath), original);
        const auto trashName = QFile::encodeName(entries[0].chopped(10));
        QVERIFY(exists(base + "/data/Trash/files/" + trashName));
        QVERIFY(::rename((base + "/data/Trash/files/" + trashName).constData(), original.constData()) == 0);
        QVERIFY(exists(original));
        QVERIFY(::unlink(original.constData()) == 0);
        info.close();
        QVERIFY(info.remove());

        // A second ordinary name is restored by the system tool in this isolated XDG home.
        QVERIFY(writeFile(base + "/scan/restore-me"));
        tree = scan(base + "/scan", cancel);
        result = executeCleanup(prepareCleanup(tree, {find(*tree, "restore-me")}), CleanupAction::Trash, cancel);
        QVERIFY2(result[0].success, qPrintable(result[0].message));
        const auto gio = QStandardPaths::findExecutable("gio");
        const auto dbus = QStandardPaths::findExecutable("dbus-run-session");
        if (!gio.isEmpty() && !dbus.isEmpty()) {
            QVERIFY(QDir().mkpath(tmp.path() + "/runtime"));
            QVERIFY(::chmod((base + "/runtime").constData(), 0700) == 0);
            QProcess process;
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert("XDG_RUNTIME_DIR", tmp.path() + "/runtime");
            env.insert("XDG_CACHE_HOME", tmp.path() + "/cache");
            env.insert("XDG_CONFIG_HOME", tmp.path() + "/config");
            process.setProcessEnvironment(env);
            process.start(dbus, {"--", gio, "trash", "--list"});
            QVERIFY(process.waitForFinished(5000));
            // GVfs may be unavailable in headless CI; metadata correctness above remains mandatory.
            if (process.exitCode() == 0) {
                const auto listing = process.readAllStandardOutput();
                QVERIFY2(listing.contains("restore-me"), listing.constData());
                const auto names = metadata.entryList({"*.trashinfo"}, QDir::Files);
                QCOMPARE(names.size(), 1);
                process.start(dbus, {"--", gio, "trash", "--restore", "trash:///" + names[0].chopped(10)});
                QVERIFY(process.waitForFinished(5000));
                QCOMPARE(process.exitCode(), 0);
                QVERIFY(exists(base + "/scan/restore-me"));
            }
            // The private D-Bus session exits before GVfs finishes its metadata flush.
            QTest::qWait(100);
            QTRY_VERIFY_WITH_TIMEOUT(QDir(tmp.path()).removeRecursively(), 1000);
        }
    }

    void trashRejectsSymlinkDirectoryAndKeepsSelection() {
        QTemporaryDir tmp;
        Environment environment;
        const auto base = QFile::encodeName(tmp.path());
        qputenv("HOME", base); qputenv("XDG_DATA_HOME", base + "/data");
        QVERIFY(QDir().mkpath(tmp.path() + "/data"));
        QVERIFY(QDir().mkpath(tmp.path() + "/outside"));
        QVERIFY(QDir().mkpath(tmp.path() + "/scan"));
        QVERIFY(::symlink((base + "/outside").constData(), (base + "/data/Trash").constData()) == 0);
        QVERIFY(writeFile(base + "/scan/item"));
        std::atomic_bool cancel = false;
        const auto tree = scan(base + "/scan", cancel);
        const auto result = executeCleanup(prepareCleanup(tree, {find(*tree, "item")}), CleanupAction::Trash, cancel);
        QVERIFY(!result[0].success);
        QVERIFY(exists(base + "/scan/item"));
        QVERIFY(QDir(tmp.path() + "/outside").isEmpty());
    }

    void permissionErrorsAreVisible() {
        if (::getuid() == 0) QSKIP("Permission fixture needs an unprivileged user");
        QTemporaryDir tmp;
        const auto root = QFile::encodeName(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + "/private"));
        QVERIFY(writeFile(root + "/private/item"));
        QVERIFY(::chmod((root + "/private").constData(), 0000) == 0);
        std::atomic_bool cancel = false;
        const auto tree = scan(root, cancel);
        QVERIFY(::chmod((root + "/private").constData(), 0700) == 0);
        QVERIFY(tree->issueCount > 0);
        QVERIFY(!tree->nodes[size_t(find(*tree, "private"))].error.isEmpty());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepareCleanup(tree, {find(*tree, "private")}));
    }
};

QTEST_GUILESS_MAIN(EngineTest)
#include "engine_test.moc"
