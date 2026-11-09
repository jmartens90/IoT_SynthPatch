#include "ScopeView.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <QVariantMap>
#include <climits>
#include <cmath>

#include "Device.h"

namespace {

using Pt = QSGGeometry::Point2D;

// A line piece p0 -> p1 as two triangles, w wide, with square ends (so joints have no notches).
void addLine(QList<Pt> &out, double x0, double y0, double x1, double y1, double w) {
  double dx = x1 - x0, dy = y1 - y0;
  const double len = std::hypot(dx, dy);
  if (len < 1e-6) {
    dx = 1;
    dy = 0;
  } else {
    dx /= len;
    dy /= len;
  }
  const double h = w / 2;
  const double nx = -dy * h, ny = dx * h, ex = dx * h, ey = dy * h;
  const Pt a{float(x0 - ex + nx), float(y0 - ey + ny)}, b{float(x0 - ex - nx), float(y0 - ey - ny)};
  const Pt c{float(x1 + ex + nx), float(y1 + ey + ny)}, d{float(x1 + ex - nx), float(y1 + ey - ny)};
  out << a << b << c << c << b << d;
}

void addRect(QList<Pt> &out, double x, double y, double w, double h) {
  const Pt a{float(x), float(y)}, b{float(x + w), float(y)}, c{float(x), float(y + h)}, d{float(x + w), float(y + h)};
  out << a << b << c << c << b << d;
}

// One child node: triangles in one colour.
QSGGeometryNode *makeNode(const QColor &color) {
  auto *node = new QSGGeometryNode;
  auto *geo = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
  geo->setDrawingMode(QSGGeometry::DrawTriangles);
  node->setGeometry(geo);
  node->setFlag(QSGNode::OwnsGeometry);
  auto *mat = new QSGFlatColorMaterial;
  mat->setColor(color);
  node->setMaterial(mat);
  node->setFlag(QSGNode::OwnsMaterial);
  return node;
}

void setPoints(QSGNode *n, const QList<Pt> &pts) {
  auto *node = static_cast<QSGGeometryNode *>(n);
  QSGGeometry *geo = node->geometry();
  geo->allocate(int(pts.size()));
  if (!pts.isEmpty()) memcpy(geo->vertexDataAsPoint2D(), pts.constData(), size_t(pts.size()) * sizeof(Pt));
  node->markDirty(QSGNode::DirtyGeometry);
}

}  // namespace

ScopeView::ScopeView(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

QObject *ScopeView::source() const { return dev_; }

void ScopeView::setSource(QObject *d) {
  Device *dev = qobject_cast<Device *>(d);
  if (dev == dev_) return;
  if (dev_) disconnect(dev_, nullptr, this, nullptr);
  dev_ = dev;
  if (dev_) connect(dev_, &Device::cvChanged, this, &ScopeView::onData);
  emit sourceChanged();
  onData();
}

void ScopeView::setHead(double h) {
  if (h == head_) return;
  head_ = h;
  emit headChanged();
  update();  // only the translation changes: cheap
}

void ScopeView::setSpan(int s) {
  if (s == span_ || s < 2) return;
  span_ = s;
  emit spanChanged();
  onData();
}

void ScopeView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  if (newGeometry.size() != oldGeometry.size()) onData();  // the lanes moved: build again
}

// Lanes: LFO the top quarter, envelope the second quarter, V/oct the bottom half.
void ScopeView::laneY(int lane, double &top, double &h) const {
  const double H = height();
  if (lane == 0) top = 0, h = H * .25;
  else if (lane == 1) top = H * .25, h = H * .25;
  else top = H * .5, h = H * .5;
}

