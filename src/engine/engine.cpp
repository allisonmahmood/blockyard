#include "engine.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStringDecoder>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <limits>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

namespace blockyard {
namespace {
class Fd {
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
    Fd(Fd &&other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Fd &operator=(Fd &&other) noexcept {
        if (this != &other) { if (value_ >= 0) ::close(value_); value_ = std::exchange(other.value_, -1); }
        return *this;
    }
    int get() const { return value_; }
    explicit operator bool() const { return value_ >= 0; }
private:
    int value_;
};

QString errorText(const char *operation) {
    return QString::fromLatin1(operation) + ": " + QString::fromLocal8Bit(std::strerror(errno));
}
[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }

Fd safeOpen(int parent, const QByteArray &name, int flags, bool sameMount = true) {
    struct open_how how {};
    how.flags = static_cast<quint64>(flags | O_CLOEXEC | O_NOFOLLOW);
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS;
    if (sameMount) how.resolve |= RESOLVE_NO_XDEV;
    return Fd(static_cast<int>(::syscall(SYS_openat2, parent, name.constData(), &how, sizeof(how))));
}

Fd openAbsolute(const QByteArray &path, int flags) {
    if (!path.startsWith('/') || path.contains('\0')) fail("Expected an absolute native path");
    Fd slash(::open("/", O_PATH | O_DIRECTORY | O_CLOEXEC));
    return safeOpen(slash.get(), path.size() == 1 ? QByteArray(".") : path.mid(1), flags, false);
}

Identity identity(const struct stat &st) {
    return {static_cast<quint64>(st.st_dev), static_cast<quint64>(st.st_ino),
        static_cast<quint64>(st.st_nlink), static_cast<quint64>(st.st_size),
        static_cast<quint32>(st.st_mode), st.st_mtim.tv_sec, st.st_mtim.tv_nsec,
        st.st_ctim.tv_sec, st.st_ctim.tv_nsec};
}

bool sameObject(const Identity &expected, const struct stat &actual) {
    return expected.device == static_cast<quint64>(actual.st_dev)
        && expected.inode == static_cast<quint64>(actual.st_ino)
        && (expected.mode & S_IFMT) == (actual.st_mode & S_IFMT);
}

bool unchanged(const Identity &expected, const struct stat &actual, bool moved = false) {
    return sameObject(expected, actual) && expected.mode == static_cast<quint32>(actual.st_mode)
        && (moved || expected.links == static_cast<quint64>(actual.st_nlink))
        && expected.size == static_cast<quint64>(actual.st_size)
        && expected.modifiedSeconds == actual.st_mtim.tv_sec
        && expected.modifiedNanos == actual.st_mtim.tv_nsec
        && (moved || (expected.changedSeconds == actual.st_ctim.tv_sec
                   && expected.changedNanos == actual.st_ctim.tv_nsec));
}

std::vector<QByteArray> namesIn(int fd, const std::atomic_bool *cancel = nullptr) {
    // A fresh description avoids sharing a readdir offset with the held descriptor.
    Fd copy(::openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (!copy) fail(errorText("Open directory"));
    const int duplicate = ::dup(copy.get());
    DIR *raw = ::fdopendir(duplicate);
    if (!raw) { if (duplicate >= 0) ::close(duplicate); fail(errorText("Read directory")); }
    const auto closeDirectory = [](DIR *value) { ::closedir(value); };
    std::unique_ptr<DIR, decltype(closeDirectory)> dir(raw, closeDirectory);
    std::vector<QByteArray> names;
    errno = 0;
    while (const auto *entry = ::readdir(dir.get())) {
        if (cancel && cancel->load()) break;
        const QByteArray name(entry->d_name);
        if (name != "." && name != "..") names.push_back(name);
        errno = 0;
    }
    if (errno) fail(errorText("Read directory"));
    std::sort(names.begin(), names.end());
    return names;
}

QByteArray relativePath(const Tree &tree, int id) {
    if (id == 0) return ".";
    const auto full = tree.pathFor(id);
    return full.mid(tree.rootPath.size() + (tree.rootPath == "/" ? 0 : 1));
}

QString categoryFor(const QByteArray &name, const QString &parent, bool directory) {
    const auto lower = name.toLower();
    if (lower == ".cache" || lower == "cache" || lower == "node_modules" || lower == "target"
        || lower == ".next" || lower == ".turbo" || lower == ".npm") return QStringLiteral("cache");
    if (lower == "worktrees" || lower == ".codex" || lower == ".claude") return QStringLiteral("scratch");
    if (lower == ".cargo" || lower == ".rustup" || lower == ".local") return QStringLiteral("tools");
    if (lower == "dev" || lower == "src" || lower == ".git" || lower == "projects") return QStringLiteral("code");
    if (lower == "pictures" || lower == "videos" || lower == "music") return QStringLiteral("media");
    if (lower == "documents" || lower == "downloads") return QStringLiteral("documents");
    if (!directory) {
        const auto ext = lower.mid(lower.lastIndexOf('.') + 1);
        static const QList<QByteArray> media = {"mp4", "mkv", "mov", "webm", "jpg", "jpeg", "png", "flac", "mp3", "wav"};
        if (media.contains(ext)) return QStringLiteral("media");
    }
    return parent;
}

QString protectedPath(const QByteArray &path, const QByteArray &name) {
    if (name.startsWith(".blockyard-cleanup-")) return QStringLiteral("Cleanup recovery directory. Recover its contents with your file manager.");
    static const QList<QByteArray> roots = {"/etc", "/usr", "/var", "/proc", "/sys", "/dev", "/run", "/boot", "/root"};
    for (const auto &root : roots)
        if (path == root || path.startsWith(root + '/')) return QStringLiteral("System-managed data. Inspect only.");
    static const QList<QByteArray> managed = {".git", ".ssh", ".gnupg", ".password-store", ".config", "Trash",
        ".docker", "containers", "docker", "postgres", "postgresql", "mysql", "mariadb", "libvirt", "keyrings"};
    if (managed.contains(name)) return QStringLiteral("Application or service data. Inspect only.");
    if (!path.isEmpty()) for (const auto &component : path.split('/')) {
        if (managed.contains(component)) return QStringLiteral("Application or service data. Inspect only.");
        if (component.startsWith(".blockyard-cleanup-")) return QStringLiteral("Cleanup recovery directory. Recover its contents with your file manager.");
    }
    return {};
}

void addIssue(Tree &tree, Node &node, const QString &message) {
    node.error = message;
    node.protectedReason = QStringLiteral("Incomplete scan. Rescan before cleanup.");
    ++tree.issueCount;
    if (tree.issues.size() < 30) tree.issues.push_back(displayPath(tree.pathFor(node.id)) + ": " + message);
}

void rejectWorktree(int directory) {
    struct stat git {};
    if (::fstatat(directory, ".git", &git, AT_SYMLINK_NOFOLLOW) == 0 && !S_ISDIR(git.st_mode))
        fail("An ancestor is now a Git worktree. Rescan and inspect it with Git before removing data.");
}

bool hasWorktreeAncestor(QByteArray path) {
    for (;;) {
        const auto directory = openAbsolute(path, O_PATH | O_DIRECTORY);
        if (!directory) fail(errorText("Inspect root ancestors"));
        struct stat git {};
        if (::fstatat(directory.get(), ".git", &git, AT_SYMLINK_NOFOLLOW) == 0 && !S_ISDIR(git.st_mode)) return true;
        if (path == "/") return false;
        path = path.left(path.lastIndexOf('/'));
        if (path.isEmpty()) path = "/";
    }
}

using ChangedLinks = std::set<std::pair<quint64, quint64>>;

void validateNode(const Tree &tree, int id, int parent, const QByteArray &name,
                  const std::atomic_bool &cancel, bool moved = false, const ChangedLinks *changedLinks = nullptr) {
    if (cancel.load()) fail("Cleanup cancelled");
    const auto &node = tree.nodes.at(static_cast<size_t>(id));
    struct stat st {};
    if (::fstatat(parent, name.constData(), &st, AT_SYMLINK_NOFOLLOW) < 0) fail(errorText("Recheck selection"));
    const bool linksChanged = changedLinks && changedLinks->contains({node.identity.device, node.identity.inode});
    if (!unchanged(node.identity, st, moved || linksChanged)) fail("Selection changed since the scan. Rescan and review it again.");
    Fd handle = safeOpen(parent, name, node.directory ? O_RDONLY | O_DIRECTORY : O_PATH);
    if (!handle) fail(errorText("Check filesystem boundary"));
    if (!node.directory) return;
    auto names = namesIn(handle.get());
    if (names.size() != node.children.size()) fail("Folder contents changed since the scan. Rescan and review it again.");
    for (size_t i = 0; i < names.size(); ++i) {
        const auto childId = node.children[i];
        if (tree.nodes.at(static_cast<size_t>(childId)).name != names[i]) fail("Folder contents changed since the scan.");
        validateNode(tree, childId, handle.get(), names[i], cancel, false, changedLinks);
    }
}

bool renameNoReplace(int from, const QByteArray &name, int to, const QByteArray &destination) {
    return ::syscall(SYS_renameat2, from, name.constData(), to, destination.constData(), RENAME_NOREPLACE) == 0;
}

QByteArray uniqueName(const char *prefix) { return QByteArray(prefix) + QUuid::createUuid().toByteArray(QUuid::Id128); }

// Each child is detached into the private directory before its identity is checked.
// A writer holding a former directory handle cannot swap the detached leaf name.
void removeStaged(const Tree &tree, int id, int staging, const QByteArray &name,
                  std::atomic_bool &cancel, ChangedLinks &changedLinks) {
    if (cancel.load()) fail("Cleanup cancelled; remaining files were preserved");
    const auto &node = tree.nodes.at(static_cast<size_t>(id));
    struct stat st {};
    if (::fstatat(staging, name.constData(), &st, AT_SYMLINK_NOFOLLOW) < 0) fail(errorText("Check staged item"));
    if (!unchanged(node.identity, st, true)) fail("A staged item changed. Remaining files were preserved.");
    if (!node.directory) {
        if (::unlinkat(staging, name.constData(), 0) < 0) fail(errorText("Remove file"));
        changedLinks.emplace(node.identity.device, node.identity.inode);
        return;
    }
    Fd directory = safeOpen(staging, name, O_RDONLY | O_DIRECTORY);
    if (!directory) fail(errorText("Open staged directory"));
    for (const int childId : node.children) {
        if (cancel.load()) fail("Cleanup cancelled; remaining files were preserved");
        const auto &child = tree.nodes.at(static_cast<size_t>(childId));
        const QByteArray detached = uniqueName("entry-");
        if (!renameNoReplace(directory.get(), child.name, staging, detached)) fail(errorText("Stage child"));
        try {
            removeStaged(tree, childId, staging, detached, cancel, changedLinks);
        } catch (...) {
            // Never overwrite a replacement created by a concurrent writer.
            renameNoReplace(staging, detached, directory.get(), child.name);
            throw;
        }
    }
    if (!namesIn(directory.get()).empty()) fail("New files appeared during cleanup. They were preserved.");
    if (::unlinkat(staging, name.constData(), AT_REMOVEDIR) < 0) fail(errorText("Remove directory"));
}

Fd privateDirectory(int parent, const QByteArray &name, bool create) {
    if (create && ::mkdirat(parent, name.constData(), 0700) < 0 && errno != EEXIST) fail(errorText("Create Trash directory"));
    auto fd = safeOpen(parent, name, O_RDONLY | O_DIRECTORY, false);
    if (!fd) fail(errorText("Open Trash directory"));
    struct stat st {};
    if (::fstat(fd.get(), &st) < 0 || st.st_uid != ::getuid() || (st.st_mode & 0077) != 0)
        fail("Trash directory must be owned by you and have private permissions");
    return fd;
}

QByteArray percentEncoded(const QByteArray &path) {
    static const char digits[] = "0123456789ABCDEF";
    QByteArray result;
    for (const unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') result += char(c);
        else { result += '%'; result += digits[c >> 4]; result += digits[c & 15]; }
    }
    return result;
}

void trashStaged(int staging, const QByteArray &stagedName, const QByteArray &original) {
    const QByteArray home = qgetenv("HOME");
    QByteArray data = qgetenv("XDG_DATA_HOME");
    if (data.isEmpty()) data = home + "/.local/share";
    if (!data.startsWith('/')) fail("XDG_DATA_HOME must be an absolute path");
    // Existing XDG data directories may be public; the Trash itself must be private.
    Fd dataFd = openAbsolute(data, O_RDONLY | O_DIRECTORY);
    if (!dataFd) fail("Cannot open the XDG data directory safely. Create it before using Trash.");
    auto trash = privateDirectory(dataFd.get(), "Trash", true);
    auto files = privateDirectory(trash.get(), "files", true);
    auto info = privateDirectory(trash.get(), "info", true);
    struct stat source {}, destination {};
    if (::fstatat(staging, stagedName.constData(), &source, AT_SYMLINK_NOFOLLOW) < 0
        || ::fstat(files.get(), &destination) < 0) fail(errorText("Check Trash filesystem"));
    if (source.st_dev != destination.st_dev) fail("Trash is on another filesystem. Nothing was deleted; choose permanent removal explicitly if wanted.");
    const auto entryName = uniqueName("blockyard-");
    const auto infoName = entryName + ".trashinfo";
    Fd metadata(::openat(info.get(), infoName.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (!metadata) fail(errorText("Create Trash metadata"));
    const auto content = QByteArray("[Trash Info]\nPath=") + percentEncoded(original)
        + "\nDeletionDate=" + QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss").toLatin1() + '\n';
    size_t written = 0;
    while (written < static_cast<size_t>(content.size())) {
        const auto amount = ::write(metadata.get(), content.constData() + written, static_cast<size_t>(content.size()) - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) { ::unlinkat(info.get(), infoName.constData(), 0); fail(errorText("Write Trash metadata")); }
        written += static_cast<size_t>(amount);
    }
    if (::fsync(metadata.get()) < 0) { ::unlinkat(info.get(), infoName.constData(), 0); fail(errorText("Save Trash metadata")); }
    if (!renameNoReplace(staging, stagedName, files.get(), entryName)) {
        ::unlinkat(info.get(), infoName.constData(), 0);
        fail(errorText("Move to Trash"));
    }
}
} // namespace

QString displayPath(const QByteArray &native) {
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless | QStringDecoder::Flag::ConvertInitialBom);
    const QString decoded = decoder.decode(native);
    const auto hexEscape = [](char32_t code) {
        const int width = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
        const auto prefix = code <= 0xff ? QStringLiteral("\\x") : code <= 0xffff ? QStringLiteral("\\u") : QStringLiteral("\\U");
        return prefix + QString::number(quint32(code), 16).rightJustified(width, '0').toUpper();
    };
    QString result;
    const auto append = [&](char32_t code) {
        if (code == '\\') result += QStringLiteral("\\\\");
        else if (code == '\n') result += QStringLiteral("\\n");
        else if (code == '\r') result += QStringLiteral("\\r");
        else if (code == '\t') result += QStringLiteral("\\t");
        else {
            const auto category = QChar::category(code);
            if (category == QChar::Other_Control || category == QChar::Other_Format
                || category == QChar::Separator_Line || category == QChar::Separator_Paragraph)
                result += hexEscape(code);
            else result += QString::fromUcs4(&code, 1);
        }
    };
    if (decoder.hasError()) {
        for (const unsigned char byte : native) {
            if (byte >= 0x80) result += hexEscape(byte);
            else append(byte);
        }
    } else for (const char32_t code : decoded.toUcs4()) append(code);
    return result;
}

QByteArray Tree::pathFor(int id) const {
    if (id < 0 || static_cast<size_t>(id) >= nodes.size()) fail("Unknown item");
    std::vector<int> ancestors;
    for (int cursor = id; cursor > 0; cursor = nodes[static_cast<size_t>(cursor)].parent) ancestors.push_back(cursor);
    QByteArray path = rootPath;
    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
        if (!path.endsWith('/')) path += '/';
        path += nodes[static_cast<size_t>(*it)].name;
    }
    return path;
}

std::pair<quint64, quint64> diskSpace(const QByteArray &path) {
    struct statvfs stats {};
    if (::statvfs(path.constData(), &stats) < 0) fail(errorText("Read disk capacity"));
    return {quint64(stats.f_bavail) * stats.f_frsize, quint64(stats.f_blocks) * stats.f_frsize};
}

std::shared_ptr<Tree> scan(const QByteArray &root, std::atomic_bool &cancel,
                          std::function<void(quint64, double)> progress) {
    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(); };
    if (root.isEmpty() || root.contains('\0')) fail("Choose a folder to scan");
    std::unique_ptr<char, decltype(&::free)> canonical(::realpath(root.constData(), nullptr), ::free);
    if (!canonical) fail(errorText("Open scan root"));
    auto tree = std::make_shared<Tree>();
    tree->rootPath = canonical.get();
    Fd rootFd = openAbsolute(tree->rootPath, O_RDONLY | O_DIRECTORY);
    if (!rootFd) fail(errorText("Open scan root"));
    struct stat rootStat {};
    if (::fstat(rootFd.get(), &rootStat) < 0) fail(errorText("Read scan root"));
    struct statfs fs {};
    const bool btrfs = ::fstatfs(rootFd.get(), &fs) == 0 && fs.f_type == BTRFS_SUPER_MAGIC;
    Node rootNode;
    rootNode.name = tree->rootPath == "/" ? QByteArray("/") : tree->rootPath.mid(tree->rootPath.lastIndexOf('/') + 1);
    rootNode.directory = true;
    rootNode.identity = identity(rootStat);
    rootNode.modified = rootStat.st_mtim.tv_sec;
    rootNode.category = categoryFor(rootNode.name, "other", true);
    rootNode.allocated = quint64(rootStat.st_blocks) * 512;
    rootNode.apparent = quint64(rootStat.st_size);
    rootNode.protectedReason = protectedPath(tree->rootPath, rootNode.name);
    if (hasWorktreeAncestor(tree->rootPath)) rootNode.protectedReason = QStringLiteral("Git worktree. Inspect with Git before removing.");
    if (rootNode.protectedReason.isEmpty()) rootNode.protectedReason = QStringLiteral("The scan root cannot be removed.");
    tree->nodes.push_back(std::move(rootNode));
    tree->entries = 1;
    std::vector<int> pending {0};
    std::set<std::pair<quint64, quint64>> hardlinks;
    double lastProgress = -1000;
    while (!pending.empty() && !cancel.load()) {
        const int parentId = pending.back();
        pending.pop_back();
        Fd dir = safeOpen(rootFd.get(), relativePath(*tree, parentId), O_RDONLY | O_DIRECTORY);
        if (!dir) { addIssue(*tree, tree->nodes[static_cast<size_t>(parentId)], errorText("Read folder")); continue; }
        std::vector<QByteArray> names;
        try { names = namesIn(dir.get(), &cancel); }
        catch (const std::exception &e) { addIssue(*tree, tree->nodes[static_cast<size_t>(parentId)], QString::fromUtf8(e.what())); continue; }
        // A .git file is a linked-worktree marker. Do not classify .git directories as disposable.
        struct stat git {};
        if (::fstatat(dir.get(), ".git", &git, AT_SYMLINK_NOFOLLOW) == 0 && !S_ISDIR(git.st_mode))
            tree->nodes[static_cast<size_t>(parentId)].protectedReason = QStringLiteral("Git worktree. Inspect with Git before removing.");
        std::vector<int> childDirectories;
        for (const auto &name : names) {
            if (cancel.load()) break;
            if (tree->nodes.size() >= static_cast<size_t>(std::numeric_limits<int>::max())) fail("This scan has too many entries");
            Node node;
            node.id = static_cast<int>(tree->nodes.size()); node.parent = parentId; node.name = name;
            struct stat st {};
            const bool valid = ::fstatat(dir.get(), name.constData(), &st, AT_SYMLINK_NOFOLLOW) == 0;
            const QString metadataError = valid ? QString() : errorText("Read metadata");
            node.directory = valid && S_ISDIR(st.st_mode);
            node.symlink = valid && S_ISLNK(st.st_mode);
            node.identity = identity(st);
            node.modified = st.st_mtim.tv_sec;
            node.category = categoryFor(name, tree->nodes[static_cast<size_t>(parentId)].category, node.directory);
            node.files = node.directory ? 0 : 1;
            node.apparent = valid ? quint64(st.st_size) : 0;
            node.allocated = valid ? quint64(st.st_blocks) * 512 : 0;
            if (valid && S_ISREG(st.st_mode) && st.st_nlink > 1
                && !hardlinks.emplace(quint64(st.st_dev), quint64(st.st_ino)).second) {
                node.apparent = 0; node.allocated = 0;
            }
            const auto &parentProtection = tree->nodes[static_cast<size_t>(parentId)].protectedReason;
            if (!parentProtection.isEmpty() && parentProtection != "The scan root cannot be removed.")
                node.protectedReason = parentProtection;
            else {
                // The root already checked its ancestors; descendants need only their basename.
                const QByteArray systemPath = parentId == 0 && tree->rootPath == "/" ? '/' + name : QByteArray();
                node.protectedReason = protectedPath(systemPath, name);
            }
            if (valid && !node.directory && !S_ISREG(st.st_mode) && !node.symlink)
                node.protectedReason = QStringLiteral("Special filesystem object. Inspect only.");
            bool descend = node.directory;
            if (valid) {
                Fd boundary = safeOpen(dir.get(), name, O_PATH);
                if (!boundary && errno == EXDEV) {
                    node.protectedReason = QStringLiteral("Mounted filesystem. Scan it separately.");
                    node.allocated = 0; node.apparent = 0; descend = false;
                } else if (node.directory && btrfs && st.st_ino == 256) {
                    node.protectedReason = QStringLiteral("Btrfs subvolume. Scan it separately.");
                    node.allocated = 0; node.apparent = 0; descend = false;
                }
            }
            const int id = node.id;
            tree->nodes.push_back(std::move(node));
            tree->nodes[static_cast<size_t>(parentId)].children.push_back(id);
            ++tree->entries;
            if (!valid) addIssue(*tree, tree->nodes[static_cast<size_t>(id)], metadataError);
            if (descend) childDirectories.push_back(id);
            const auto now = elapsed();
            if (progress && now - lastProgress >= 150) { progress(tree->entries, now); lastProgress = now; }
        }
        for (auto it = childDirectories.rbegin(); it != childDirectories.rend(); ++it) pending.push_back(*it);
    }
    tree->cancelled = cancel.load();
    for (size_t i = tree->nodes.size(); i-- > 1;) {
        const auto &child = tree->nodes[i];
        auto &parent = tree->nodes[static_cast<size_t>(child.parent)];
        parent.apparent += child.apparent; parent.allocated += child.allocated; parent.files += child.files;
        if (!child.error.isEmpty() && parent.error.isEmpty()) parent.error = QStringLiteral("Some descendants could not be scanned.");
        if (!child.protectedReason.isEmpty() && parent.id > 0 && parent.protectedReason.isEmpty())
            parent.protectedReason = QStringLiteral("Contains protected or unscanned data. Select individual children.");
    }
    tree->elapsedMs = elapsed();
    if (progress) progress(tree->entries, tree->elapsedMs);
    return tree;
}

PreparedCleanup prepareCleanup(std::shared_ptr<const Tree> tree, std::vector<int> ids) {
    if (!tree || tree->nodes.empty()) fail("Scan a folder first");
    if (tree->cancelled) fail("Complete the scan before preparing cleanup");
    if (ids.empty()) fail("Select at least one item");
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (const int id : ids) {
        if (id <= 0 || static_cast<size_t>(id) >= tree->nodes.size()) fail("The scan root or an unknown item cannot be removed");
        const auto &node = tree->nodes[static_cast<size_t>(id)];
        if (!node.protectedReason.isEmpty()) fail(displayPath(tree->pathFor(id)) + ": " + node.protectedReason);
        if (!node.error.isEmpty()) fail("Rescan items with scan errors before cleanup");
    }
    const auto selected = std::set<int>(ids.begin(), ids.end());
    std::erase_if(ids, [&](int id) {
        for (int parent = tree->nodes[static_cast<size_t>(id)].parent; parent >= 0; parent = tree->nodes[static_cast<size_t>(parent)].parent)
            if (selected.contains(parent)) return true;
        return false;
    });
    PreparedCleanup result {std::move(tree), std::move(ids), 0,
        {"Selected sizes are not guaranteed freed space. Hard links, snapshots, and running programs can retain data."}};
    for (const int id : result.ids) result.allocated += result.tree->nodes[static_cast<size_t>(id)].allocated;
    return result;
}

std::vector<CleanupOutcome> executeCleanup(const PreparedCleanup &prepared, CleanupAction action,
                                         std::atomic_bool &cancel) {
    std::vector<CleanupOutcome> outcomes;
    ChangedLinks changedLinks;
    if (!prepared.tree) return outcomes;
    const auto &tree = *prepared.tree;
    for (const int id : prepared.ids) {
        CleanupOutcome outcome;
        QByteArray stageName;
        Fd parent;
        Fd stage;
        const auto &node = tree.nodes.at(static_cast<size_t>(id));
        outcome.path = displayPath(tree.pathFor(id));
        bool staged = false;
        try {
            if (id == 0 || !node.protectedReason.isEmpty() || tree.cancelled) fail("This item is protected");
            if (cancel.load()) fail("Cleanup cancelled");
            if (hasWorktreeAncestor(tree.rootPath)) fail("The scan root is now inside a Git worktree. Rescan before cleanup.");
            Fd root = openAbsolute(tree.rootPath, O_RDONLY | O_DIRECTORY);
            if (!root) fail(errorText("Reopen scan root"));
            struct stat rootSt {};
            if (::fstat(root.get(), &rootSt) < 0 || !sameObject(tree.nodes[0].identity, rootSt)) fail("Scan root changed. Rescan before cleanup.");
            rejectWorktree(root.get());
            std::vector<int> ancestors;
            for (int cursor = node.parent; cursor > 0; cursor = tree.nodes[static_cast<size_t>(cursor)].parent)
                ancestors.push_back(cursor);
            parent = std::move(root);
            for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
                const auto &ancestor = tree.nodes[static_cast<size_t>(*it)];
                auto next = safeOpen(parent.get(), ancestor.name, O_RDONLY | O_DIRECTORY);
                if (!next) fail(errorText("Reopen selection parent"));
                struct stat parentSt {};
                if (::fstat(next.get(), &parentSt) < 0 || !sameObject(ancestor.identity, parentSt))
                    fail("Selection ancestor changed. Rescan before cleanup.");
                rejectWorktree(next.get());
                parent = std::move(next);
            }
            validateNode(tree, id, parent.get(), node.name, cancel, false, &changedLinks);
            stageName = uniqueName(".blockyard-cleanup-");
            if (::mkdirat(parent.get(), stageName.constData(), 0700) < 0) fail(errorText("Create private cleanup directory"));
            stage = safeOpen(parent.get(), stageName, O_RDONLY | O_DIRECTORY);
            if (!stage) fail(errorText("Open private cleanup directory"));
            if (!renameNoReplace(parent.get(), node.name, stage.get(), "item")) fail(errorText("Stage selection"));
            staged = true;
            validateNode(tree, id, stage.get(), "item", cancel, true, &changedLinks);
            if (action == CleanupAction::Trash) {
                trashStaged(stage.get(), "item", tree.pathFor(id));
                if (!node.directory) changedLinks.emplace(node.identity.device, node.identity.inode);
            } else removeStaged(tree, id, stage.get(), "item", cancel, changedLinks);
            staged = false;
            outcome.success = true;
            outcome.message = action == CleanupAction::Trash ? "Moved to Trash. Restore it with your file manager."
                                                            : "Permanently removed.";
        } catch (const std::exception &e) {
            outcome.message = QString::fromUtf8(e.what());
            if (staged && stage && parent) {
                if (renameNoReplace(stage.get(), "item", parent.get(), node.name))
                    outcome.message += " Remaining selection restored to its original location.";
                else outcome.message += " Remaining selection preserved in " + displayPath(tree.pathFor(node.parent) + '/' + stageName) + ".";
            }
        }
        if (parent && !stageName.isEmpty() && ::unlinkat(parent.get(), stageName.constData(), AT_REMOVEDIR) < 0)
            outcome.message += " Recovery files remain in " + displayPath(tree.pathFor(node.parent) + '/' + stageName) + ".";
        outcomes.push_back(std::move(outcome));
    }
    return outcomes;
}
} // namespace blockyard
