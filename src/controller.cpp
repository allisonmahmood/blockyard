#include "controller.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>
#include <QtConcurrentRun>
#include <algorithm>
#include <limits>

Controller::Controller(QObject *parent) : QObject(parent) {
  m_filterTimer.setSingleShot(true);
  m_filterTimer.setInterval(150);
  connect(&m_filterTimer, &QTimer::timeout, this, [this] {
    rebuildRows();
    emit stateChanged();
  });
  connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished, this, [this] {
    auto result = m_scanWatcher.result();
    if (!m_pendingPath.isEmpty()) {
      const auto next = m_pendingPath;
      m_pendingPath.clear();
      m_status = "idle";
      startScan(next);
      return;
    }
    if (!result.error.isEmpty()) {
      m_status = "failed";
      m_message = result.error;
      emit stateChanged();
      return;
    }
    m_tree = std::move(result.tree);
    m_current = m_selected = 0;
    m_entries = static_cast<int>(std::min<quint64>(m_tree->entries, INT_MAX));
    m_elapsed = m_tree->elapsedMs;
    m_status = m_tree->cancelled ? "cancelled" : "complete";
    m_message = m_tree->cancelled
                    ? "Scan stopped. These results are incomplete."
                    : QString();
    m_rootPath = QFile::decodeName(m_tree->rootPath);
    for (const auto &path : m_failedPaths) {
      if (!path.startsWith(m_tree->rootPath + '/'))
        continue;
      int id = 0;
      const auto components = path.mid(m_tree->rootPath.size() + 1).split('/');
      for (const auto &component : components) {
        int next = -1;
        for (int child : node(id)->children)
          if (node(child)->name == component) {
            next = child;
            break;
          }
        if (next < 0) {
          id = -1;
          break;
        }
        id = next;
      }
      if (id > 0)
        m_marks.push_back(id);
    }
    m_failedPaths.clear();
    refreshDisk();
    rebuildView();
    emit stateChanged();
  });
  connect(&m_cleanupWatcher, &QFutureWatcher<CleanupResult>::finished, this,
          [this] {
            const auto result = m_cleanupWatcher.result();
            m_busy = false;
            m_cleanupSpaceKnown = result.measured;
            m_cleanupDelta = result.measured ? result.after - result.before : 0;
            m_failedPaths = result.failedPaths;
            QVariantList outcomes;
            for (const auto &o : result.outcomes)
              outcomes.append(QVariantMap{{"path", o.path},
                                          {"success", o.success},
                                          {"message", o.message}});
            if (!result.error.isEmpty())
              outcomes.append(QVariantMap{{"path", m_rootPath},
                                          {"success", false},
                                          {"message", result.error}});
            m_marks.clear();
            m_prepared.reset();
            emit cleanupFinished(outcomes);
            rescan();
            emit stateChanged();
          });
}
Controller::~Controller() {
  m_cancel = true;
  m_scanWatcher.waitForFinished();
  m_cleanupWatcher.waitForFinished();
}
const blockyard::Node *Controller::node(int id) const {
  return m_tree && id >= 0 && static_cast<size_t>(id) < m_tree->nodes.size()
             ? &m_tree->nodes[id]
             : nullptr;
}
double Controller::weight(const blockyard::Node &n) const {
  return m_metric == "files"      ? static_cast<double>(n.files)
         : m_metric == "apparent" ? static_cast<double>(n.apparent)
                                  : static_cast<double>(n.allocated);
}
QString Controller::currentPath() const {
  return node(m_current) ? blockyard::displayPath(m_tree->pathFor(m_current))
                         : m_rootPath;
}
double Controller::scanBytes() const { return node(0) ? weight(*node(0)) : 0; }
QString Controller::formatBytes(double n) const {
  static const char *units[]{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
  int i = 0;
  while (n >= 1024 && i < 5) {
    n /= 1024;
    ++i;
  }
  return QString::number(n, 'f', i && n < 10 ? 1 : 0) + " " + units[i];
}
QVariantMap Controller::nodeMap(int id) const {
  auto n = node(id);
  if (!n)
    return {};
  return {{"id", id},
          {"parentId", n->parent},
          {"name", blockyard::displayPath(n->name)},
          {"path", blockyard::displayPath(m_tree->pathFor(id))},
          {"directory", n->directory},
          {"symlink", n->symlink},
          {"allocated", static_cast<double>(n->allocated)},
          {"apparent", static_cast<double>(n->apparent)},
          {"files", static_cast<double>(n->files)},
          {"modified", static_cast<double>(n->modified)},
          {"category", n->category},
          {"error", n->error},
          {"protectedReason", n->protectedReason},
          {"childCount", static_cast<int>(n->children.size())}};
}
QVariantList Controller::breadcrumbs() const {
  QVariantList r;
  for (int id = m_current; node(id); id = node(id)->parent)
    r.prepend(nodeMap(id));
  return r;
}
QVariantList Controller::markedIds() const {
  QVariantList r;
  for (int id : m_marks)
    r.append(id);
  return r;
}
QVariantList Controller::queue() const {
  QVariantList r;
  for (int id : m_marks)
    r.append(nodeMap(id));
  return r;
}
double Controller::queueBytes() const {
  double total = 0;
  for (int id : m_marks)
    if (auto n = node(id))
      total += n->allocated;
  return total;
}
bool Controller::ancestorOf(int parent, int child) const {
  while (node(child)) {
    if (parent == child)
      return true;
    child = node(child)->parent;
  }
  return false;
}
void Controller::startScan(const QString &path) {
  if (m_busy) {
    emit errorOccurred("Wait for cleanup to finish before scanning.");
    return;
  }
  if (path.trimmed().isEmpty())
    return;
  if (scanning()) {
    m_pendingPath = path;
    m_cancel = true;
    return;
  }
  m_cancel = false;
  m_status = "scanning";
  m_message.clear();
  m_rootPath = path;
  m_entries = 0;
  m_elapsed = 0;
  m_tree.reset();
  m_prepared.reset();
  m_marks.clear();
  m_mapCells.clear();
  m_rows.clear();
  m_rowsTotal = 0;
  m_current = m_selected = -1;
  m_filter.clear();
  emit stateChanged();
  const auto native =
      QFile::encodeName(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
  m_scanWatcher.setFuture(QtConcurrent::run([this, native] {
    ScanResult r;
    try {
      r.tree = blockyard::scan(
          native, m_cancel, [this](quint64 entries, double elapsed) {
            QMetaObject::invokeMethod(
                this,
                [this, entries, elapsed] {
                  m_entries =
                      static_cast<int>(std::min<quint64>(entries, INT_MAX));
                  m_elapsed = elapsed;
                  emit stateChanged();
                },
                Qt::QueuedConnection);
          });
    } catch (const std::exception &e) {
      r.error = QString::fromUtf8(e.what());
    }
    return r;
  }));
}
void Controller::cancelScan() {
  if (scanning()) {
    m_pendingPath.clear();
    m_cancel = true;
    m_message = "Stopping scan…";
    emit stateChanged();
  }
}
void Controller::rescan() {
  if (!m_rootPath.isEmpty())
    startScan(m_rootPath);
}
void Controller::refreshDisk() {
  if (!m_tree)
    return;
  try {
    auto [available, total] = blockyard::diskSpace(m_tree->rootPath);
    m_available = available;
    m_total = total;
  } catch (const std::exception &) {
    m_available = m_total = 0;
  }
}
void Controller::navigate(int id) {
  auto n = node(id);
  if (!n || !n->directory || m_busy)
    return;
  m_current = m_selected = id;
  m_filter.clear();
  m_rowsLimit = 200;
  rebuildView();
  emit stateChanged();
}
void Controller::selectNode(int id) {
  if (node(id)) {
    m_selected = id;
    emit stateChanged();
  }
}
void Controller::goUp() {
  if (auto n = node(m_current); n && n->parent >= 0)
    navigate(n->parent);
}
void Controller::selectAdjacent(int delta) {
  if (m_rows.empty())
    return;
  int index = -1;
  for (int i = 0; i < m_rows.size(); ++i)
    if (m_rows[i].toMap().value("id").toInt() == m_selected) {
      index = i;
      break;
    }
  index = std::clamp(index + delta, 0, static_cast<int>(m_rows.size()) - 1);
  selectNode(m_rows[index].toMap().value("id").toInt());
}
void Controller::setDepth(int v) {
  v = std::clamp(v, 1, 6);
  if (v == m_depth)
    return;
  m_depth = v;
  int budget = 1500;
  m_mapCells = node(m_current)
                   ? buildMap(node(m_current)->children, m_depth, budget)
                   : QVariantList{};
  emit stateChanged();
}
void Controller::setMetric(const QString &v) {
  if ((v != "allocated" && v != "apparent" && v != "files") || v == m_metric)
    return;
  m_metric = v;
  rebuildView();
  emit stateChanged();
}
void Controller::setFilter(const QString &v) {
  if (v == m_filter)
    return;
  m_filter = v;
  m_rowsLimit = 200;
  m_filterTimer.start();
  emit stateChanged();
}
void Controller::loadMoreRows() {
  m_rowsLimit += 200;
  rebuildRows(false);
  emit stateChanged();
}
void Controller::rebuildRows(bool reset) {
  m_rows.clear();
  if (reset) {
    m_sortedRows.clear();
    if (auto n = node(m_current)) {
      if (m_filter.isEmpty())
        m_sortedRows = n->children;
      else
        for (const auto &entry : m_tree->nodes)
          if (blockyard::displayPath(entry.name)
                  .contains(m_filter, Qt::CaseInsensitive))
            m_sortedRows.push_back(entry.id);
    }
  }
  m_rowsTotal = static_cast<int>(m_sortedRows.size());
  const int visible = std::min(m_rowsLimit, m_rowsTotal);
  std::partial_sort(m_sortedRows.begin(), m_sortedRows.begin() + visible,
                    m_sortedRows.end(), [this](int a, int b) {
                      const auto va = weight(*node(a)), vb = weight(*node(b));
                      return va == vb ? node(a)->name < node(b)->name : va > vb;
                    });
  for (int i = 0; i < visible; ++i)
    m_rows.append(nodeMap(m_sortedRows[i]));
}
QVariantList Controller::buildMap(const std::vector<int> &children,
                                  int depthLeft, int &budget) const {
  if (children.empty())
    return {};
  std::vector<int> ids = children;
  const auto visible = std::min({static_cast<int>(ids.size()), budget, 160});
  std::partial_sort(ids.begin(), ids.begin() + visible, ids.end(),
                    [this](int a, int b) {
                      double va = weight(*node(a)), vb = weight(*node(b));
                      return va == vb ? a < b : va > vb;
                    });
  // Reserve sibling labels before allowing a large first subtree to consume the
  // budget.
  budget -= visible;
  QVariantList result;
  double omitted = 0, omittedFiles = 0;
  int omittedCount = 0;
  for (size_t index = 0; index < ids.size(); ++index) {
    const int id = ids[index];
    const auto &n = *node(id);
    const double value = weight(n);
    if (value <= 0)
      continue;
    if (index >= static_cast<size_t>(visible)) {
      omitted += value;
      omittedFiles += n.files;
      ++omittedCount;
      continue;
    }
    auto map = nodeMap(id);
    map.insert("value", value);
    map.insert("metric", m_metric);
    if (depthLeft > 1 && budget > 0 && !n.children.empty())
      map.insert("children", buildMap(n.children, depthLeft - 1, budget));
    result.append(map);
  }
  if (omittedCount)
    result.append(QVariantMap{
        {"id", -1},
        {"name", QString("%1 other items · see folder list").arg(omittedCount)},
        {"value", omitted},
        {"category", "other"},
        {"metric", m_metric},
        {"files", omittedFiles}});
  return result;
}
void Controller::rebuildView() {
  rebuildRows();
  m_mapCells.clear();
  if (auto n = node(m_current)) {
    int budget = 1500;
    m_mapCells = buildMap(n->children, m_depth, budget);
  }
}
void Controller::toggleMark(int id) {
  const auto n = node(id);
  if (!n || m_busy || scanning())
    return;
  if (auto i = std::find(m_marks.begin(), m_marks.end(), id);
      i != m_marks.end()) {
    m_marks.erase(i);
    m_prepared.reset();
    emit stateChanged();
    return;
  }
  if (!n->protectedReason.isEmpty() || !n->error.isEmpty()) {
    emit errorOccurred(!n->protectedReason.isEmpty() ? n->protectedReason
                                                     : n->error);
    return;
  }
  for (int marked : m_marks)
    if (ancestorOf(marked, id)) {
      emit errorOccurred(
          "This item is already included by its marked parent folder.");
      return;
    }
  std::erase_if(m_marks,
                [this, id](int child) { return ancestorOf(id, child); });
  m_marks.push_back(id);
  m_prepared.reset();
  emit stateChanged();
}
void Controller::clearQueue() {
  if (m_busy)
    return;
  m_marks.clear();
  m_prepared.reset();
  emit stateChanged();
}
QVariantMap Controller::prepareReview() {
  if (m_busy || scanning())
    return {{"error", "Wait for the current operation to finish."},
            {"items", QVariantList{}},
            {"total", 0},
            {"warnings", QStringList{}}};
  try {
    m_prepared = blockyard::prepareCleanup(m_tree, m_marks);
    QVariantList items;
    for (int id : m_prepared->ids)
      items.append(nodeMap(id));
    return {{"items", items},
            {"total", static_cast<double>(m_prepared->allocated)},
            {"warnings", m_prepared->warnings},
            {"error", ""}};
  } catch (const std::exception &e) {
    m_prepared.reset();
    return {{"error", QString::fromUtf8(e.what())},
            {"items", QVariantList{}},
            {"total", 0},
            {"warnings", QStringList{}}};
  }
}
void Controller::executeReview(const QString &action) {
  if (m_busy || scanning() || !m_prepared ||
      (action != "trash" && action != "permanent"))
    return;
  auto prepared = std::move(*m_prepared);
  m_prepared.reset();
  m_busy = true;
  m_cancel = false;
  emit stateChanged();
  m_cleanupWatcher.setFuture(
      QtConcurrent::run([this, prepared = std::move(prepared), action] {
        CleanupResult r;
        bool beforeKnown = false;
        try {
          r.before = blockyard::diskSpace(prepared.tree->rootPath).first;
          beforeKnown = true;
        } catch (const std::exception &) {
        }
        try {
          r.outcomes = blockyard::executeCleanup(
              prepared,
              action == "trash" ? blockyard::CleanupAction::Trash
                                : blockyard::CleanupAction::Permanent,
              m_cancel);
        } catch (const std::exception &e) {
          r.error = QString::fromUtf8(e.what());
        }
        try {
          r.after = blockyard::diskSpace(prepared.tree->rootPath).first;
          r.measured = beforeKnown;
        } catch (const std::exception &) {
        }
        for (size_t i = 0; i < prepared.ids.size(); ++i)
          if (i >= r.outcomes.size() || !r.outcomes[i].success)
            r.failedPaths.push_back(prepared.tree->pathFor(prepared.ids[i]));
        return r;
      }));
}
void Controller::openSelected() {
  if (auto n = node(m_selected)) {
    auto path = m_tree->pathFor(n->directory ? n->id : n->parent);
    if (!QDesktopServices::openUrl(
            QUrl::fromEncoded("file://" + path.toPercentEncoding("/"))))
      emit errorOccurred("Could not open the folder in your file manager.");
  }
}
void Controller::openTrash() {
  if (!QDesktopServices::openUrl(QUrl("trash:///")))
    emit errorOccurred("Could not open the system Trash.");
}
void Controller::copySelectedPath() {
  if (node(m_selected))
    QGuiApplication::clipboard()->setText(
        QFile::decodeName(m_tree->pathFor(m_selected)));
}
void Controller::chooseFolder() { emit folderRequested(); }