// New data (or a new size): work out the piano-roll segments and labels, then rebuild.
void ScopeView::onData() {
  segs_.clear();
  QVariantList labels;
  if (dev_) {
    const QList<double> &voct = dev_->cvSeries(2), &env = dev_->cvSeries(1);
    base_ = qint64(dev_->cvEnd()) - voct.size();
    auto noteAt = [&](int i) { return std::isnan(voct[i]) ? INT_MIN : int(std::lround(voct[i] * 12)) + 36; };
    auto onAt = [&](int i) { return i < env.size() && env[i] > 0.02; };  // NaN > x is false
    nlo_ = INT_MAX;
    nhi_ = INT_MIN;
    for (int i = 0; i < voct.size();) {
      const int n = noteAt(i);
      const bool on = onAt(i);
      int j = i;
      while (j < voct.size() && noteAt(j) == n && onAt(j) == on) j++;
      if (n != INT_MIN) {
        segs_.append({i, j, n, on});
        nlo_ = qMin(nlo_, n);
        nhi_ = qMax(nhi_, n);
      }
      i = j;
    }
    nlo_ -= 1;
    nhi_ += 1;
    double top, lane;
    laneY(2, top, lane);
    const double rh = (lane - 16) / (nhi_ - nlo_ + 1), bh = qMax(3.0, rh - 2);
    for (const Segment &s : segs_) {
      if (!s.on || bh <= 8) continue;
      labels.append(QVariantMap{{"k0", double(base_ + s.i0)},
                                {"k1", double(base_ + s.i1)},
                                {"y", top + 14 + (nhi_ - s.note) * rh},
                                {"h", bh},
                                {"text", dev_->noteName(s.note)}});
    }
  }
  if (labels != labels_) {
    labels_ = labels;
    emit labelsChanged();
  }
  geomDirty_ = true;
  update();
}

QSGNode *ScopeView::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
  auto *root = static_cast<QSGTransformNode *>(old);
  if (!root) {
    root = new QSGTransformNode;
    root->appendChildNode(makeNode(lfoColor_));
    root->appendChildNode(makeNode(envColor_));
    root->appendChildNode(makeNode(voctColor_));
    QColor hold = voctColor_;
    hold.setAlphaF(0.25f);
    root->appendChildNode(makeNode(hold));
    geomDirty_ = true;
  }
  const double p = pps();

  if (geomDirty_ && dev_) {
    geomDirty_ = false;
    // Points are placed at x = index * pps; the translation below slides them into view.
    for (int ch = 0; ch < 2; ch++) {
      const QList<double> &v = dev_->cvSeries(ch);
      double top, lane;
      laneY(ch, top, lane);
      auto Y = [&](double volts) { return top + lane - 4 - volts / 3.3 * (lane - 8); };
      QList<Pt> pts;
      pts.reserve(v.size() * 6);
      for (int i = 0; i + 1 < v.size(); i++) {
        if (std::isnan(v[i]) || std::isnan(v[i + 1])) continue;  // a gap: no line across it
        addLine(pts, i * p, Y(v[i]), (i + 1) * p, Y(v[i + 1]), lineWidth_);
      }
      setPoints(root->childAtIndex(ch), pts);
    }
    // V/oct piano roll: a block while the envelope is open, a faint line while V/oct only holds
    QList<Pt> blocks, holds;
    if (!segs_.isEmpty()) {
      double top, lane;
      laneY(2, top, lane);
      const double rh = (lane - 16) / (nhi_ - nlo_ + 1), h = qMax(3.0, rh - 2);
      for (const Segment &s : segs_) {
        const double x0 = s.i0 * p, x1 = s.i1 * p, y = top + 14 + (nhi_ - s.note) * rh;
        if (s.on) addRect(blocks, x0 + 1, y, qMax(2.0, x1 - x0 - 2), h);
        else addRect(holds, x0, y + h / 2 - 1, x1 - x0, 2);
      }
    }
    setPoints(root->childAtIndex(2), blocks);
    setPoints(root->childAtIndex(3), holds);
  }

  QMatrix4x4 m;
  m.translate(float(width() - (head_ - base_) * p), 0);
  root->setMatrix(m);
  return root;
}
