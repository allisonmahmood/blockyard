#include "treemap.h"
#include "desktop/theme.h"
#include <QFontMetricsF>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
QColor blend(QColor a, QColor b, double amount) {
  return QColor::fromRgbF(a.redF() * (1 - amount) + b.redF() * amount,
                          a.greenF() * (1 - amount) + b.greenF() * amount,
                          a.blueF() * (1 - amount) + b.blueF() * amount);
}
QString bytes(double value) {
  static const char *units[]{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
  int unit = 0;
  while (value >= 1024 && unit < 5) {
    value /= 1024;
    ++unit;
  }
  return QString::number(value, 'f', unit > 0 && value < 10 ? 1 : 0) + " " +
         units[unit];
}
// Squarify a bounded visible sibling set. Layout work happens only on repaint.
QList<QRectF> tiles(const QList<double> &values, QRectF remaining) {
  QList<QRectF> result;
  double total = std::accumulate(values.begin(), values.end(), 0.0);
  if (total <= 0 || remaining.width() <= 0 || remaining.height() <= 0)
    return result;
  QList<double> areas;
  for (double value : values)
    areas.append(value / total * remaining.width() * remaining.height());
  int cursor = 0;
  auto worst = [](double sum, double low, double high, double side) {
    return std::max(side * side * high / (sum * sum),
                    sum * sum / (side * side * low));
  };
  while (cursor < areas.size()) {
    int end = cursor + 1;
    double sum = areas[cursor], low = sum, high = sum;
    const double side =
        std::max(0.001, std::min(remaining.width(), remaining.height()));
    while (end < areas.size()) {
      double next = areas[end];
      if (worst(sum + next, std::min(low, next), std::max(high, next), side) >
          worst(sum, low, high, side))
        break;
      sum += next;
      low = std::min(low, next);
      high = std::max(high, next);
      ++end;
    }
    if (remaining.width() >= remaining.height()) {
      const double width = sum / std::max(remaining.height(), 0.001);
      double y = remaining.y();
      for (int i = cursor; i < end; ++i) {
        double height = areas[i] / std::max(width, 0.001);
        result.append(QRectF(remaining.x(), y, width, height));
        y += height;
      }
      remaining.setLeft(remaining.x() + width);
    } else {
      const double height = sum / std::max(remaining.width(), 0.001);
      double x = remaining.x();
      for (int i = cursor; i < end; ++i) {
        double width = areas[i] / std::max(height, 0.001);
        result.append(QRectF(x, remaining.y(), width, height));
        x += width;
      }
      remaining.setTop(remaining.y() + height);
    }
    cursor = end;
  }
  return result;
}
} // namespace
Treemap::Treemap(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  setAcceptHoverEvents(true);
  setAntialiasing(false);
}
void Treemap::setModel(const QVariantList &v) {
  if (m_model == v)
    return;
  m_model = v;
  m_hits.clear();
  m_hovered = -1;
  m_hoveredPath.clear();
  emit hoveredChanged();
  emit modelChanged();
  update();
}
void Treemap::setSelectedId(int v) {
  if (m_selected == v)
    return;
  m_selected = v;
  emit selectionChanged();
  update();
}
void Treemap::setMarkedIds(const QVariantList &v) {
  if (m_marked == v)
    return;
  m_marked = v;
  emit selectionChanged();
  update();
}
void Treemap::setFontFamily(const QString &v) {
  if (m_font == v)
    return;
  m_font = v;
  emit appearanceChanged();
  update();
}
void Treemap::setTheme(QObject *v) {
  if (m_theme == v)
    return;
  disconnect(m_themeConnection);
  m_theme = v;
  if (auto t = qobject_cast<Theme *>(v))
    m_themeConnection = connect(t, &Theme::changed, this, [this] { update(); });
  emit appearanceChanged();
  update();
}
QColor Treemap::color(const char *name, const QColor &fallback) const {
  if (!m_theme)
    return fallback;
  const QColor c = m_theme->property(name).value<QColor>();
  return c.isValid() ? c : fallback;
}
const Treemap::Hit *Treemap::hit(QPointF p) const {
  for (auto i = m_hits.crbegin(); i != m_hits.crend(); ++i)
    if (i->rect.contains(p))
      return &*i;
  return nullptr;
}
void Treemap::paint(QPainter *p) {
  m_hits.clear();
  p->fillRect(boundingRect(), color("background", Qt::black));
  drawNodes(p, m_model, boundingRect(), 0);
}
void Treemap::drawNodes(QPainter *p, const QVariantList &nodes, QRectF bounds,
                        int depth) {
  if (bounds.width() < 2 || bounds.height() < 2 || depth > 8)
    return;
  QList<double> values;
  for (const auto &n : nodes)
    values.append(std::max(0.000001, n.toMap().value("value").toDouble()));
  const auto rects = tiles(values, bounds);
  const QColor bg = color("background", Qt::black),
               fg = color("foreground", Qt::white);
  const bool light = bg.lightnessF() > 0.5;
  QFont font(m_font);
  font.setPixelSize(12);
  p->setFont(font);
  for (int i = 0; i < rects.size(); ++i) {
    auto data = nodes[i].toMap();
    int id = data.value("id").toInt();
    QRectF r = rects[i].adjusted(2, 2, -2, -2);
    if (r.width() < 1 || r.height() < 1)
      continue;
    const auto category = data.value("category").toString();
    const char *key = category == "code"        ? "blue"
                      : category == "scratch"   ? "orange"
                      : category == "media"     ? "cyan"
                      : category == "cache"     ? "yellow"
                      : category == "tools"     ? "green"
                      : category == "documents" ? "magenta"
                                                : "accent";
    QColor accent = color(key, QColor("#6b99ba"));
    QColor fill =
        blend(bg, accent, light ? 0.15 + depth * 0.025 : 0.23 + depth * 0.055);
    if (id == m_hovered)
      fill = blend(fill, fg, 0.035);
    p->fillRect(r, fill);
    p->fillRect(QRectF(r.x(), r.y(), r.width(), 2), accent);
    if (id >= 0)
      m_hits.append({r, id, data.value("path").toString()});
    const auto children = data.value("children").toList();
    p->save();
    p->setClipRect(r.adjusted(3, 2, -3, -2));
    p->setPen(fg);
    QFontMetricsF fm(font);
    QString name = data.value("name").toString();
    QString size =
        data.value("metric").toString() == "files"
            ? QString::number(data.value("files").toULongLong()) + " files"
            : bytes(data.value("value").toDouble());
    double labelWidth = r.width() - 14;
    if (r.width() > 180) {
      p->drawText(r.adjusted(6, 5, -7, -5), Qt::AlignTop | Qt::AlignRight,
                  size);
      labelWidth -= fm.horizontalAdvance(size) + 12;
    }
    if (r.width() > 30 && r.height() > 20)
      p->drawText(QPointF(r.x() + 7, r.y() + 18),
                  fm.elidedText(name, Qt::ElideRight, labelWidth));
    if (children.empty() && r.height() > 55 && r.width() > 65) {
      p->setPen(blend(fg, fill, 0.15));
      QFont large = font;
      large.setPixelSize(r.width() > 130 && r.height() > 110 ? 22 : 12);
      p->setFont(large);
      p->drawText(r.adjusted(7, 27, -7, -8), Qt::AlignLeft | Qt::AlignBottom,
                  size);
      p->setFont(font);
    }
    p->restore();
    if (!children.empty() && r.width() > 65 && r.height() > 62)
      drawNodes(p, children, r.adjusted(3, 25, -3, -3), depth + 1);
    if (m_marked.contains(id)) {
      p->setPen(QPen(accent, 2));
      p->drawRect(r.adjusted(3, 3, -3, -3));
      p->setPen(fg);
      p->drawText(r.adjusted(0, 0, -8, -7), Qt::AlignRight | Qt::AlignBottom,
                  QString::fromUtf8("✓"));
    }
    if (id == m_selected) {
      p->setPen(QPen(fg, 2));
      p->setBrush(Qt::NoBrush);
      p->drawRect(r.adjusted(1, 1, -1, -1));
    }
  }
}
void Treemap::mousePressEvent(QMouseEvent *e) {
  if (const auto *h = hit(e->position())) {
    if (e->button() == Qt::RightButton)
      emit marked(h->id);
    else
      emit selected(h->id);
    e->accept();
  } else
    e->ignore();
}
void Treemap::mouseDoubleClickEvent(QMouseEvent *e) {
  if (const auto *h = hit(e->position())) {
    emit opened(h->id);
    e->accept();
  }
}
void Treemap::hoverMoveEvent(QHoverEvent *e) {
  auto h = hit(e->position());
  int id = h ? h->id : -1;
  if (id != m_hovered) {
    m_hovered = id;
    m_hoveredPath = h ? h->path : QString();
    emit hoveredChanged();
    update();
  }
}
void Treemap::hoverLeaveEvent(QHoverEvent *) {
  m_hovered = -1;
  m_hoveredPath.clear();
  emit hoveredChanged();
  update();
}
