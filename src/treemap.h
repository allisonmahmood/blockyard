#pragma once
#include <QPointer>
#include <QQuickPaintedItem>
#include <QVariantList>

class Treemap : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(QVariantList model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(int selectedId READ selectedId WRITE setSelectedId NOTIFY
                 selectionChanged)
  Q_PROPERTY(QVariantList markedIds READ markedIds WRITE setMarkedIds NOTIFY
                 selectionChanged)
  Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY
                 appearanceChanged)
  Q_PROPERTY(QObject *theme READ theme WRITE setTheme NOTIFY appearanceChanged)
  Q_PROPERTY(QString hoveredPath READ hoveredPath NOTIFY hoveredChanged)
public:
  explicit Treemap(QQuickItem *parent = nullptr);
  QVariantList model() const { return m_model; }
  void setModel(const QVariantList &value);
  int selectedId() const { return m_selected; }
  void setSelectedId(int value);
  QVariantList markedIds() const { return m_marked; }
  void setMarkedIds(const QVariantList &value);
  QString fontFamily() const { return m_font; }
  void setFontFamily(const QString &value);
  QObject *theme() const { return m_theme; }
  void setTheme(QObject *value);
  QString hoveredPath() const { return m_hoveredPath; }
  void paint(QPainter *painter) override;
signals:
  void modelChanged();
  void selectionChanged();
  void appearanceChanged();
  void hoveredChanged();
  void selected(int id);
  void opened(int id);
  void marked(int id);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void hoverMoveEvent(QHoverEvent *event) override;
  void hoverLeaveEvent(QHoverEvent *event) override;

private:
  struct Hit {
    QRectF rect;
    int id;
    QString path;
  };
  QVariantList m_model, m_marked;
  QList<Hit> m_hits;
  int m_selected = -1, m_hovered = -1;
  QString m_font, m_hoveredPath;
  QPointer<QObject> m_theme;
  QMetaObject::Connection m_themeConnection;
  QColor color(const char *property, const QColor &fallback) const;
  const Hit *hit(QPointF point) const;
  void drawNodes(QPainter *, const QVariantList &, QRectF, int depth);
};
