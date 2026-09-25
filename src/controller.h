#pragma once
#include "engine/engine.h"
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <atomic>
#include <optional>

class Controller final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString rootPath READ rootPath NOTIFY stateChanged)
  Q_PROPERTY(QString currentPath READ currentPath NOTIFY stateChanged)
  Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY stateChanged)
  Q_PROPERTY(QString scanMessage READ scanMessage NOTIFY stateChanged)
  Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(int progressEntries READ progressEntries NOTIFY stateChanged)
  Q_PROPERTY(int issueCount READ issueCount NOTIFY stateChanged)
  Q_PROPERTY(int selectedId READ selectedId NOTIFY stateChanged)
  Q_PROPERTY(int currentId READ currentId NOTIFY stateChanged)
  Q_PROPERTY(int depth READ depth WRITE setDepth NOTIFY stateChanged)
  Q_PROPERTY(double scanElapsed READ scanElapsed NOTIFY stateChanged)
  Q_PROPERTY(double totalBytes READ totalBytes NOTIFY stateChanged)
  Q_PROPERTY(double availableBytes READ availableBytes NOTIFY stateChanged)
  Q_PROPERTY(double scanBytes READ scanBytes NOTIFY stateChanged)
  Q_PROPERTY(QStringList issues READ issues NOTIFY stateChanged)
  Q_PROPERTY(QVariantMap selected READ selected NOTIFY stateChanged)
  Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY stateChanged)
  Q_PROPERTY(QVariantList rows READ rows NOTIFY stateChanged)
  Q_PROPERTY(int rowsTotal READ rowsTotal NOTIFY stateChanged)
  Q_PROPERTY(QVariantList mapCells READ mapCells NOTIFY stateChanged)
  Q_PROPERTY(QVariantList markedIds READ markedIds NOTIFY stateChanged)
  Q_PROPERTY(QVariantList queue READ queue NOTIFY stateChanged)
  Q_PROPERTY(double queueBytes READ queueBytes NOTIFY stateChanged)
  Q_PROPERTY(
      bool lastCleanupSpaceKnown READ lastCleanupSpaceKnown NOTIFY stateChanged)
  Q_PROPERTY(double lastCleanupSpaceDelta READ lastCleanupSpaceDelta NOTIFY
                 stateChanged)
  Q_PROPERTY(QString metric READ metric WRITE setMetric NOTIFY stateChanged)
  Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY stateChanged)
public:
  explicit Controller(QObject *parent = nullptr);
  ~Controller() override;
  QString rootPath() const { return m_rootPath; }
  QString currentPath() const;
  QString scanStatus() const { return m_status; }
  QString scanMessage() const { return m_message; }
  bool scanning() const { return m_status == "scanning"; }
  bool busy() const { return m_busy; }
  int progressEntries() const { return m_entries; }
  int issueCount() const { return m_tree ? m_tree->issueCount : 0; }
  int selectedId() const { return m_selected; }
  int currentId() const { return m_current; }
  int depth() const { return m_depth; }
  void setDepth(int);
  double scanElapsed() const { return m_elapsed; }
  double totalBytes() const { return m_total; }
  double availableBytes() const { return m_available; }
  double scanBytes() const;
  QStringList issues() const { return m_tree ? m_tree->issues : QStringList{}; }
  QVariantMap selected() const { return nodeMap(m_selected); }
  QVariantList breadcrumbs() const;
  QVariantList rows() const { return m_rows; }
  int rowsTotal() const { return m_rowsTotal; }
  QVariantList mapCells() const { return m_mapCells; }
  QVariantList markedIds() const;
  QVariantList queue() const;
  double queueBytes() const;
  bool lastCleanupSpaceKnown() const { return m_cleanupSpaceKnown; }
  double lastCleanupSpaceDelta() const { return m_cleanupDelta; }
  QString metric() const { return m_metric; }
  void setMetric(const QString &);
  QString filter() const { return m_filter; }
  void setFilter(const QString &);
  Q_INVOKABLE void startScan(const QString &);
  Q_INVOKABLE void startScanUrl(const QUrl &url) {
    if (url.isLocalFile())
      startScan(url.toLocalFile());
  }
  Q_INVOKABLE void cancelScan();
  Q_INVOKABLE void rescan();
  Q_INVOKABLE void navigate(int);
  Q_INVOKABLE void selectNode(int);
  Q_INVOKABLE void goUp();
  Q_INVOKABLE void selectAdjacent(int);
  Q_INVOKABLE void toggleMark(int);
  Q_INVOKABLE void clearQueue();
  Q_INVOKABLE QVariantMap prepareReview();
  Q_INVOKABLE void executeReview(const QString &);
  Q_INVOKABLE void openSelected();
  Q_INVOKABLE void openTrash();
  Q_INVOKABLE void copySelectedPath();
  Q_INVOKABLE void chooseFolder();
  Q_INVOKABLE void loadMoreRows();
  Q_INVOKABLE QString formatBytes(double) const;
signals:
  void stateChanged();
  void cleanupFinished(QVariantList outcomes);
  void errorOccurred(QString message);
  void folderRequested();

private:
  struct ScanResult {
    std::shared_ptr<blockyard::Tree> tree;
    QString error;
  };
  struct CleanupResult {
    std::vector<blockyard::CleanupOutcome> outcomes;
    QString error;
    double before = 0, after = 0;
    bool measured = false;
    std::vector<QByteArray> failedPaths;
  };
  std::shared_ptr<blockyard::Tree> m_tree;
  std::atomic_bool m_cancel{false};
  QFutureWatcher<ScanResult> m_scanWatcher;
  QFutureWatcher<CleanupResult> m_cleanupWatcher;
  std::optional<blockyard::PreparedCleanup> m_prepared;
  QString m_rootPath, m_status = "idle", m_message, m_metric = "allocated",
                      m_filter, m_pendingPath;
  int m_selected = -1, m_current = -1, m_depth = 3, m_entries = 0,
      m_rowsTotal = 0, m_rowsLimit = 200;
  double m_elapsed = 0, m_total = 0, m_available = 0, m_cleanupDelta = 0;
  bool m_busy = false, m_cleanupSpaceKnown = false;
  QVariantList m_rows, m_mapCells;
  std::vector<int> m_marks, m_sortedRows;
  std::vector<QByteArray> m_failedPaths;
  QTimer m_filterTimer;
  const blockyard::Node *node(int) const;
  double weight(const blockyard::Node &) const;
  QVariantMap nodeMap(int) const;
  void rebuildView();
  void rebuildRows(bool reset = true);
  QVariantList buildMap(const std::vector<int> &, int, int &budget) const;
  bool ancestorOf(int, int) const;
  void refreshDisk();
};
