#pragma once

#include <QByteArray>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace blockyard {

// Display-only escaping. Never feed this string back into a filesystem operation.
QString displayPath(const QByteArray &native);

// Native names and filesystem identities never make a round trip through display text.
struct Identity {
    quint64 device = 0, inode = 0, links = 0, size = 0;
    quint32 mode = 0;
    qint64 modifiedSeconds = 0, modifiedNanos = 0, changedSeconds = 0, changedNanos = 0;
};
struct Node {
    int id = 0, parent = -1;
    QByteArray name;
    bool directory = false, symlink = false;
    quint64 allocated = 0, apparent = 0, files = 0;
    qint64 modified = 0;
    QString category, error, protectedReason;
    std::vector<int> children;
    Identity identity;
};
struct Tree {
    std::vector<Node> nodes;
    QByteArray rootPath;
    quint64 entries = 0;
    int issueCount = 0;
    QStringList issues;
    bool cancelled = false;
    double elapsedMs = 0;
    QByteArray pathFor(int id) const;
};

std::shared_ptr<Tree> scan(const QByteArray &root, std::atomic_bool &cancel,
                          std::function<void(quint64, double)> progress = {});
// Available to this user, followed by filesystem capacity, in bytes.
std::pair<quint64, quint64> diskSpace(const QByteArray &path);

enum class CleanupAction { Trash, Permanent };
struct PreparedCleanup {
    std::shared_ptr<const Tree> tree;
    std::vector<int> ids;
    quint64 allocated = 0;
    QStringList warnings;
};
struct CleanupOutcome { QString path; bool success = false; QString message; };
PreparedCleanup prepareCleanup(std::shared_ptr<const Tree> tree, std::vector<int> ids);
std::vector<CleanupOutcome> executeCleanup(const PreparedCleanup &prepared,
    CleanupAction action, std::atomic_bool &cancel);

} // namespace blockyard
